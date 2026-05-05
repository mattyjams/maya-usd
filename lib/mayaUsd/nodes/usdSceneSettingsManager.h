//
// Copyright 2026 Autodesk
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#ifndef MAYAUSD_USD_SCENE_SETTINGS_MANAGER_H
#define MAYAUSD_USD_SCENE_SETTINGS_MANAGER_H

#include <mayaUsd/base/api.h>
#include <mayaUsd/nodes/usdSettingsNode.h>

#include <pxr/pxr.h>
#include <pxr/usd/usd/stage.h>

#include <maya/MMessage.h>
#include <maya/MObject.h>
#include <maya/MObjectHandle.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace MAYAUSD_NS_DEF {

/*! \brief Plugin-side registry that owns the lifecycle of all UsdSettingsNode instances.
 *
 *  UsdSceneSettingsManager keys every managed instance by its Maya DG node name
 *  (e.g. "UsdDefaultRenderSettings"). Each registered node name is paired with a
 *  populator function that seeds the node's USD stage with domain-specific content.
 *
 *  For each registered node name the manager guarantees that exactly one non-referenced
 *  UsdSettingsNode (of type UsdDefaultSettings) exists in the scene at all times while the
 *  plugin is loaded. The guarantee is upheld across File > New, File > Open, File > Save, and
 *  Maya Reference workflows.
 *
 *  Because nodes are locked at creation, the DG node name is stable for the lifetime of
 *  the scene and serves as both the human-readable display name and the unique key used
 *  by the manager's lookup tables.
 *
 *  Usage:
 *  \code
 *  // Register a node once (typically via a static initializer in a .cpp file):
 *  UsdSceneSettingsManager::registerSettingNode(
 *      "UsdDefaultRenderSettings",  // Maya DG node name (also the manager's lookup key)
 *      [](UsdStageRefPtr stage) {
 *          // populate stage with domain-specific USD prims
 *      });
 *
 *  // Obtain the stage for a registered node:
 *  UsdStageRefPtr stage = UsdSceneSettingsManager::getStage("UsdDefaultRenderSettings");
 *  \endcode
 *
 *  The manager is activated by calling onPluginInitialize() from the mayaUsdPlugin
 *  entry point (plugin/adsk/plugin/plugin.cpp::initializePlugin) and cleaned up via
 *  onPluginFinalize() in the matching uninitializePlugin path.
 */
class MAYAUSD_CORE_PUBLIC UsdSceneSettingsManager
{
public:
    //! Signature of the populator function that seeds a node's USD stage.
    using Populator = std::function<void(PXR_NS::UsdStageRefPtr)>;

    // -----------------------------------------------------------------------
    // Registration
    // -----------------------------------------------------------------------

    /*! Register a settings node by its DG node name.
     *
     *  The \p nodeName string is used directly as the Maya DG node name when the
     *  manager creates the node (e.g. "UsdDefaultRenderSettings"). The node is locked
     *  immediately after creation, so its name is stable and is reused as the
     *  manager's lookup key for all subsequent find / getStage calls.
     *
     *  If the plugin is already initialized when registerSettingNode() is called (e.g. a
     *  sub-plugin loaded after the main plugin), the corresponding node is created immediately.
     *
     *  \param nodeName  Maya DG node name and lookup key, e.g. "UsdDefaultRenderSettings".
     *  \param populate  Function invoked once to seed the stage with domain content.
     */
    static void registerSettingNode(const std::string& nodeName, Populator populate);

    //! Invoke the populator for \p nodeName on \p stage. Called from
    //! UsdSettingsNode::ensureStage().
    static void callPopulator(const std::string& nodeName, PXR_NS::UsdStageRefPtr stage);

    // -----------------------------------------------------------------------
    // Node access
    // -----------------------------------------------------------------------

    //! Return the live MObject for the managed singleton named \p nodeName,
    //! or a null MObject if none is tracked. Pure O(1) cache lookup; the
    //! manager itself is the only authority on which singletons exist (see
    //! the class doc), so this never falls through to a scene scan.
    static MObject find(const std::string& nodeName);

    //! Return the USD stage for \p nodeName, creating the node if needed.
    static PXR_NS::UsdStageRefPtr getStage(const std::string& nodeName);

