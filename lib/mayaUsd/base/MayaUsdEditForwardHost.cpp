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
#include "MayaUsdEditForwardHost.h"

#include <maya/MGlobal.h>

#ifdef WANT_ADSKUSDEDITFORWARD_BUILD

void MayaUsdEditForwardHost::ExecuteInCmd(std::function<void()> callback, bool immediate)
{
    if (immediate) {
        if (callback) {
            callback();
        }
        return;
    }

    static std::vector<std::function<void()>> callbacks;
    callbacks.push_back(callback);
    MGlobal::executeTaskOnIdle([](void* data) {
        for (auto cb : callbacks) {
            if (cb) {
                cb();
            }
        }
        callbacks.clear();
    });
}

bool MayaUsdEditForwardHost::IsEditForwardingPaused() const { return _paused; }

void MayaUsdEditForwardHost::PauseEditForwarding(bool pause) { _paused = pause; }

void MayaUsdEditForwardHost::TrackLayerStates(const pxr::SdfLayerHandle& layer)
{
    // TODO : need to hook up maya-usd layer tracking for undo.
}

#endif // WANT_ADSKUSDEDITFORWARD_BUILD
