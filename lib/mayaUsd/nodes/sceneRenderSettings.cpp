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

// Registers the "UsdDefaultRenderSettings" node with UsdSceneSettingsManager.
// This is the only file that needs to change when the render-settings USD
// schema layout evolves; everything else (node type, callbacks, serialization)
// is handled generically by the manager and UsdSettingsNode.

#include <mayaUsd/nodes/usdSceneSettingsManager.h>

#include <pxr/base/tf/token.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdRender/settings.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

// Use a static initializer so the node is registered before plugin initialization
// calls onPluginInitialize(). If the plugin is already running (sub-plugin load),
// registerSettingNode() creates the node immediately.
// NOLINTNEXTLINE(cert-err58-cpp) -- intentional static initializer pattern
const bool kRenderSettingsRegistered = []() {
    MayaUsd::UsdSceneSettingsManager::registerSettingNode(
        "UsdDefaultRenderSettings", [](PXR_NS::UsdStageRefPtr stage) {
            if (!stage) {
                return;
            }

            // Create a /Render scope to hold all render settings,
            // per UsdRender conventions (https://openusd.org/dev/api/usd_render_page_front.html).
            const SdfPath renderScopePath("/Render");
            const SdfPath renderSettingsPath("/Render/SceneRenderSettings");

            UsdGeomScope::Define(stage, renderScopePath);

            // Define the RenderSettings prim; leave attributes un-authored so they
            // fall back to the UsdRenderSettings schema defaults.
            UsdRenderSettings renderSettings = UsdRenderSettings::Define(stage, renderSettingsPath);
            if (!renderSettings) {
                return;
            }

            // Set stage metadata so consumers can discover the primary prim.
            stage->SetMetadata(TfToken("renderSettingsPrimPath"), renderSettingsPath.GetString());
        });
    return true;
}();

} // namespace