    //! Return the USD stage for the managed node whose DG name is \p nodeName,
    //! or nullptr if no such managed node exists. Unlike getStage(), this never
    //! creates the node: callers that pass an unregistered or torn-down name
    //! always get nullptr. The stage itself is materialized lazily on first
    //! call, which runs the populator and fires the stage observer hook.
    //! Used by the UFE stage resolver so GatewayHierarchy descent can resolve
    //! a settings-node UFE path.
    static PXR_NS::UsdStageRefPtr getStageForNodeName(const std::string& nodeName);

    //! Reverse lookup: return the live MObject of the managed node whose USD stage
    //! matches \p stage, or a null MObject if none. Does NOT trigger lazy stage
    //! creation (uses UsdSettingsNode::hasStage()). Used by the UFE stagePath()
    //! resolver as a non-mutating fallback so that StagesSubject::stageChanged can
    //! map a sender stage back to its UFE gateway path and forward UFE notifications.
    static MObject nodeForStage(PXR_NS::UsdStageWeakPtr stage);

    /*! Return the live USD stages of all managed nodes that have already
     *  materialized a stage. Does NOT trigger lazy stage creation (only
     *  returns stages where UsdSettingsNode::hasStage() is true).
     *
     *  Intended for callers that want to walk live settings-node stages
     *  without perturbing them — e.g. MayaStagesSubject::setupListeners()
     *  rebuilding USD-notice observation after a clearListeners().
     */
    static std::vector<PXR_NS::UsdStageRefPtr> getAllLiveStages();

    // -----------------------------------------------------------------------
    // Stage observation hook
    //
    // The UFE layer installs a hook here at plugin initialization. The hook
    // is invoked exactly once for each stage whenever a managed node first
    // materializes its USD stage (lazy ensureStage() or deserialization on
    // File > Open). It lets the UFE notification subject register USD
    // listeners on the new stage without making this layer depend on UFE.
    // Lookup APIs (getStage, getStageForNodeName) are pure getters; they do
    // not invoke the hook.
    // -----------------------------------------------------------------------

    using StageObserverHook = std::function<void(const PXR_NS::UsdStageRefPtr&)>;

    //! Install (or clear, with a default-constructed argument) the hook.
    static void setStageObserverHook(StageObserverHook hook);

    //! Invoke the installed hook (if any) for \p stage. Called by
    //! UsdSettingsNode::ensureStage() and ::deserializeFromAttributes()
    //! immediately after the stage is materialized. No-op if no hook is
    //! installed or \p stage is null.
    static void notifyStageObserver(const PXR_NS::UsdStageRefPtr& stage);

    // -----------------------------------------------------------------------
    // Plugin lifecycle (called from MayaUsdProxyShapePlugin)
    // -----------------------------------------------------------------------

    //! Install scene callbacks and create one node per registered node name.
    //! Must be called after registerNode() for UsdSettingsNode::typeName.
    static void onPluginInitialize();

    //! Remove scene callbacks and delete all managed nodes.
    //! Must be called before deregisterNode() for UsdSettingsNode::typeName.
    static void onPluginFinalize();

private:
    //! Return the live MObject for \p nodeName, creating it if it does not
    //! yet exist. Used by the manager's own create-on-demand and lifecycle
    //! paths; external callers go through getStage() instead.
    static MObject findOrCreate(const std::string& nodeName);

    //! Create a UsdSettingsNode whose locked DG name is \p nodeName.
    static MObject createNode(const std::string& nodeName);

    //! Scan the scene for existing UsdSettingsNode instances and populate the
    //! instances map.
    static void rebuildInstancesFromScene();

    //! Cast a valid MObjectHandle to a UsdSettingsNode*. Returns nullptr on failure.
    static UsdSettingsNode* nodeFromHandle(const MObjectHandle& handle);

    static void onAfterNew(void* clientData);
    static void onAfterOpen(void* clientData);
    static void onBeforeSave(void* clientData);

    // Storage is wrapped in function-local statics so registration from
    // other TUs' static initializers is order-independent.

    //! Registered node name -> populator function.
    static std::map<std::string, Populator>& registry();

    //! Registered node name -> live node handle.
    static std::map<std::string, MObjectHandle>& instances();

    //! Optional hook called after a managed node materializes its USD stage.
    static StageObserverHook& stageObserverHook();

    static MCallbackId _afterNewCbId;
    static MCallbackId _afterOpenCbId;
    static MCallbackId _beforeSaveCbId;
    static bool        _isPluginInitialized;
};

} // namespace MAYAUSD_NS_DEF

#endif // MAYAUSD_USD_SCENE_SETTINGS_MANAGER_H
