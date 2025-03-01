//
// Copyright 2021 Autodesk
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
#ifndef MAYAUSD_UTILS_UTILSERIALIZATION_H
#define MAYAUSD_UTILS_UTILSERIALIZATION_H

#include <mayaUsd/base/api.h>
#include <mayaUsd/nodes/proxyShapeBase.h>

#include <pxr/pxr.h>
#include <pxr/usd/sdf/fileFormat.h>
#include <pxr/usd/sdf/layer.h>

/// General utility functions used when serializing Usd edits during a save operation
namespace MAYAUSD_NS_DEF {
namespace utils {

/*! \brief Helps suggest a folder to export anonymous layers to.  Checks in order:
    1. File-backed root layer folder.
    2. Current Maya scene folder.
    3. Current Maya workspace scenes folder.
 */
MAYAUSD_CORE_PUBLIC
std::string suggestedStartFolder(PXR_NS::UsdStageRefPtr stage);

/*! \brief Queries Maya for the current workspace "scenes" folder.
 */
MAYAUSD_CORE_PUBLIC
std::string getSceneFolder();

MAYAUSD_CORE_PUBLIC
std::string generateUniqueFileName(const std::string& basename);

MAYAUSD_CORE_PUBLIC
std::string generateUniqueLayerFileName(const std::string& basename, const SdfLayerRefPtr& layer);

/*! \brief Queries the Maya optionVar that decides what the internal format
    of a .usd file should be, either "usdc" or "usda".
 */
MAYAUSD_CORE_PUBLIC
std::string usdFormatArgOption();

enum USDUnsavedEditsOption
{
    kSaveToUSDFiles = 1,
    kSaveToMayaSceneFile,
    kIgnoreUSDEdits
};
/*! \brief Queries the Maya optionVar that decides which saving option Maya
    should use for Usd edits.
 */
MAYAUSD_CORE_PUBLIC
USDUnsavedEditsOption serializeUsdEditsLocationOption();

/*! \brief Return if the relative-path plug is set to true on the proxy shape.
 */
MAYAUSD_CORE_PUBLIC
bool isProxyShapePathRelative(MayaUsdProxyShapeBase& proxyShape);

/*! \brief File path mode for the setNewProxyPath and isProxyPathModeRelative functions.
 *         kProxyPathRelative makes the file path relative to the Maya scene.
 *         kProxyPathAbsolute makes the file path absolute.
 *         kProxyPathFollowProxyShape reads the relative-path plug of the proxy shape
 *                                    to decide.
 *         kProxyPathFollowOptionVar reads the mayaUsd_MakePathRelativeToSceneFile
 *                                   options to decide.
 */
enum ProxyPathMode
{
    kProxyPathRelative,
    kProxyPathAbsolute,
    kProxyPathFollowProxyShape,
    kProxyPathFollowOptionVar
};

/*! \brief Convert the proxy path mode into a boolean telling if the path should be relative.
 *  \note the proxy node name is only required for the kProxyPathFollowProxyShape mode.
 */
MAYAUSD_CORE_PUBLIC
bool isProxyPathModeRelative(ProxyPathMode proxyPathMode, const MString& proxyNodeName);

/*! \brief Utility function to update the file path attribute on the proxy shape
    when an anonymous root layer gets exported to disk. Also optionally updates
    the target layer if the anonymous layer was the target layer.
 */
MAYAUSD_CORE_PUBLIC
void setNewProxyPath(
    const MString&        proxyNodeName,
    const MString&        newValue,
    ProxyPathMode         proxyPathMode,
    const SdfLayerRefPtr& layer,
    bool                  isTargetLayer);

struct LayerParent
{
    // Every layer that we are saving should have either a parent layer that
    // we will need to remap to point to the new path, or the stage if it is an
    // anonymous root layer.
    SdfLayerRefPtr _layerParent;
    std::string    _proxyPath;
};

struct LayerInfo
{
    UsdStageRefPtr              stage;
    SdfLayerRefPtr              layer;
    MayaUsd::utils::LayerParent parent;
};

struct PathInfo
{
    std::string absolutePath;
    bool        savePathAsRelative { false };
    std::string customRelativeAnchor;
};

using LayerInfos = std::vector<LayerInfo>;

struct StageLayersToSave
{
    LayerInfos                  _anonLayers;
    std::vector<SdfLayerRefPtr> _dirtyFileBackedLayers;
};

/*! \brief Save an layer to disk to the given file path and using the given format.
    If the file path is empty then use the current file path of the layer.
    If the format is empty then use the current user-selected USD format option
    as defined by the usdFormatArgOption() function. (See above.)

    If the file path is relative, then it can be made relative to either the scene
    file (for the root layer) or its parent layer (for sub-layers). We assume the
    caller voluntarily made the path relative.
 */
MAYAUSD_CORE_PUBLIC
bool saveLayerWithFormat(
    SdfLayerRefPtr     layer,
    const std::string& requestedFilePath = "",
    const std::string& requestedFormatArg = "");

/*! \brief Save an anonymous layer to disk and update the sublayer path array
    in the parent layer.
 */
MAYAUSD_CORE_PUBLIC
PXR_NS::SdfLayerRefPtr saveAnonymousLayer(
    PXR_NS::UsdStageRefPtr stage,
    PXR_NS::SdfLayerRefPtr anonLayer,
    LayerParent            parent,
    const std::string&     basename,
    std::string            formatArg = "",
    std::string*           errorMsg = nullptr);

/*! \brief Save an anonymous layer to disk and update the sublayer path array
    in the parent layer.
 */
MAYAUSD_CORE_PUBLIC
PXR_NS::SdfLayerRefPtr saveAnonymousLayer(
    PXR_NS::UsdStageRefPtr stage,
    PXR_NS::SdfLayerRefPtr anonLayer,
    const PathInfo&        pathInfo,
    LayerParent            parent,
    std::string            formatArg = "",
    std::string*           errorMsg = nullptr);

/*! \brief Update the list of sub-layers with a new layer identity.
 *         The new sub-layer is identified by its path explicitly,
 *         because a given layer might get referenced through multiple
 *         different relative paths, so we cannot interrogate it about
 *         what its path is.
 */
MAYAUSD_CORE_PUBLIC
void updateSubLayer(
    const SdfLayerRefPtr& parentLayer,
    const SdfLayerRefPtr& oldSubLayer,
    const std::string&    newSubLayerPath);

/*! \brief Ensures that the filepath contains a valid USD extension.
 */
MAYAUSD_CORE_PUBLIC
void ensureUSDFileExtension(std::string& filePath);

/*! \brief Check the sublayer stack of the stage looking for any anonymous
    layers that will need to be saved.
 */
MAYAUSD_CORE_PUBLIC
void getLayersToSaveFromProxy(const std::string& proxyPath, StageLayersToSave& layersInfo);

} // namespace utils
} // namespace MAYAUSD_NS_DEF

#endif // MAYAUSD_UTILS_UTILSERIALIZATION_H
