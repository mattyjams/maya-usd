//
// Copyright 2016 Pixar
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
#include "proxyShapeBase.h"

#include <mayaUsd/base/debugCodes.h>
#include <mayaUsd/base/tokens.h>
#include <mayaUsd/fileio/utils/readUtil.h>
#include <mayaUsd/fileio/utils/writeUtil.h>
#include <mayaUsd/listeners/proxyShapeNotice.h>
#include <mayaUsd/nodes/layerManager.h>
#include <mayaUsd/nodes/proxyShapeStageExtraData.h>
#include <mayaUsd/nodes/stageData.h>
#include <mayaUsd/ufe/Utils.h>
#include <mayaUsd/undo/OpUndoItemMuting.h>
#include <mayaUsd/utils/customLayerData.h>
#include <mayaUsd/utils/diagnosticDelegate.h>
#include <mayaUsd/utils/layerLocking.h>
#include <mayaUsd/utils/layerMuting.h>
#include <mayaUsd/utils/loadRules.h>
#include <mayaUsd/utils/query.h>
#include <mayaUsd/utils/stageCache.h>
#include <mayaUsd/utils/targetLayer.h>
#include <mayaUsd/utils/util.h>
#include <mayaUsd/utils/utilFileSystem.h>
#include <mayaUsd/utils/variantFallbacks.h>

#include <usdUfe/utils/layers.h>

#include <pxr/base/gf/bbox3d.h>
#include <pxr/base/gf/range3d.h>
#include <pxr/base/gf/ray.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/js/json.h>
#include <pxr/base/tf/envSetting.h>
#include <pxr/base/tf/fileUtils.h>
#include <pxr/base/tf/hash.h>
#include <pxr/base/tf/pathUtils.h>
#include <pxr/base/tf/staticData.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/tf/stringUtils.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/trace/trace.h>
#include <pxr/pxr.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/pcp/types.h>
#include <pxr/usd/sdf/attributeSpec.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/stageCacheContext.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/boundable.h>
#include <pxr/usd/usdGeom/gprim.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdUtils/stageCache.h>

#include <maya/MBoundingBox.h>
#include <maya/MDGContext.h>
#include <maya/MDGContextGuard.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MEvaluationNode.h>
#include <maya/MFileIO.h>
#include <maya/MFnCompoundAttribute.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnData.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnPluginData.h>
#include <maya/MFnReference.h>
#include <maya/MFnStringArrayData.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MPoint.h>
#include <maya/MProfiler.h>
#include <maya/MPxSurfaceShape.h>
#include <maya/MSceneMessage.h>
#include <maya/MSelectionMask.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MTime.h>
#include <maya/MUuid.h>
#include <maya/MViewport2Renderer.h>
#include <ufe/path.h>
#include <ufe/pathString.h>

#include <ghc/filesystem.hpp>

#include <map>
#include <string>
#include <utility>
#include <vector>

using MayaUsd::LayerManager;
using MayaUsd::ProxyAccessor;

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(MayaUsdProxyShapeBaseTokens, MAYAUSD_PROXY_SHAPE_BASE_TOKENS);

MayaUsdProxyShapeBase::ClosestPointDelegate MayaUsdProxyShapeBase::_sharedClosestPointDelegate
    = nullptr;

const std::string  kAnonymousLayerName { "anonymousLayer1" };
const std::string  kSessionLayerPostfix { "-session" };
const std::string  kUnsharedStageLayerName { "unshareableLayer" };
static const char* kMutedLayersAttrName = "mutedLayers";
static const char* kLockedLayersAttrName = "lockedLayers";

// ========================================================

// TypeID from the MayaUsd type ID range.
const MTypeId MayaUsdProxyShapeBase::typeId(0x58000094);
const MString MayaUsdProxyShapeBase::typeName(MayaUsdProxyShapeBaseTokens->MayaTypeName.GetText());

const MString MayaUsdProxyShapeBase::displayFilterName(
    TfStringPrintf("%sDisplayFilter", MayaUsdProxyShapeBaseTokens->MayaTypeName.GetText()).c_str());
const MString MayaUsdProxyShapeBase::displayFilterLabel("USD Proxies");

std::atomic<int> g_proxyShapeInstancesCount;

// Attributes
MObject MayaUsdProxyShapeBase::filePathAttr;
MObject MayaUsdProxyShapeBase::filePathRelativeAttr;
MObject MayaUsdProxyShapeBase::primPathAttr;
MObject MayaUsdProxyShapeBase::excludePrimPathsAttr;
MObject MayaUsdProxyShapeBase::loadPayloadsAttr;
MObject MayaUsdProxyShapeBase::shareStageAttr;
MObject MayaUsdProxyShapeBase::timeAttr;
MObject MayaUsdProxyShapeBase::complexityAttr;
MObject MayaUsdProxyShapeBase::inStageDataAttr;
MObject MayaUsdProxyShapeBase::inStageDataCachedAttr;
MObject MayaUsdProxyShapeBase::stageCacheIdAttr;
MObject MayaUsdProxyShapeBase::drawRenderPurposeAttr;
MObject MayaUsdProxyShapeBase::drawProxyPurposeAttr;
MObject MayaUsdProxyShapeBase::drawGuidePurposeAttr;
MObject MayaUsdProxyShapeBase::sessionLayerNameAttr;
MObject MayaUsdProxyShapeBase::rootLayerNameAttr;
MObject MayaUsdProxyShapeBase::mutedLayersAttr;
MObject MayaUsdProxyShapeBase::lockedLayersAttr;
// Change counter attributes
MObject MayaUsdProxyShapeBase::updateCounterAttr;
MObject MayaUsdProxyShapeBase::resyncCounterAttr;
// Output attributes
MObject MayaUsdProxyShapeBase::outTimeAttr;
MObject MayaUsdProxyShapeBase::outStageDataAttr;
MObject MayaUsdProxyShapeBase::outStageCacheIdAttr;
MObject MayaUsdProxyShapeBase::variantFallbacksAttr;
MObject MayaUsdProxyShapeBase::layerManagerAttr;

namespace {
// utility function to extract the tag name from an anonymous layer.
// e.g
// given layer identifier = anon:00000232FE3FB470:anonymousLayer1234
// tag name = anonymousLayer1234
std::string extractAnonTagName(const std::string& identifier)
{
    std::size_t found = identifier.find_last_of(":");
    return identifier.substr(found + 1);
}

// recursive function to create new anonymous Sublayer(s)
// and set the edit target accordingly.
void createNewAnonSubLayerRecursive(
    const UsdStageRefPtr& newUsdStage,
    const SdfLayerRefPtr& targetLayer,
    const SdfLayerRefPtr& parentLayer)
{
    if (!parentLayer->IsAnonymous()) {
        return;
    }

    SdfSubLayerProxy sublayers = parentLayer->GetSubLayerPaths();
    for (auto path : sublayers) {
        SdfLayerRefPtr subLayer = SdfLayer::Find(path);
        if (subLayer) {
            const std::string tagName = extractAnonTagName(subLayer->GetIdentifier());
            if (subLayer->IsAnonymous()) {
                SdfLayerRefPtr newLayer = SdfLayer::CreateAnonymous(tagName);
                newLayer->TransferContent(subLayer);

                size_t index = sublayers.Find(path);
                parentLayer->RemoveSubLayerPath(index);
                parentLayer->InsertSubLayerPath(newLayer->GetIdentifier(), index);

                if (extractAnonTagName(targetLayer->GetIdentifier()) == tagName) {
                    newUsdStage->SetEditTarget(newLayer);
                }

                createNewAnonSubLayerRecursive(newUsdStage, targetLayer, newLayer);
            } else {
                if (extractAnonTagName(targetLayer->GetIdentifier()) == tagName) {
                    newUsdStage->SetEditTarget(subLayer);
                }
            }
        }
    }
}
//! Profiler category for proxy accessor events
const int _shapeBaseProfilerCategory
    = MProfiler::addCategory("ProxyShapeBase", "ProxyShapeBase events");

struct InComputeGuard
{
    InComputeGuard(MayaUsdProxyShapeBase& proxy)
        : _proxy(proxy)
    {
        _proxy.in_compute++;
    }

    ~InComputeGuard() { _proxy.in_compute--; }

private:
    MayaUsdProxyShapeBase& _proxy;
};

} // namespace

/* static */
void* MayaUsdProxyShapeBase::creator() { return new MayaUsdProxyShapeBase(); }

/* static */
MStatus MayaUsdProxyShapeBase::initialize()
{
    MStatus retValue = MS::kSuccess;

    //
    // create attr factories
    //
    MFnNumericAttribute numericAttrFn;
    MFnTypedAttribute   typedAttrFn;
    MFnUnitAttribute    unitAttrFn;

    filePathAttr
        = typedAttrFn.create("filePath", "fp", MFnData::kString, MObject::kNullObj, &retValue);
    typedAttrFn.setInternal(true);
    typedAttrFn.setAffectsAppearance(true);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    CHECK_MSTATUS_AND_RETURN_IT(typedAttrFn.setUsedAsFilename(true));
    retValue = addAttribute(filePathAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    filePathRelativeAttr
        = numericAttrFn.create("filePathRelative", "fpr", MFnNumericData::kBoolean, 0.0, &retValue);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    numericAttrFn.setInternal(true);
    numericAttrFn.setStorable(false);
    numericAttrFn.setWritable(false);
    retValue = addAttribute(filePathRelativeAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    primPathAttr
        = typedAttrFn.create("primPath", "pp", MFnData::kString, MObject::kNullObj, &retValue);
    typedAttrFn.setInternal(true);
    typedAttrFn.setAffectsAppearance(true);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = addAttribute(primPathAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    excludePrimPathsAttr = typedAttrFn.create(
        "excludePrimPaths", "epp", MFnData::kString, MObject::kNullObj, &retValue);
    typedAttrFn.setInternal(true);
    typedAttrFn.setAffectsAppearance(true);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = addAttribute(excludePrimPathsAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    loadPayloadsAttr
        = numericAttrFn.create("loadPayloads", "lpl", MFnNumericData::kBoolean, 1.0, &retValue);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    numericAttrFn.setKeyable(false);
    numericAttrFn.setReadable(false);
    numericAttrFn.setInternal(true);
    numericAttrFn.setHidden(true);
    numericAttrFn.setAffectsAppearance(true);
    retValue = addAttribute(loadPayloadsAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    shareStageAttr
        = numericAttrFn.create("shareStage", "scmp", MFnNumericData::kBoolean, 1.0, &retValue);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    numericAttrFn.setKeyable(false);
    numericAttrFn.setReadable(false);
    numericAttrFn.setAffectsAppearance(true);
    retValue = addAttribute(shareStageAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    timeAttr = unitAttrFn.create("time", "tm", MFnUnitAttribute::kTime, 0.0, &retValue);
    unitAttrFn.setAffectsAppearance(true);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = addAttribute(timeAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    complexityAttr = numericAttrFn.create("complexity", "cplx", MFnNumericData::kInt, 0, &retValue);
    numericAttrFn.setMin(0);
    numericAttrFn.setSoftMax(4);
    numericAttrFn.setMax(8);
    numericAttrFn.setChannelBox(true);
    numericAttrFn.setStorable(false);
    numericAttrFn.setAffectsAppearance(true);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = addAttribute(complexityAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    inStageDataAttr = typedAttrFn.create(
        "inStageData", "id", MayaUsdStageData::mayaTypeId, MObject::kNullObj, &retValue);
    typedAttrFn.setReadable(false);
    typedAttrFn.setStorable(false);
    typedAttrFn.setDisconnectBehavior(MFnNumericAttribute::kReset); // on disconnect, reset to Null
    typedAttrFn.setAffectsAppearance(true);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = addAttribute(inStageDataAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    // inStageData or filepath-> inStageDataCached -> outStageData
    inStageDataCachedAttr = typedAttrFn.create(
        "inStageDataCached", "idc", MayaUsdStageData::mayaTypeId, MObject::kNullObj, &retValue);
    typedAttrFn.setStorable(false);
    typedAttrFn.setWritable(false);
    typedAttrFn.setAffectsAppearance(true);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = addAttribute(inStageDataCachedAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    stageCacheIdAttr
        = numericAttrFn.create("stageCacheId", "stcid", MFnNumericData::kInt, -1, &retValue);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    numericAttrFn.setDisconnectBehavior(
        MFnNumericAttribute::kReset); // on disconnect, reset to default
    numericAttrFn.setStorable(false);
    numericAttrFn.setCached(true);
    numericAttrFn.setConnectable(true);
    numericAttrFn.setReadable(true);
    numericAttrFn.setInternal(true);
    numericAttrFn.setAffectsAppearance(true);
    retValue = addAttribute(stageCacheIdAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    drawRenderPurposeAttr = numericAttrFn.create(
        "drawRenderPurpose", "drp", MFnNumericData::kBoolean, 0.0, &retValue);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    numericAttrFn.setKeyable(true);
    numericAttrFn.setReadable(false);
    numericAttrFn.setAffectsAppearance(true);
    retValue = addAttribute(drawRenderPurposeAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    drawProxyPurposeAttr
        = numericAttrFn.create("drawProxyPurpose", "dpp", MFnNumericData::kBoolean, 1.0, &retValue);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    numericAttrFn.setKeyable(true);
    numericAttrFn.setReadable(false);
    numericAttrFn.setAffectsAppearance(true);
    retValue = addAttribute(drawProxyPurposeAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    drawGuidePurposeAttr
        = numericAttrFn.create("drawGuidePurpose", "dgp", MFnNumericData::kBoolean, 0.0, &retValue);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    numericAttrFn.setKeyable(true);
    numericAttrFn.setReadable(false);
    numericAttrFn.setAffectsAppearance(true);
    retValue = addAttribute(drawGuidePurposeAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    variantFallbacksAttr = typedAttrFn.create(
        "variantFallbacks", "vfs", MFnData::kString, MObject::kNullObj, &retValue);
    typedAttrFn.setReadable(true);
    typedAttrFn.setWritable(true);
    typedAttrFn.setConnectable(true);
    typedAttrFn.setStorable(true);
    typedAttrFn.setAffectsAppearance(true);
    typedAttrFn.setInternal(true);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = addAttribute(variantFallbacksAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    // outputs
    outTimeAttr = unitAttrFn.create("outTime", "otm", MFnUnitAttribute::kTime, 0.0, &retValue);
    unitAttrFn.setCached(false);
    unitAttrFn.setConnectable(true);
    unitAttrFn.setReadable(true);
    unitAttrFn.setStorable(false);
    unitAttrFn.setWritable(false);
    unitAttrFn.setAffectsAppearance(true);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = addAttribute(outTimeAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    outStageDataAttr = typedAttrFn.create(
        "outStageData", "od", MayaUsdStageData::mayaTypeId, MObject::kNullObj, &retValue);
    typedAttrFn.setStorable(false);
    typedAttrFn.setWritable(false);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = addAttribute(outStageDataAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    updateCounterAttr
        = numericAttrFn.create("updateId", "upid", MFnNumericData::kInt64, -1, &retValue);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    numericAttrFn.setStorable(false);
    numericAttrFn.setWritable(false);
    numericAttrFn.setHidden(true);
    numericAttrFn.setInternal(true);
    retValue = addAttribute(updateCounterAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    resyncCounterAttr
        = numericAttrFn.create("resyncId", "rsid", MFnNumericData::kInt64, -1, &retValue);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    numericAttrFn.setStorable(false);
    numericAttrFn.setWritable(false);
    numericAttrFn.setHidden(true);
    numericAttrFn.setInternal(true);
    retValue = addAttribute(resyncCounterAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    outStageCacheIdAttr
        = numericAttrFn.create("outStageCacheId", "ostcid", MFnNumericData::kInt, -1, &retValue);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    numericAttrFn.setStorable(false);
    numericAttrFn.setWritable(false);
    retValue = addAttribute(outStageCacheIdAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    sessionLayerNameAttr = typedAttrFn.create(
        "outStageSessionLayerId", "oslid", MFnData::kString, MObject::kNullObj, &retValue);
    typedAttrFn.setInternal(true);
    typedAttrFn.setHidden(true);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = addAttribute(sessionLayerNameAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    rootLayerNameAttr = typedAttrFn.create(
        "outStageRootLayerId", "orlid", MFnData::kString, MObject::kNullObj, &retValue);
    typedAttrFn.setInternal(true);
    typedAttrFn.setHidden(true);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = addAttribute(rootLayerNameAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    mutedLayersAttr = typedAttrFn.create(
        kMutedLayersAttrName, "mla", MFnData::kStringArray, MObject::kNullObj, &retValue);
    typedAttrFn.setStorable(true);
    typedAttrFn.setWritable(true);
    typedAttrFn.setReadable(true);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = addAttribute(mutedLayersAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    lockedLayersAttr = typedAttrFn.create(
        kLockedLayersAttrName, "lockla", MFnData::kStringArray, MObject::kNullObj, &retValue);
    typedAttrFn.setStorable(true);
    typedAttrFn.setWritable(true);
    typedAttrFn.setReadable(true);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = addAttribute(lockedLayersAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    layerManagerAttr = typedAttrFn.create(
        "layerManager", "lymgr", MFnData::kString, MObject::kNullObj, &retValue);
    typedAttrFn.setStorable(true);
    typedAttrFn.setWritable(true);
    typedAttrFn.setReadable(true);
    typedAttrFn.setInternal(true);
    typedAttrFn.setHidden(true);
    typedAttrFn.setDisconnectBehavior(MFnNumericAttribute::kReset); // on disconnect, reset to Null
    typedAttrFn.setAffectsAppearance(false);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = addAttribute(layerManagerAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    //
    // add attribute dependencies
    //
    retValue = attributeAffects(timeAttr, outTimeAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    retValue = attributeAffects(filePathAttr, inStageDataCachedAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = attributeAffects(filePathAttr, outStageDataAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = attributeAffects(filePathAttr, outStageCacheIdAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = attributeAffects(filePathAttr, rootLayerNameAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = attributeAffects(filePathAttr, sessionLayerNameAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = attributeAffects(filePathAttr, primPathAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    retValue = attributeAffects(primPathAttr, outStageDataAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = attributeAffects(primPathAttr, outStageCacheIdAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    retValue = attributeAffects(shareStageAttr, inStageDataCachedAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = attributeAffects(shareStageAttr, outStageDataAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = attributeAffects(shareStageAttr, outStageCacheIdAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    retValue = attributeAffects(loadPayloadsAttr, inStageDataCachedAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = attributeAffects(loadPayloadsAttr, outStageDataAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = attributeAffects(loadPayloadsAttr, outStageCacheIdAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    retValue = attributeAffects(inStageDataAttr, inStageDataCachedAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = attributeAffects(inStageDataAttr, outStageDataAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = attributeAffects(inStageDataAttr, outStageCacheIdAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    retValue = attributeAffects(stageCacheIdAttr, outStageDataAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = attributeAffects(stageCacheIdAttr, inStageDataCachedAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = attributeAffects(stageCacheIdAttr, outStageCacheIdAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    retValue = attributeAffects(inStageDataCachedAttr, outStageDataAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    retValue = attributeAffects(inStageDataCachedAttr, outStageCacheIdAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    retValue = attributeAffects(variantFallbacksAttr, outStageDataAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    retValue = attributeAffects(layerManagerAttr, outStageDataAttr);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    return retValue;
}

/* static */
MayaUsdProxyShapeBase* MayaUsdProxyShapeBase::GetShapeAtDagPath(const MDagPath& dagPath)
{
    MObject mObj = dagPath.node();
    if (mObj.apiType() != MFn::kPluginShape) {
        TF_CODING_ERROR(
            "Could not get MayaUsdProxyShapeBase for non-plugin shape node "
            "at DAG path: %s (apiTypeStr = %s)",
            dagPath.fullPathName().asChar(),
            mObj.apiTypeStr());
        return nullptr;
    }

    const MFnDependencyNode depNodeFn(mObj);
    MayaUsdProxyShapeBase*  pShape = static_cast<MayaUsdProxyShapeBase*>(depNodeFn.userNode());
    if (!pShape) {
        TF_CODING_ERROR(
            "Could not get MayaUsdProxyShapeBase for node at DAG path: %s",
            dagPath.fullPathName().asChar());
        return nullptr;
    }

    return pShape;
}

/* static */
int MayaUsdProxyShapeBase::countProxyShapeInstances() { return g_proxyShapeInstancesCount; }

/* static */
void MayaUsdProxyShapeBase::SetClosestPointDelegate(ClosestPointDelegate delegate)
{
    _sharedClosestPointDelegate = delegate;
}

/* virtual */
bool MayaUsdProxyShapeBase::GetObjectSoftSelectEnabled() const { return false; }

void MayaUsdProxyShapeBase::enableProxyAccessor()
{
    _usdAccessor = ProxyAccessor::createAndRegister(*this);
}

void beforeSaveCallback(void* clientData)
{
    auto proxyShape = static_cast<MayaUsdProxyShapeBase*>(clientData);
    if (!proxyShape) {
        return;
    }

    MStatus           status;
    MFnDependencyNode depNode(proxyShape->thisMObject(), &status);
    CHECK_MSTATUS(status);

    MPlug filePathPlug = depNode.findPlug(MayaUsdProxyShapeBase::filePathAttr);
    MPlug filePathRelativePlug = depNode.findPlug(MayaUsdProxyShapeBase::filePathRelativeAttr);

    // Make proxy shape's file path relative if needed
    ghc::filesystem::path filePath(filePathPlug.asString().asChar());
    if (filePath.is_absolute() && filePathRelativePlug.asBool()) {
        auto relativePath
            = UsdMayaUtilFileSystem::getPathRelativeToMayaSceneFile(filePath.generic_string());
        filePathPlug.setString(relativePath.c_str());
    }
}

/* virtual */
void MayaUsdProxyShapeBase::postConstructor()
{
    MProfilingScope profilingScope(
        _shapeBaseProfilerCategory, MProfiler::kColorE_L3, "Issue Invalidate Stage Notice");

    setRenderable(true);

    MayaUsdProxyStageInvalidateNotice(*this).Send();

    if (_preSaveCallbackId == 0) {
        _preSaveCallbackId
            = MSceneMessage::addCallback(MSceneMessage::kBeforeSave, beforeSaveCallback, this);
    }

    MayaUsd::MayaNodeTypeObserver& shapeObserver = getProxyShapesObserver();
    MayaUsd::MayaNodeObserver*     observer = shapeObserver.addObservedNode(thisMObject());
    if (observer)
        observer->addListener(*this);
}

/* virtual */
bool MayaUsdProxyShapeBase::getInternalValue(const MPlug& plug, MDataHandle& handle)
{
    bool retVal = true;

    if (plug == updateCounterAttr) {
        handle.set(_UsdStageUpdateCounter);
    } else if (plug == resyncCounterAttr) {
        handle.set(_UsdStageResyncCounter);
    } else {
        retVal = MPxSurfaceShape::getInternalValue(plug, handle);
    }

    return retVal;
}

/* virtual */
MStatus MayaUsdProxyShapeBase::compute(const MPlug& plug, MDataBlock& dataBlock)
{
    InComputeGuard inComputeGuard(*this);

    if (plug == excludePrimPathsAttr || plug == timeAttr || plug == complexityAttr
        || plug == drawRenderPurposeAttr || plug == drawProxyPurposeAttr
        || plug == drawGuidePurposeAttr) {
        MProfilingScope profilingScope(
            _shapeBaseProfilerCategory,
            MProfiler::kColorE_L3,
            "Call MHWRender::MRenderer::setGeometryDrawDirty from compute");
        // If the attribute that needs to be computed is one of these, then it
        // does not affect the ouput stage data, but it *does* affect imaging
        // the shape. In that case, we notify Maya that the shape needs to be
        // redrawn and let it take care of computing the attribute. This covers
        // the case where an attribute on the proxy shape may have an incoming
        // connection from another node (e.g. "time1.outTime" being connected
        // to the proxy shape's "time" attribute). In that case,
        // setDependentsDirty() might not get called and only compute() might.
        MHWRender::MRenderer::setGeometryDrawDirty(thisMObject());
        return MS::kUnknownParameter;
    } else if (plug == inStageDataCachedAttr) {
        return computeInStageDataCached(dataBlock);
    } else if (plug == outTimeAttr) {
        auto retStatus = computeOutputTime(dataBlock);
        ProxyAccessor::compute(_usdAccessor, plug, dataBlock);
        return retStatus;
    } else if (plug == outStageDataAttr) {
        auto ret = computeOutStageData(dataBlock);
        return ret;
    } else if (plug == outStageCacheIdAttr) {
        return computeOutStageCacheId(dataBlock);
    } else if (plug.isDynamic()) {
        return ProxyAccessor::compute(_usdAccessor, plug, dataBlock);
    }

    return MS::kUnknownParameter;
}

/* virtual */
SdfLayerRefPtr MayaUsdProxyShapeBase::computeRootLayer(MDataBlock& dataBlock, const std::string&)
{
    if (LayerManager::supportedNodeType(MPxNode::typeId())) {
        const MString rootLayerName = dataBlock.inputValue(rootLayerNameAttr).asString();
        return LayerManager::findLayer(UsdMayaUtil::convert(rootLayerName), this);
    } else {
        return nullptr;
    }
}

/* virtual */
SdfLayerRefPtr MayaUsdProxyShapeBase::computeSessionLayer(MDataBlock& dataBlock)
{
    if (LayerManager::supportedNodeType(MPxNode::typeId())) {
        auto sessionLayerName = dataBlock.inputValue(sessionLayerNameAttr).asString();
        return LayerManager::findLayer(UsdMayaUtil::convert(sessionLayerName), this);
    } else {
        return nullptr;
    }
}

namespace {

void remapSublayerRecursive(
    const SdfLayerRefPtr&              layer,
    std::map<std::string, std::string> remappedLayers)
{
    if (!layer || remappedLayers.empty())
        return;

    bool                     updated = false;
    SdfSubLayerProxy         sublayerPaths = layer->GetSubLayerPaths();
    std::vector<std::string> newSublayerPaths;
    newSublayerPaths.reserve(sublayerPaths.size());
    for (const auto sublayerPath : sublayerPaths) {
        auto sublayer = SdfLayer::Find(sublayerPath);
        remapSublayerRecursive(sublayer, remappedLayers);
        if (remappedLayers.empty())
            return;

        if (remappedLayers.find(sublayerPath) != remappedLayers.end()) {
            updated = true;
            if (!remappedLayers[sublayerPath].empty())
                newSublayerPaths.push_back(remappedLayers[sublayerPath]);
            remappedLayers.erase(sublayerPath);
        } else {
            newSublayerPaths.push_back(sublayerPath);
        }
    }

    if (updated) {
        layer->SetSubLayerPaths(newSublayerPaths);
    }
}

void reproduceSharedStageState(
    const UsdStageRefPtr& stage,
    const SdfLayerRefPtr& sharedRootLayer,
    const SdfLayerRefPtr& unsharedRootLayer)
{
    if (!TF_VERIFY(stage))
        return;
    if (!TF_VERIFY(sharedRootLayer))
        return;
    if (!TF_VERIFY(unsharedRootLayer))
        return;

    // Transfer the FPS (frames-per-second) of the original root layer to the new unshared
    // root layer, so that the animation timeline does not change. We copy both the metadata
    // on the layer and on the stage object itself.
    if (sharedRootLayer->HasFramesPerSecond()) {
        const double fps = sharedRootLayer->GetFramesPerSecond();
        unsharedRootLayer->SetFramesPerSecond(fps);
        stage->SetFramesPerSecond(fps);
    }

    // Transfer the TCPS (timecodes-per-second) for the same reason as above.
    if (sharedRootLayer->HasTimeCodesPerSecond()) {
        const double tcps = sharedRootLayer->GetTimeCodesPerSecond();
        unsharedRootLayer->SetTimeCodesPerSecond(tcps);
        stage->SetTimeCodesPerSecond(tcps);
    }
}

} // namespace

MStatus MayaUsdProxyShapeBase::computeInStageDataCached(MDataBlock& dataBlock)
{
    MProfilingScope profilingScope(
        _shapeBaseProfilerCategory, MProfiler::kColorE_L3, "Compute inStageDataCached plug");

    MStatus retValue = MS::kSuccess;

    // Background computation is relying on normal context
    if (!dataBlock.context().isNormal()) {
        // Create the output outData ========
        MFnPluginData pluginDataFn;
        MObject       stageDataObj = pluginDataFn.create(MayaUsdStageData::mayaTypeId, &retValue);
        CHECK_MSTATUS_AND_RETURN_IT(retValue);

        MayaUsdStageData* outData
            = reinterpret_cast<MayaUsdStageData*>(pluginDataFn.data(&retValue));
        CHECK_MSTATUS_AND_RETURN_IT(retValue);

        // When evaluating in background we should point to the same stage as in normal context
        // This way we will share the stage between all evaluation context and avoid losing data
        // in case of dirty stage, i.e. stage with new or modified layers.
        MDGContext normalContext;
        {
            MDGContextGuard contextGuard(normalContext);
            MDataBlock      dataBlockForNormalContext = forceCache();

            MDataHandle inDataCachedHandleForNormalContext
                = dataBlockForNormalContext.inputValue(inStageDataCachedAttr, &retValue);
            CHECK_MSTATUS_AND_RETURN_IT(retValue);

            MayaUsdStageData* inData = dynamic_cast<MayaUsdStageData*>(
                inDataCachedHandleForNormalContext.asPluginData());

            // Set the outUsdStageData
            outData->stage = inData->stage;
            outData->primPath = inData->primPath;
        }

        // Retrieve data handle for stage data cache
        MDataHandle outDataCachedHandle = dataBlock.outputValue(inStageDataCachedAttr, &retValue);
        CHECK_MSTATUS_AND_RETURN_IT(retValue);

        outDataCachedHandle.set(outData);
        outDataCachedHandle.setClean();
        return MS::kSuccess;
    }

    // Normal context computation
    UsdStageRefPtr sharedUsdStage;
    UsdStageRefPtr unsharedUsdStage;
    UsdStageRefPtr finalUsdStage;
    SdfPath        primPath;

    MayaUsd::LayerNameMap layerNameMap = LayerManager::getLayerNameMap(this);

    MDataHandle inDataHandle = dataBlock.inputValue(inStageDataAttr, &retValue);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    bool sharableStage = isShareableStage();

    // Load the unshared comp from file
    // This is so that we can remap the anon layer identifiers that have been loaded from disk which
    // are saved in the unshared root layer
    if (!_unsharedStageRootLayer && !sharableStage) {
        // Once an anon layer is loaded the identifier changes
        _unsharedStageRootLayer = computeRootLayer(dataBlock, "");
        if (_unsharedStageRootLayer) {
            // Anon layers when loaded will have difference identifiers, remap them
            auto referencedLayers = MayaUsd::CustomLayerData::getStringArray(
                _unsharedStageRootLayer, MayaUsdMetadata->ReferencedLayers);
            VtArray<std::string> updatedReferences;
            for (const auto& identifier : referencedLayers) {
                // Update the identifier reference in the customer layer data
                auto layer = LayerManager::findLayer(identifier, this);
                if (layer) {
                    updatedReferences.push_back(layer->GetIdentifier());
                }
                // We also need to push this anyway in case we dont find it since file backed layers
                // arent in the layermanager database
                else {
                    updatedReferences.push_back(identifier);
                }
            }
            if (!updatedReferences.empty()) {
                MayaUsd::CustomLayerData::setStringArray(
                    updatedReferences, _unsharedStageRootLayer, MayaUsdMetadata->ReferencedLayers);
            }
        }
    }

    bool isIncomingStage = false;

    // Note: we explicitly always load no payload initially and will load them
    //       later on if requested. The reason is that there are other code elsewhere
    //       that updates the stages. Those functions must find the same existing stages.
    //       Somewhat unfortunately, the OpenUSD API to open a stage only finds an existing
    //       stage if *all* parameters passed to open the stage match, including the initial
    //       load set. In order to be consistent everywhere, we must always pass in the same
    //       initial load set. Given that not loading payloads is the "lightest" version,
    //       that is what we request.
    //
    //       Furthermore, for any stage that had its payload loaded or unloaded by the user
    //       we keep the detailed load and unload state of all prims, so we would use the
    //       load-none mode anyway and apply the detailed rules afterward, and this is
    //       probably the common case for stages that actually contain payloads.
    //
    //       TODO: in the future, we need to provide a stage (and layer) internal manager
    //             that would stand between MayaUSD and OpenUSD and manage stages and layers
    //             and deal with consistent Open and other considerations, like updating stages
    //             and layers when anonymous layers are saved to disk.
    const UsdStage::InitialLoadSet loadSet = UsdStage::InitialLoadSet::LoadNone;

    // If inData has an incoming connection, then use it. Otherwise generate stage from the filepath
    if (!inDataHandle.data().isNull()) {
        MayaUsdStageData* inStageData
            = dynamic_cast<MayaUsdStageData*>(inDataHandle.asPluginData());
        sharedUsdStage = inStageData->stage;
        primPath = inStageData->primPath;
        isIncomingStage = true;
    } else {
        // Check if we have a Stage from the Cache Id
        const auto cacheIdNum = dataBlock.inputValue(stageCacheIdAttr, &retValue).asInt();
        CHECK_MSTATUS_AND_RETURN_IT(retValue);
        const auto cacheId = UsdStageCache::Id::FromLongInt(cacheIdNum);
        const auto stageCached = cacheId.IsValid() && UsdUtilsStageCache::Get().Contains(cacheId);
        if (stageCached) {
            sharedUsdStage = UsdUtilsStageCache::Get().Find(cacheId);

            // Check what is the cache connected to
            MStringArray result;
            MGlobal::executeCommand(
                ("listConnections -t shape -shapes on " + this->name()), result);

            // The stage is only incoming if the cache is connected to a shape
            if (stageCached && result.length()) {
                isIncomingStage = true;
            }

            // If the stage set by stage ID is not anonymous, set the filePath
            // attribute to it so that it can be reloaded when the Maya scene
            // is re-opened.
            SdfLayerHandle rootLayer = sharedUsdStage->GetRootLayer();
            if (rootLayer && !rootLayer->IsAnonymous()) {
                MDataHandle outDataHandle = dataBlock.outputValue(filePathAttr, &retValue);
                CHECK_MSTATUS_AND_RETURN_IT(retValue);
                outDataHandle.set(MString(rootLayer->GetIdentifier().c_str()));
            }
        } else {
            //
            // Calculate from USD filepath and primPath and variantKey
            //

            // Get variant fallback from the proxy shape and set it as Global Variant fallbacks.
            PcpVariantFallbackMap defaultVariantFallbacks;
            PcpVariantFallbackMap fallbacks(updateVariantFallbacks(defaultVariantFallbacks, *this));

            // Get input attr values
            const MString file = dataBlock.inputValue(filePathAttr, &retValue).asString();
            CHECK_MSTATUS_AND_RETURN_IT(retValue);

            //
            // let the usd stage cache deal with caching the usd stage data
            //
            std::string fileString = TfStringTrimRight(file.asChar());

            TF_DEBUG(USDMAYA_PROXYSHAPEBASE)
                .Msg(
                    "ProxyShapeBase::reloadStage original USD file path is %s\n",
                    fileString.c_str());

            ghc::filesystem::path filestringPath(fileString);
            if (filestringPath.is_absolute()) {
                fileString = UsdMayaUtilFileSystem::resolvePath(fileString);
                TF_DEBUG(USDMAYA_PROXYSHAPEBASE)
                    .Msg(
                        "ProxyShapeBase::reloadStage resolved the USD file path to %s\n",
                        fileString.c_str());
            } else {
                fileString = UsdMayaUtilFileSystem::resolveRelativePathWithinMayaContext(
                    thisMObject(), fileString);
                TF_DEBUG(USDMAYA_PROXYSHAPEBASE)
                    .Msg(
                        "ProxyShapeBase::reloadStage resolved the relative USD file path to %s\n",
                        fileString.c_str());
            }

            // Fall back on providing the path "as is" to USD
            if (fileString.empty()) {
                fileString.assign(file.asChar(), file.length());
            }

            TF_DEBUG(USDMAYA_PROXYSHAPEBASE)
                .Msg("ProxyShapeBase::loadStage called for the usd file: %s\n", fileString.c_str());

            // == Load the Stage

            {
#if AR_VERSION == 1
                PXR_NS::ArGetResolver().ConfigureResolverForAsset(fileString);
#endif

                // When opening or creating stages we must have an active UsdStageCache.
                // The stage cache is the only one who holds a strong reference to the
                // UsdStage. See https://github.com/Autodesk/maya-usd/issues/528 for
                // more information.
                UsdStageCacheContext ctx(
                    UsdMayaStageCache::Get(loadSet, UsdMayaStageCache::ShareMode::Shared));

                SdfLayerRefPtr rootLayer
                    = sharableStage ? computeRootLayer(dataBlock, fileString) : nullptr;
                if (nullptr == rootLayer) {
                    rootLayer = SdfLayer::FindOrOpen(fileString);
                } else {
                    // When reloading a Maya scene in which the root layer was saved in
                    // the scene, the root layer will be anonymous. In order for the next
                    // compute to find the root layer again, we need to set it as the
                    // _anonymousRootLayer, as done below when creating a new proxy shape
                    // and the root layer is initially created.
                    if (rootLayer->IsAnonymous()) {
                        _anonymousRootLayer = rootLayer;
                    }
                }

                if (nullptr == rootLayer) {
                    // Create an empty in-memory root layer so that a new stage in memory
                    // will be created below by the UsdStage::Open call. This happens when
                    // a brand new stage with a new anonymous layer is requested to be
                    // created by the user.
                    if (!_anonymousRootLayer)
                        _anonymousRootLayer = SdfLayer::CreateAnonymous(kAnonymousLayerName);
                    rootLayer = _anonymousRootLayer;
                }

                {
                    // Note: computeSessionLayer will find a session layer *only* if the
                    //       Maya scene had been saved and thus serialized the session
                    //       layer. Otherwise it returns null which will mean to use
                    //       whatever session layer happens to be associated with the
                    //       stage we potentially find in the stage cache.
                    SdfLayerRefPtr sessionLayer = computeSessionLayer(dataBlock);

                    MProfilingScope profilingScope(
                        _shapeBaseProfilerCategory, MProfiler::kColorE_L3, "Open stage");

                    static const MString kSessionLayerOptionVarName(
                        MayaUsdOptionVars->ProxyTargetsSessionLayerOnOpen.GetText());

                    bool targetSession
                        = MGlobal::optionVarIntValue(kSessionLayerOptionVarName) == 1;
                    targetSession = targetSession || !rootLayer->PermissionToEdit();

                    // Note: UsdStage::Open has the peculiar design that it will return
                    //       any previously open stage that happen to match its arguments,
                    //       all its arguments, but only those arguments.
                    //
                    //       So *not* passing in a session layer will find any stage that
                    //       has the given root layer. That is why it is important *not* to
                    //       pass the session layer if the session layer is null. Otherwise
                    //       the cache would try find a stage *without* a session layer.
                    //
                    //       So, not passing the (null) session layer is how a newly-created
                    //       shared stage with the same root layer will find the correct stage
                    //       with the existing session layer.
                    //
                    //       If the stage is not in the cache and no session layer is passed
                    //       then UsdStage::Open will create the in-memory session layer for us,
                    //       just as we want.
                    if (sessionLayer) {
                        sharedUsdStage = UsdStage::Open(rootLayer, sessionLayer, loadSet);
                    } else {
                        sharedUsdStage = UsdStage::Open(rootLayer, loadSet);
                    }

                    sharedUsdStage->SetEditTarget(
                        targetSession ? sharedUsdStage->GetSessionLayer()
                                      : sharedUsdStage->GetRootLayer());
                }

                // Update file path attribute to match the correct root layer id if it was anonymous
                if (!fileString.empty() && SdfLayer::IsAnonymousLayerIdentifier(fileString)) {
                    if (rootLayer->IsAnonymous() && rootLayer->GetIdentifier() != fileString) {
                        MDataHandle outDataHandle = dataBlock.outputValue(filePathAttr, &retValue);
                        CHECK_MSTATUS_AND_RETURN_IT(retValue);
                        outDataHandle.set(MString(rootLayer->GetIdentifier().c_str()));
                    }
                }
            }
            // Reset only if the global variant fallbacks has been modified
            if (!fallbacks.empty()) {
                saveVariantFallbacks(fallbacks, *this);
                // Restore default value
                UsdStage::SetGlobalVariantFallbacks(defaultVariantFallbacks);
            }
        }
    }

    if (!sharedUsdStage) {
        return MS::kFailure;
    }

    // Create the output outData
    MFnPluginData pluginDataFn;
    pluginDataFn.create(MayaUsdStageData::mayaTypeId, &retValue);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    MayaUsdStageData* stageData = reinterpret_cast<MayaUsdStageData*>(pluginDataFn.data(&retValue));
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    if (isIncomingStage) {
        std::vector<std::string> incomingLayers { sharedUsdStage->GetRootLayer()->GetIdentifier() };
        _incomingLayers = UsdUfe::getAllSublayers(incomingLayers, true);
    } else {
        _incomingLayers.clear();
    }

    // Share the stage
    if (sharableStage) {
        // When we switch out of unshared we need to save this data so when the user toggles back
        // they get the same state they were in, but in order to this we have to keep the layers in
        // the hierarchy alive since the stage is gone and so they will get removed
        if (_unsharedStageRootLayer) {
            _unsharedStageRootSublayers.clear();
            auto subLayers = UsdUfe::getAllSublayerRefs(_unsharedStageRootLayer);
            _unsharedStageRootSublayers.insert(
                _unsharedStageRootSublayers.begin(), subLayers.begin(), subLayers.end());
        }
        finalUsdStage = sharedUsdStage;
    }
    // Own the stage
    else {
        SdfLayerRefPtr inRootLayer = sharedUsdStage->GetRootLayer();

        if (!_unsharedStageRootLayer) {
            _unsharedStageRootLayer = SdfLayer::CreateAnonymous(kUnsharedStageLayerName);
            // Add the incoming root layer as a subpath
            VtArray<std::string> referencedLayers { inRootLayer->GetIdentifier() };
            MayaUsd::CustomLayerData::setStringArray(
                referencedLayers, _unsharedStageRootLayer, MayaUsdMetadata->ReferencedLayers);
            _unsharedStageRootLayer->SetSubLayerPaths({ inRootLayer->GetIdentifier() });
        } else {

            // Check if we need to remap the source
            // At the moment we remap the old root with the new root  and we assume that the root
            // is the first item in the referenced layers
            auto referencedLayers = MayaUsd::CustomLayerData::getStringArray(
                _unsharedStageRootLayer, MayaUsdMetadata->ReferencedLayers);
            auto oldRootIdentifer = referencedLayers.empty() ? "" : referencedLayers[0];

            if (!oldRootIdentifer.empty() && oldRootIdentifer != inRootLayer->GetIdentifier()) {
                // Remap the existing source to the new one
                std::map<std::string, std::string> remappedLayers {
                    { oldRootIdentifer, inRootLayer->GetIdentifier() }
                };
                remapSublayerRecursive(_unsharedStageRootLayer, remappedLayers);
            }

            // If its a new layer (or wasn't remapped properly)
            auto sublayerIds = UsdUfe::getAllSublayers(_unsharedStageRootLayer);
            if (sublayerIds.find(inRootLayer->GetIdentifier()) == sublayerIds.end()) {
                // Add new layer to subpaths
                auto subLayers = _unsharedStageRootLayer->GetSubLayerPaths();
                subLayers.push_back(inRootLayer->GetIdentifier());
                _unsharedStageRootLayer->SetSubLayerPaths(subLayers);
            }

            // Remember layers referenced from source
            const VtArray<std::string> newReferencedLayers { inRootLayer->GetIdentifier() };
            MayaUsd::CustomLayerData::setStringArray(
                newReferencedLayers, _unsharedStageRootLayer, MayaUsdMetadata->ReferencedLayers);
        }

        unsharedUsdStage = getUnsharedStage(loadSet);
        finalUsdStage = unsharedUsdStage;

        // Transfer data of the original root layer to the new unshared root layer,
        // so that some user-visible state does not change. For example, we need to
        // transfer the FPS (frames-per-second) metadata so that the animations play
        // at the same rate.
        reproduceSharedStageState(finalUsdStage, inRootLayer, _unsharedStageRootLayer);
    }

    if (finalUsdStage) {
        // Compute the load set for the stage.
        MDataHandle loadPayloadsHandle = dataBlock.inputValue(loadPayloadsAttr, &retValue);
        CHECK_MSTATUS_AND_RETURN_IT(retValue);

        // Apply the payload rules based on either the saved payload rules
        // dynamic attribute containing the exact load rules for payload,
        // or the load-payload attribute.
        if (hasLoadRulesAttribute(*this)) {
            copyLoadRulesFromAttribute(*this, *finalUsdStage);
        } else {
            if (loadPayloadsHandle.asBool()) {
                finalUsdStage->Load(SdfPath("/"), UsdLoadPolicy::UsdLoadWithDescendants);
            } else {
                finalUsdStage->Unload(SdfPath("/"));
            }
        }

        primPath = finalUsdStage->GetPseudoRoot().GetPath();

        // EMSUSD-1087 Applying the lock permissions to layers should be done before the layer
        // muting
        copyLayerLockingFromAttribute(*this, layerNameMap, *finalUsdStage);

        UsdEditTarget editTarget;
        if (!_targetLayer) {
            editTarget = getEditTargetFromAttribute(*this, layerNameMap, *finalUsdStage);
            if (editTarget.IsValid()) {
                _targetLayer = editTarget.GetLayer();
            }
        }

        updateShareMode(sharedUsdStage, unsharedUsdStage, loadSet);
        // Note: setting the target layer must be done after updateShareMode()
        //       because the session layer changes when switching between shared
        //       and unshared and if the edit target was on the session layer,
        //      then the edit target is also updated by updateShareMode().
        if (_targetLayer) {
            // Note: it is possible the cached edit target layer is no longer valid,
            //       for example if it was deleted. Trying to set an invalid layer would
            //       throw an exception.
            if (UsdUfe::isLayerInStage(_targetLayer, *finalUsdStage)
                && _targetLayer != editTarget.GetLayer()) {
                editTarget = UsdEditTarget(_targetLayer);
            }
        }
        if (editTarget.IsValid()) {
            finalUsdStage->SetEditTarget(editTarget);
        }

        // Note: muting layer needs to be done after setting edit target layer
        //       because the target layer could be the muted layer itself,
        //       or one of the nested layers of a muted layer
        copyLayerMutingFromAttribute(*this, layerNameMap, *finalUsdStage);
    }

    // Set the outUsdStageData
    stageData->stage = finalUsdStage;
    stageData->primPath = primPath;

    // Set the data on the output plug
    MDataHandle inDataCachedHandle = dataBlock.outputValue(inStageDataCachedAttr, &retValue);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    inDataCachedHandle.set(stageData);
    inDataCachedHandle.setClean();

    return MS::kSuccess;
}

UsdStageRefPtr MayaUsdProxyShapeBase::getUnsharedStage(UsdStage::InitialLoadSet loadSet)
{
    // The unshared stages are *also* kept in a stage cache so that we can find them
    // again when proxy shape attribute change. For example, if the 'loadPayloads'
    // attribute change, we want to find the same unshared stage, we don't want to lose
    // edits, in particular in its session layer.
    //
    // We also need to be able to find them when switching a stage between non-shared
    // and shared, so that we can transfer the content of the session layer.
    //
    // Fortunately, the USD stage cache matches stages using *all* arguments provided.
    // So if the unshared session layer is unique to this proxy shape, there is no
    // chance of finding it by accident from another proxy shape.
    UsdStageCacheContext ctx(
        UsdMayaStageCache::Get(loadSet, UsdMayaStageCache::ShareMode::Unshared));

    if (!_unsharedStageSessionLayer)
        _unsharedStageSessionLayer = SdfLayer::CreateAnonymous();

    return UsdStage::UsdStage::Open(_unsharedStageRootLayer, _unsharedStageSessionLayer, loadSet);
}

void MayaUsdProxyShapeBase::updateShareMode(
    const UsdStageRefPtr&    sharedUsdStage,
    const UsdStageRefPtr&    unsharedUsdStage,
    UsdStage::InitialLoadSet loadSet)
{
    // Based on the previous shared mode and current shared mode of the stage,
    // transfer the content of the session layer from one to the other as needed.
    const auto shareMode = isShareableStage() ? ShareMode::Shared : ShareMode::Unshared;
    if (shareMode == _previousShareMode)
        return;

    // Only transfer the session content if the previous mode was known
    // or if the current mode is unshared, as the session layer content
    // is put in the shared stage when loaded from disk in a Maya scene.
    //
    // IOW:
    //     Shared   -> Unshared : copy
    //     Unshared -> Shared   : copy
    //     Unknown  -> Unshared : copy
    //
    //     Shared   -> Shared   : content already in place, pruned above
    //     Unshared -> Unshared : content already in place, pruned above
    //     Unknown  -> Shared   : content already in place
    //
    //     X -> Unknown : impossible since the new mode is always known
    if (_previousShareMode != ShareMode::Unknown || shareMode == ShareMode::Unshared)
        transferSessionLayer(shareMode, sharedUsdStage, unsharedUsdStage, loadSet);

    _previousShareMode = shareMode;
}

void MayaUsdProxyShapeBase::transferSessionLayer(
    ShareMode                currentMode,
    const UsdStageRefPtr&    sharedUsdStage,
    const UsdStageRefPtr&    unsharedUsdStage,
    UsdStage::InitialLoadSet loadSet)
{
    // When flipping to shared from unshared, the unshared set was not loaded.
    // Load it now to be able to transfer the session layer content.
    UsdStageRefPtr validUnsharedUsdStage
        = unsharedUsdStage ? unsharedUsdStage : getUnsharedStage(loadSet);

    SdfLayerHandle sharedSession = sharedUsdStage->GetSessionLayer();
    SdfLayerHandle unsharedSession = validUnsharedUsdStage->GetSessionLayer();

    if (!sharedSession || !unsharedSession)
        return;

    if (currentMode == ShareMode::Shared) {
        sharedSession->TransferContent(unsharedSession);
        if (unsharedSession == _targetLayer)
            _targetLayer = sharedSession;
    } else {
        unsharedSession->TransferContent(sharedSession);
        if (sharedSession == _targetLayer)
            _targetLayer = unsharedSession;
    }
}

MStatus MayaUsdProxyShapeBase::computeOutStageData(MDataBlock& dataBlock)
{
    MProfilingScope computeOutStageDatacomputeOutStageData(
        _shapeBaseProfilerCategory, MProfiler::kColorE_L3, "Compute outStageData plug");

    MStatus retValue = MS::kSuccess;

    const bool isNormalContext = dataBlock.context().isNormal();
    if (isNormalContext) {
        TfReset(_boundingBoxCache);

        // Reset the stage listener until we determine that everything is valid.
        _stageNoticeListener.SetStage(UsdStageWeakPtr());
        _stageNoticeListener.SetStageContentsChangedCallback(nullptr);
        _stageNoticeListener.SetStageObjectsChangedCallback(nullptr);
    }

    MDataHandle inDataCachedHandle = dataBlock.inputValue(inStageDataCachedAttr, &retValue);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    UsdStageRefPtr usdStage;

    MayaUsdStageData* inData = dynamic_cast<MayaUsdStageData*>(inDataCachedHandle.asPluginData());
    if (inData) {
        usdStage = inData->stage;
    }

    // If failed to get a valid stage, then
    // Propagate inDataCached -> outData
    // and return
    if (!usdStage) {
        MDataHandle outDataHandle = dataBlock.outputValue(outStageDataAttr, &retValue);
        CHECK_MSTATUS_AND_RETURN_IT(retValue);
        outDataHandle.copy(inDataCachedHandle);
        return MS::kSuccess;
    }

    // Get the primPath
    const SdfPath primPath = _GetPrimPath(dataBlock);
    if (primPath.IsEmpty()) {
        return MS::kFailure;
    }

    // Get the prim
    UsdPrim usdPrim;
    if (primPath == SdfPath::AbsoluteRootPath()) {
        usdPrim = usdStage->GetPseudoRoot();
    } else {
        // Validate assumption: primPath is descendant of passed-in stage primPath
        //   Make sure that the primPath is a child of the passed in stage's primpath
        if (primPath.HasPrefix(inData->primPath)) {
            usdPrim = usdStage->GetPrimAtPath(primPath);
        } else {
            TF_WARN(
                "%s: Shape primPath <%s> is not a descendant of input "
                "stage primPath <%s>",
                MPxSurfaceShape::name().asChar(),
                primPath.GetText(),
                inData->primPath.GetText());
        }
    }

    // Create the output outData
    MFnPluginData pluginDataFn;
    pluginDataFn.create(MayaUsdStageData::mayaTypeId, &retValue);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    MayaUsdStageData* stageData = reinterpret_cast<MayaUsdStageData*>(pluginDataFn.data(&retValue));
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    // Set the outUsdStageData
    stageData->stage = usdStage;
    stageData->primPath = primPath;

    //
    // set the data on the output plug
    //
    MDataHandle outDataHandle = dataBlock.outputValue(outStageDataAttr, &retValue);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    outDataHandle.set(stageData);
    outDataHandle.setClean();

    if (isNormalContext) {
        // Start listening for notices for the USD stage.
        _stageNoticeListener.SetStage(usdStage);
        _stageNoticeListener.SetStageContentsChangedCallback(
            [this](const UsdNotice::StageContentsChanged& notice) {
                return _OnStageContentsChanged(notice);
            });

        _stageNoticeListener.SetStageObjectsChangedCallback(
            [this](const UsdNotice::ObjectsChanged& notice) {
                return _OnStageObjectsChanged(notice);
            });

        _stageNoticeListener.SetStageLayerMutingChangedCallback(
            [this](const UsdNotice::LayerMutingChanged& notice) {
                return _OnLayerMutingChanged(notice);
            });

        _stageNoticeListener.SetStageEditTargetChangedCallback(
            [this](const UsdNotice::StageEditTargetChanged& notice) {
                return _OnStageEditTargetChanged(notice);
            });

        MayaUsdProxyStageSetNotice(*this).Send();
    }

    return MS::kSuccess;
}

MStatus MayaUsdProxyShapeBase::computeOutputTime(MDataBlock& dataBlock)
{
    MStatus     retValue = MS::kSuccess;
    MDataHandle inDataHandle = dataBlock.inputValue(timeAttr, &retValue);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    MTime inTime = inDataHandle.asTime();

    MDataHandle outDataHandle = dataBlock.outputValue(outTimeAttr, &retValue);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);
    outDataHandle.set(inTime);
    outDataHandle.setClean();

    return retValue;
}

MStatus MayaUsdProxyShapeBase::computeOutStageCacheId(MDataBlock& dataBlock)
{
    MStatus retValue = MS::kSuccess;

    MDataHandle inDataCachedHandle = dataBlock.inputValue(inStageDataCachedAttr, &retValue);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    UsdStageRefPtr usdStage;

    MayaUsdStageData* inData = dynamic_cast<MayaUsdStageData*>(inDataCachedHandle.asPluginData());
    if (inData) {
        usdStage = inData->stage;
    }

    if (!usdStage) {
        return MS::kFailure;
    }

    int  cacheId = -1;
    auto id = UsdUtilsStageCache::Get().Insert(usdStage);
    if (id)
        cacheId = id.ToLongInt();

    MDataHandle outCacheIdHandle = dataBlock.outputValue(outStageCacheIdAttr, &retValue);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    outCacheIdHandle.set(cacheId);
    outCacheIdHandle.setClean();

    return MS::kSuccess;
}

/* virtual */
bool MayaUsdProxyShapeBase::isBounded() const { return isStageValid(); }

/* virtual */
void MayaUsdProxyShapeBase::CacheEmptyBoundingBox(MBoundingBox&) { }

/* virtual */
UsdTimeCode MayaUsdProxyShapeBase::GetOutputTime(MDataBlock dataBlock) const
{
    return _GetTime(dataBlock);
}

void MayaUsdProxyShapeBase::copyInternalData(MPxNode* srcNode)
{
    MStatus retValue { MS::kSuccess };

    // get the source data block
    MayaUsdProxyShapeBase* srcProxyShapeBase = static_cast<MayaUsdProxyShapeBase*>(srcNode);
    MDataBlock             srcDataBlock = srcProxyShapeBase->forceCache();

    // ---------------------------------------------------------------------------------
    // copyInternalData is called multiple times so we do have to protect against it.
    // ---------------------------------------------------------------------------------

    // first, read the input value from "outStageDataAttr". outStageDataAttr gets computed when
    // we get the stage on the proxy. If there is no incoming data, we return right away.
    MDataHandle srcInDataCachedHandle = srcDataBlock.inputValue(outStageDataAttr, &retValue);
    if (srcInDataCachedHandle.data().isNull()) {
        return;
    }

    // query from the destination block to make sure inStageDataCachedAttr attribute is clean. If it
    // is clean that means we already have the attr value.
    MDataBlock dataBlock = forceCache();
    if (dataBlock.isClean(inStageDataCachedAttr)) {
        return;
    }

    // get the handle inDataCachedHandle and return if it doesn't have the data.
    MDataHandle inDataCachedHandle = dataBlock.outputValue(inStageDataCachedAttr, &retValue);
    if (inDataCachedHandle.data().isNull()) {
        return;
    }

    MayaUsdStageData* srcInData
        = dynamic_cast<MayaUsdStageData*>(srcInDataCachedHandle.asPluginData());
    if (!srcInData || !srcInData->stage) {
        return;
    }

    // get the pointer to source stage
    UsdStageRefPtr srcUsdStage = srcInData->stage;

    // transfer session layer
    // session layer is never shared so transfer its content always.
    SdfLayerRefPtr sessionLayer
        = SdfLayer::CreateAnonymous(kAnonymousLayerName + kSessionLayerPostfix + ".usda");
    sessionLayer->TransferContent(srcUsdStage->GetSessionLayer());

    // decide if the root layer needs to be shared or deep copied.
    SdfLayerRefPtr rootLayer;
    if (srcUsdStage->GetRootLayer()->IsAnonymous()) {
        rootLayer = SdfLayer::CreateAnonymous(kAnonymousLayerName);
        rootLayer->TransferContent(srcUsdStage->GetRootLayer());
    } else {
        rootLayer = srcUsdStage->GetRootLayer();
    }

    // create a new usd stage from the root and session layers
    UsdStageRefPtr newUsdStage
        = UsdStage::OpenMasked(rootLayer, sessionLayer, UsdStagePopulationMask::All());
    TF_VERIFY(newUsdStage);

    // handle edit target for session and root layers.
    // setting edit target for SubLayers is handled separately.
    auto srcCurrentTargetLayer = srcUsdStage->GetEditTarget().GetLayer();
    auto isSessionLayer
        = srcCurrentTargetLayer->GetIdentifier().find(kSessionLayerPostfix) != std::string::npos;
    auto isAnonymous = srcUsdStage->GetRootLayer()->IsAnonymous();
    if (isSessionLayer) {
        newUsdStage->SetEditTarget(newUsdStage->GetSessionLayer());
    } else if (!isSessionLayer && !isAnonymous) {
        newUsdStage->SetEditTarget(srcCurrentTargetLayer);
    }

    // recursively create new anon Sublayer(s) for session and root layers
    createNewAnonSubLayerRecursive(newUsdStage, srcCurrentTargetLayer, sessionLayer);
    createNewAnonSubLayerRecursive(newUsdStage, srcCurrentTargetLayer, rootLayer);

    // set the stage and primPath
    MFnPluginData pluginDataFn;
    pluginDataFn.create(MayaUsdStageData::mayaTypeId, &retValue);
    CHECK_MSTATUS(retValue);

    MayaUsdStageData* newUsdStageData
        = reinterpret_cast<MayaUsdStageData*>(pluginDataFn.data(&retValue));
    CHECK_MSTATUS(retValue);

    newUsdStageData->stage = newUsdStage;
    newUsdStageData->primPath = newUsdStage->GetPseudoRoot().GetPath();

    // mark the data clean.
    inDataCachedHandle.set(newUsdStageData);
    inDataCachedHandle.setClean();
}

/* virtual */
MBoundingBox MayaUsdProxyShapeBase::boundingBox() const
{
    TRACE_FUNCTION();

    MProfilingScope profilerScope(
        _shapeBaseProfilerCategory, MProfiler::kColorE_L3, "Compute bounding box");

    MStatus status;

    // Make sure outStage is up to date
    MayaUsdProxyShapeBase* nonConstThis = const_cast<ThisClass*>(this);
    MDataBlock             dataBlock = nonConstThis->forceCache();
    dataBlock.inputValue(outStageDataAttr, &status);
    CHECK_MSTATUS_AND_RETURN(status, MBoundingBox());

    // XXX:
    // If we could cheaply determine whether a stage only has static geometry,
    // we could make this value a constant one for that case, avoiding the
    // memory overhead of a cache entry per frame
    UsdTimeCode currTime = GetOutputTime(dataBlock);

    std::map<UsdTimeCode, MBoundingBox>::const_iterator cacheLookup
        = _boundingBoxCache.find(currTime);

    if (cacheLookup != _boundingBoxCache.end()) {
        return cacheLookup->second;
    }

    MProfilingScope profilingScope(
        _shapeBaseProfilerCategory, MProfiler::kColorB_L1, "Compute USD Stage BoundingBox");

    UsdPrim prim = _GetUsdPrim(dataBlock);
    if (!prim) {
        return MBoundingBox();
    }

    const UsdGeomImageable imageablePrim(prim);

    bool drawRenderPurpose = false;
    bool drawProxyPurpose = true;
    bool drawGuidePurpose = false;
    _GetDrawPurposeToggles(dataBlock, &drawRenderPurpose, &drawProxyPurpose, &drawGuidePurpose);

    const TfToken purpose1 = UsdGeomTokens->default_;
    const TfToken purpose2 = drawRenderPurpose ? UsdGeomTokens->render : TfToken();
    const TfToken purpose3 = drawProxyPurpose ? UsdGeomTokens->proxy : TfToken();
    const TfToken purpose4 = drawGuidePurpose ? UsdGeomTokens->guide : TfToken();

    // Compute the bound in "Usd World" space. This will apply the transform the
    // referenced prim may have relative to the root of its Usd scene
    GfBBox3d allBox
        = imageablePrim.ComputeWorldBound(currTime, purpose1, purpose2, purpose3, purpose4);

    UsdMayaUtil::AddMayaExtents(allBox, prim, currTime);

    Ufe::BBox3d pulledUfeBBox = MayaUsd::ufe::getPulledPrimsBoundingBox(ufePath());
    if (!pulledUfeBBox.empty()) {
        GfBBox3d pulledBox(GfRange3d(
            GfVec3d(pulledUfeBBox.min.x(), pulledUfeBBox.min.y(), pulledUfeBBox.min.z()),
            GfVec3d(pulledUfeBBox.max.x(), pulledUfeBBox.max.y(), pulledUfeBBox.max.z())));
        allBox = GfBBox3d::Combine(allBox, pulledBox);
    }

    MBoundingBox& retval = nonConstThis->_boundingBoxCache[currTime];

    const GfRange3d boxRange = allBox.ComputeAlignedBox();

    // Convert to GfRange3d to MBoundingBox
    if (!boxRange.IsEmpty()) {
        const GfVec3d boxMin = boxRange.GetMin();
        const GfVec3d boxMax = boxRange.GetMax();
        retval = MBoundingBox(
            MPoint(boxMin[0], boxMin[1], boxMin[2]), MPoint(boxMax[0], boxMax[1], boxMax[2]));
    } else {
        nonConstThis->CacheEmptyBoundingBox(retval);
    }

    return retval;
}

void MayaUsdProxyShapeBase::clearBoundingBoxCache() { _boundingBoxCache.clear(); }

bool MayaUsdProxyShapeBase::isStageValid() const
{
    MStatus                localStatus;
    MayaUsdProxyShapeBase* nonConstThis = const_cast<MayaUsdProxyShapeBase*>(this);
    MDataBlock             dataBlock = nonConstThis->forceCache();

    MDataHandle outDataHandle = dataBlock.inputValue(outStageDataAttr, &localStatus);
    CHECK_MSTATUS_AND_RETURN(localStatus, false);

    MayaUsdStageData* outData = dynamic_cast<MayaUsdStageData*>(outDataHandle.asPluginData());
    if (!outData || !outData->stage) {
        return false;
    }

    return true;
}

bool MayaUsdProxyShapeBase::isShareableStage() const
{
    MStatus                localStatus;
    MayaUsdProxyShapeBase* nonConstThis = const_cast<MayaUsdProxyShapeBase*>(this);
    MDataBlock             dataBlock = nonConstThis->forceCache();

    MDataHandle shareStageHandle = dataBlock.inputValue(shareStageAttr, &localStatus);

    CHECK_MSTATUS_AND_RETURN(localStatus, false);

    return shareStageHandle.asBool();
}

bool MayaUsdProxyShapeBase::isStageIncoming() const
{
    MStatus                localStatus;
    MayaUsdProxyShapeBase* nonConstThis = const_cast<MayaUsdProxyShapeBase*>(this);
    MDataBlock             dataBlock = nonConstThis->forceCache();

    MDataHandle inDataHandle = dataBlock.inputValue(inStageDataAttr, &localStatus);
    CHECK_MSTATUS_AND_RETURN(localStatus, false);

    bool isIncomingStage = false;

    // If inData has an incoming connection, then use it. Otherwise generate stage from the filepath
    if (!inDataHandle.data().isNull()) {
        isIncomingStage = true;
    } else {
        // Check if we have a Stage from the Cache Id
        const auto cacheIdNum = dataBlock.inputValue(stageCacheIdAttr, &localStatus).asInt();
        CHECK_MSTATUS_AND_RETURN(localStatus, false);
        const auto cacheId = UsdStageCache::Id::FromLongInt(cacheIdNum);
        const auto stageCached = cacheId.IsValid() && UsdUtilsStageCache::Get().Contains(cacheId);

        // Check what is the cache connected to
        MStringArray result;
        MGlobal::executeCommand(("listConnections -t shape -shapes on " + this->name()), result);

        // The stage is only incoming if the cache is connected to a shape
        if (stageCached && result.length()) {
            isIncomingStage = true;
        }
    }

    return isIncomingStage;
}

bool MayaUsdProxyShapeBase::isIncomingLayer(const std::string& layerIdentifier) const
{
    return _incomingLayers.find(layerIdentifier) != _incomingLayers.end();
}

MStatus MayaUsdProxyShapeBase::preEvaluation(
    const MDGContext&      context,
    const MEvaluationNode& evaluationNode)
{
    // Any logic here should have an equivalent implementation in
    // MayaUsdProxyShapeBase::setDependentsDirty().
    if (context.isNormal()) {
        if (evaluationNode.dirtyPlugExists(excludePrimPathsAttr)) {
            _IncreaseExcludePrimPathsVersion();
        } else if (
            evaluationNode.dirtyPlugExists(outStageDataAttr) ||
            // All the plugs that affect outStageDataAttr
            evaluationNode.dirtyPlugExists(filePathAttr)
            || evaluationNode.dirtyPlugExists(primPathAttr)
            || evaluationNode.dirtyPlugExists(loadPayloadsAttr)
            || evaluationNode.dirtyPlugExists(shareStageAttr)
            || evaluationNode.dirtyPlugExists(inStageDataAttr)
            || evaluationNode.dirtyPlugExists(stageCacheIdAttr)) {
            _IncreaseUsdStageVersion();
            MayaUsdProxyStageInvalidateNotice(*this).Send();
        }
    }

    return MPxSurfaceShape::preEvaluation(context, evaluationNode);
}

MStatus MayaUsdProxyShapeBase::postEvaluation(
    const MDGContext&      context,
    const MEvaluationNode& evaluationNode,
    PostEvaluationType     evalType)
{
    // When a node is evaluated by evaluation manager setDependentsDirty is not called. The
    // functionality in setDependentsDirty needs to be duplicated in preEvaluation or
    // postEvaluation. I don't think we need the call to setGeometryDrawDirty() in
    // setDependentsDirty, but there may be a workflow I'm not seeing that does require it. I'm
    // leaving this here commented out as a reminder that we should either have both calls to
    // setGeometryDrawDirty, or no calls to setGeometryDrawDirty.
    // MHWRender::MRenderer::setGeometryDrawDirty(thisMObject());

    InComputeGuard inComputeGuard(*this);

    if (context.isNormal() && evalType == PostEvaluationEnum::kEvaluatedDirectly) {
        MDataBlock dataBlock = forceCache();
        ProxyAccessor::syncCache(_usdAccessor, thisMObject(), dataBlock);
    }

    return MPxSurfaceShape::postEvaluation(context, evaluationNode, evalType);
}

/* virtual */
MStatus MayaUsdProxyShapeBase::setDependentsDirty(const MPlug& plug, MPlugArray& plugArray)
{
    // Any logic here should have an equivalent implementation in
    // MayaUsdProxyShapeBase::preEvaluation() or postEvaluation().

    MStatus retValue;

    // If/when the MPxDrawOverride for the proxy shape specifies
    // isAlwaysDirty=false to improve performance, we must be sure to notify
    // the Maya renderer that the geometry is dirty and needs to be redrawn
    // when any plug on the proxy shape is dirtied.
    MHWRender::MRenderer::setGeometryDrawDirty(thisMObject());

    if (plug == excludePrimPathsAttr) {
        _IncreaseExcludePrimPathsVersion();
    } else if (
        plug == outStageDataAttr ||
        // All the plugs that affect outStageDataAttr
        plug == filePathAttr || plug == primPathAttr || plug == loadPayloadsAttr
        || plug == shareStageAttr || plug == inStageDataAttr || plug == stageCacheIdAttr) {
        _IncreaseUsdStageVersion();
        MayaUsdProxyStageInvalidateNotice(*this).Send();
    }

    retValue = MPxSurfaceShape::setDependentsDirty(plug, plugArray);
    CHECK_MSTATUS_AND_RETURN_IT(retValue);

    // If accessor returns success when adding dirty plugs we have to get renderer to
    // trigger compute. We achieve it by adding timeAttr to dirty plugArray. This will guarantee
    // we don't render something that requires inputs evaluted by DG.
    if (plug == timeAttr || plug.isDynamic()) {
        if (ProxyAccessor::addDependentsDirty(_usdAccessor, plug, plugArray) == MS::kSuccess) {
            MPlug outTimePlug(thisMObject(), outTimeAttr);
            plugArray.append(outTimePlug);
        }
    }

    return retValue;
}

/* virtual */
void MayaUsdProxyShapeBase::getCacheSetup(
    const MEvaluationNode&   evalNode,
    MNodeCacheDisablingInfo& disablingInfo,
    MNodeCacheSetupInfo&     cacheSetupInfo,
    MObjectArray&            monitoredAttributes) const
{
    MPxSurfaceShape::getCacheSetup(evalNode, disablingInfo, cacheSetupInfo, monitoredAttributes);
    // We want this node to be cached by default (unless cache rules have been configured
    // to exclude it.
    cacheSetupInfo.setPreference(MNodeCacheSetupInfo::kWantToCacheByDefault, true);
}

/* virtual */
void MayaUsdProxyShapeBase::configCache(const MEvaluationNode& evalNode, MCacheSchema& schema) const
{
    MPxSurfaceShape::configCache(evalNode, schema);
    // Out time is not always a dirty plug, but time can be animated. This is why we will
    // store input time and enable quick compute within proxy shape for out time
    schema.add(timeAttr);

    if (evalNode.dirtyPlugExists(inStageDataAttr) || evalNode.dirtyPlugExists(stageCacheIdAttr)) {
        schema.add(outStageDataAttr);
    }
}

UsdPrim MayaUsdProxyShapeBase::_GetUsdPrim(MDataBlock dataBlock) const
{
    MStatus localStatus;
    UsdPrim usdPrim;

    MDataHandle outDataHandle = dataBlock.inputValue(outStageDataAttr, &localStatus);
    CHECK_MSTATUS_AND_RETURN(localStatus, usdPrim);

    MayaUsdStageData* outData = dynamic_cast<MayaUsdStageData*>(outDataHandle.asPluginData());
    if (!outData) {
        return usdPrim; // empty UsdPrim
    }

    if (!outData->stage) {
        return usdPrim; // empty UsdPrim
    }

    usdPrim = (outData->primPath.IsEmpty()) ? outData->stage->GetPseudoRoot()
                                            : outData->stage->GetPrimAtPath(outData->primPath);

    return usdPrim;
}

std::vector<std::string> MayaUsdProxyShapeBase::getMutedLayers() const
{
    MStatus           status;
    MFnDependencyNode depNode(thisMObject(), &status);
    CHECK_MSTATUS_AND_RETURN(status, std::vector<std::string>());

    std::vector<std::string> muted;
    UsdMayaWriteUtil::ReadMayaAttribute(depNode, kMutedLayersAttrName, &muted);
    return muted;
}

MStatus MayaUsdProxyShapeBase::setMutedLayers(const std::vector<std::string>& muted)
{
    MStatus           status;
    MFnDependencyNode depNode(thisMObject(), &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MPlug         mutedLayersPlug = depNode.findPlug(mutedLayersAttr, true);
    VtStringArray mutedArray(muted.begin(), muted.end());
    return UsdMayaReadUtil::SetMayaAttr(mutedLayersPlug, VtValue(mutedArray)) ? MStatus::kSuccess
                                                                              : MStatus::kFailure;
}

std::vector<std::string> MayaUsdProxyShapeBase::getLockedLayers() const
{
    MStatus           status;
    MFnDependencyNode depNode(thisMObject(), &status);
    CHECK_MSTATUS_AND_RETURN(status, std::vector<std::string>());

    std::vector<std::string> locked;
    UsdMayaWriteUtil::ReadMayaAttribute(depNode, kLockedLayersAttrName, &locked);
    return locked;
}

MStatus MayaUsdProxyShapeBase::setLockedLayers(const std::vector<std::string>& locked)
{
    MStatus           status;
    MFnDependencyNode depNode(thisMObject(), &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MPlug         lockedLayersPlug = depNode.findPlug(lockedLayersAttr, true);
    VtStringArray lockedArray(locked.begin(), locked.end());
    return UsdMayaReadUtil::SetMayaAttr(lockedLayersPlug, VtValue(lockedArray)) ? MStatus::kSuccess
                                                                                : MStatus::kFailure;
}

int MayaUsdProxyShapeBase::getComplexity() const
{
    return _GetComplexity(const_cast<MayaUsdProxyShapeBase*>(this)->forceCache());
}

int MayaUsdProxyShapeBase::_GetComplexity(MDataBlock dataBlock) const
{
    int     complexity = 0;
    MStatus status;

    complexity = dataBlock.inputValue(complexityAttr, &status).asInt();

    return complexity;
}

UsdTimeCode MayaUsdProxyShapeBase::getTime() const
{
    return _GetTime(const_cast<MayaUsdProxyShapeBase*>(this)->forceCache());
}

UsdTimeCode MayaUsdProxyShapeBase::_GetTime(MDataBlock dataBlock) const
{
    MStatus status;

    return UsdTimeCode(dataBlock.inputValue(outTimeAttr, &status).asTime().value());
}

UsdStageRefPtr MayaUsdProxyShapeBase::getUsdStage() const
{
    MStatus                localStatus;
    MayaUsdProxyShapeBase* nonConstThis = const_cast<MayaUsdProxyShapeBase*>(this);
    MDataBlock             dataBlock = nonConstThis->forceCache();

    MDataHandle outDataHandle = dataBlock.inputValue(outStageDataAttr, &localStatus);
    CHECK_MSTATUS_AND_RETURN(localStatus, UsdStageRefPtr());

    MayaUsdStageData* outData = dynamic_cast<MayaUsdStageData*>(outDataHandle.asPluginData());

    if (outData && outData->stage)
        return outData->stage;
    else
        return {};
}

size_t MayaUsdProxyShapeBase::getUsdStageVersion() const { return _UsdStageVersion; }

void MayaUsdProxyShapeBase::getDrawPurposeToggles(
    bool* drawRenderPurpose,
    bool* drawProxyPurpose,
    bool* drawGuidePurpose) const
{
    MDataBlock dataBlock = const_cast<MayaUsdProxyShapeBase*>(this)->forceCache();
    _GetDrawPurposeToggles(dataBlock, drawRenderPurpose, drawProxyPurpose, drawGuidePurpose);
}

MayaUsd::LayerManager* MayaUsdProxyShapeBase::getLayerManager()
{
    // Note: don't use the CHECK_MSTATUS macros because it is expected to not
    //       find the layer manager atribute or layer manager UUID when loading
    //       old scenes or recomputing the proxy shape after the layer manager
    //       node has been deleted. So errors in this function are not hard errors.

    MStatus localStatus;

    MDataBlock  dataBlock = forceCache();
    MDataHandle layerManagerData = dataBlock.inputValue(layerManagerAttr, &localStatus);
    if (!localStatus)
        return nullptr;

    if (layerManagerData.asString().length() <= 0)
        return nullptr;

    MUuid layerManagerUuid(layerManagerData.asString(), &localStatus);
    if (!localStatus)
        return nullptr;

    MSelectionList selection;
    localStatus = selection.add(layerManagerUuid);
    if (!localStatus)
        return nullptr;

    MObject layerManagerObj;
    localStatus = selection.getDependNode(0, layerManagerObj);
    if (!localStatus)
        return nullptr;

    MFnDependencyNode depNode;
    if (!depNode.setObject(layerManagerObj))
        return nullptr;

    if (LayerManager* layerManager = dynamic_cast<LayerManager*>(depNode.userNode()))
        return layerManager;

    return nullptr;
}

void MayaUsdProxyShapeBase::setLayerManager(MayaUsd::LayerManager* layerManager)
{
    MStatus localStatus;

    MPlug layerManagerPlug
        = MFnDependencyNode(thisMObject()).findPlug(layerManagerAttr, false, &localStatus);
    CHECK_MSTATUS(localStatus);

    if (layerManager) {
        MFnDependencyNode layerManagerDepNode(layerManager->thisMObject());
        layerManagerPlug.setString(layerManagerDepNode.uuid().asString());
    } else {
        layerManagerPlug.setString("");
    }
}

SdfPath MayaUsdProxyShapeBase::getPrimPath() const
{
    return _GetPrimPath(const_cast<MayaUsdProxyShapeBase*>(this)->forceCache());
}

SdfPathVector MayaUsdProxyShapeBase::getExcludePrimPaths() const
{
    return _GetExcludePrimPaths(const_cast<MayaUsdProxyShapeBase*>(this)->forceCache());
}

size_t MayaUsdProxyShapeBase::getExcludePrimPathsVersion() const
{
    return _excludePrimPathsVersion;
}

SdfPath MayaUsdProxyShapeBase::_GetPrimPath(MDataBlock dataBlock) const
{
    MStatus       status = MS::kFailure;
    const MString primPathStr = dataBlock.inputValue(primPathAttr, &status).asString();
    if (!status) {
        return SdfPath::EmptyPath();
    }

    if (primPathStr.length() == 0) {
        return SdfPath::AbsoluteRootPath();
    } else {
        const SdfPath path(primPathStr.asChar());
        if (path.IsAbsoluteRootOrPrimPath()) {
            return path;
        } else {
            return SdfPath::EmptyPath();
        }
    }
}

SdfPathVector MayaUsdProxyShapeBase::_GetExcludePrimPaths(MDataBlock dataBlock) const
{
    SdfPathVector ret;

    const MString excludePrimPathsStr = dataBlock.inputValue(excludePrimPathsAttr).asString();
    std::vector<std::string> excludePrimPaths = TfStringTokenize(excludePrimPathsStr.asChar(), ",");
    ret.resize(excludePrimPaths.size());
    for (size_t i = 0; i < excludePrimPaths.size(); ++i) {
        ret[i] = SdfPath(TfStringTrim(excludePrimPaths[i]));
    }

    return ret;
}

bool MayaUsdProxyShapeBase::_GetDrawPurposeToggles(
    MDataBlock dataBlock,
    bool*      drawRenderPurpose,
    bool*      drawProxyPurpose,
    bool*      drawGuidePurpose) const
{
    MStatus status;

    MDataHandle drawRenderPurposeHandle = dataBlock.inputValue(drawRenderPurposeAttr, &status);
    CHECK_MSTATUS_AND_RETURN(status, false);

    MDataHandle drawProxyPurposeHandle = dataBlock.inputValue(drawProxyPurposeAttr, &status);
    CHECK_MSTATUS_AND_RETURN(status, false);

    MDataHandle drawGuidePurposeHandle = dataBlock.inputValue(drawGuidePurposeAttr, &status);
    CHECK_MSTATUS_AND_RETURN(status, false);

    if (drawRenderPurpose) {
        *drawRenderPurpose = drawRenderPurposeHandle.asBool();
    }
    if (drawProxyPurpose) {
        *drawProxyPurpose = drawProxyPurposeHandle.asBool();
    }
    if (drawGuidePurpose) {
        *drawGuidePurpose = drawGuidePurposeHandle.asBool();
    }

    return true;
}

bool MayaUsdProxyShapeBase::GetAllRenderAttributes(
    UsdPrim*       usdPrimOut,
    SdfPathVector* excludePrimPathsOut,
    int*           complexityOut,
    UsdTimeCode*   timeOut,
    bool*          drawRenderPurpose,
    bool*          drawProxyPurpose,
    bool*          drawGuidePurpose)
{
    MDataBlock dataBlock = forceCache();

    *usdPrimOut = _GetUsdPrim(dataBlock);
    if (!usdPrimOut->IsValid()) {
        return false;
    }

    *excludePrimPathsOut = _GetExcludePrimPaths(dataBlock);
    *complexityOut = _GetComplexity(dataBlock);
    *timeOut = _GetTime(dataBlock);

    _GetDrawPurposeToggles(dataBlock, drawRenderPurpose, drawProxyPurpose, drawGuidePurpose);

    return true;
}

/* virtual */
UsdPrim MayaUsdProxyShapeBase::usdPrim() const
{
    return _GetUsdPrim(const_cast<MayaUsdProxyShapeBase*>(this)->forceCache());
}

MDagPath MayaUsdProxyShapeBase::parentTransform()
{
    MFnDagNode fn(thisMObject());
    MDagPath   proxyTransformPath;
    fn.getPath(proxyTransformPath);
    proxyTransformPath.pop();
    return proxyTransformPath;
}

/*static*/
MayaUsd::MayaNodeTypeObserver& MayaUsdProxyShapeBase::getProxyShapesObserver()
{
    static MayaUsd::MayaNodeTypeObserver observer(MayaUsdProxyShapeBase::typeName);
    return observer;
}

MayaUsdProxyShapeBase::MayaUsdProxyShapeBase(
    const bool enableUfeSelection,
    const bool useLoadRulesHandling)
    : MPxSurfaceShape()
    , _isUfeSelectionEnabled(enableUfeSelection)
    , _previousShareMode(ShareMode::Unknown)
    , _anonymousRootLayer(nullptr)
    , _unsharedStageSessionLayer(nullptr)
    , _unsharedStageRootLayer(nullptr)
    , _unsharedStageRootSublayers()
    , _incomingLayers()
{
    g_proxyShapeInstancesCount += 1;

    TfRegistryManager::GetInstance().SubscribeTo<MayaUsdProxyShapeBase>();

    if (useLoadRulesHandling) {
        // Register with the load-rules handling used to transfer load rules
        // between the USD stage and a dynamic attribute on the proxy shape.
        MayaUsd::MayaUsdProxyShapeStageExtraData::addProxyShape(*this);
    }
}

/* virtual */
MayaUsdProxyShapeBase::~MayaUsdProxyShapeBase()
{
    if (_preSaveCallbackId != 0) {
        MMessage::removeCallback(_preSaveCallbackId);
        _preSaveCallbackId = 0;
    }

    // Note: the addObservedNode was done in the postConstructor.
    //       Removing a node that was not added is a safe no-op.
    //
    //       Also, removing the observed node automatically gets rid
    //       of its listeners, so we don't have to call removeListener.
    MayaUsd::MayaNodeTypeObserver& shapeObserver = getProxyShapesObserver();
    shapeObserver.removeObservedNode(thisMObject());

    // Deregister from the load-rules handling used to transfer load rules
    // between the USD stage and a dynamic attribute on the proxy shape.
    MayaUsd::MayaUsdProxyShapeStageExtraData::removeProxyShape(*this);

    g_proxyShapeInstancesCount -= 1;
}

MSelectionMask MayaUsdProxyShapeBase::getShapeSelectionMask() const
{
    // The intent of this function is to control whether this object is
    // selectable at all in VP2

    // However, due to a bug / quirk, it could be used to specifically control
    // whether the object was SOFT-selectable if you were using
    // MAYA_VP2_USE_VP1_SELECTON; in this mode, this setting is NOT querierd
    // when doing "normal" selection, but IS queried when doing soft
    // selection.

    // Unfortunately, it is queried for both "normal" selection AND soft
    // selection if you are using "true" VP2 selection.  So in order to
    // control soft selection, in both modes, we keep track of whether
    // we currently have object soft-select enabled, and then return an empty
    // selection mask if it is, but this object is set to be non-soft-selectable

    static const MSelectionMask emptyMask;
    static const MSelectionMask normalMask(MSelectionMask::kSelectMeshes);

    if (GetObjectSoftSelectEnabled() && !canBeSoftSelected()) {
        // Disable selection, to disable soft-selection
        return emptyMask;
    }
    return normalMask;
}

bool MayaUsdProxyShapeBase::canBeSoftSelected() const { return false; }

void MayaUsdProxyShapeBase::_OnStageContentsChanged(const UsdNotice::StageContentsChanged& notice)
{
    // If the USD stage this proxy represents changes without Maya's knowledge,
    // we need to inform Maya that the shape is dirty and needs to be redrawn.
    MHWRender::MRenderer::setGeometryDrawDirty(thisMObject());
}

void MayaUsdProxyShapeBase::_OnLayerMutingChanged(const UsdNotice::LayerMutingChanged& notice)
{
    const auto stage = getUsdStage();
    if (!stage)
        return;

    // We're in a callback so undo items cannot be registered into an undoable command.
    // Mute the undo/redo for commands called from here.
    MayaUsd::OpUndoItemMuting muting;
    copyLayerMutingToAttribute(*stage, *this);
}

void MayaUsdProxyShapeBase::_OnStageEditTargetChanged(
    const UsdNotice::StageEditTargetChanged& notice)
{
    const auto stage = notice.GetStage();
    if (!stage)
        return;

    const PXR_NS::UsdEditTarget target = stage->GetEditTarget();
    if (!target.IsValid())
        return;

    const PXR_NS::SdfLayerHandle& layer = target.GetLayer();
    if (!layer)
        return;

    _targetLayer = layer;
}

void MayaUsdProxyShapeBase::_OnStageObjectsChanged(const UsdNotice::ObjectsChanged& notice)
{
    MProfilingScope profilingScope(
        _shapeBaseProfilerCategory, MProfiler::kColorB_L1, "Process USD objects changed");

    switch (UsdMayaStageNoticeListener::ClassifyObjectsChanged(notice)) {
    case UsdMayaStageNoticeListener::ChangeType::kIgnored: return;
    case UsdMayaStageNoticeListener::ChangeType::kResync: ++_UsdStageResyncCounter;
    // [[fallthrough]]; // We want that fallthrough to have the update always triggered.
    case UsdMayaStageNoticeListener::ChangeType::kUpdate: ++_UsdStageUpdateCounter; break;
    }

    // This will definitely force a BBox recomputation on "Frame All" or when framing a selected
    // stage. Computing bounds in USD is expensive, so if it pops up in other frequently used
    // scenarios we will have to investigate ways to make this cache clearing less expensive.
    clearBoundingBoxCache();

    ProxyAccessor::stageChanged(_usdAccessor, thisMObject(), notice);
    MayaUsdProxyStageObjectsChangedNotice(*this, notice).Send();

    // Recompute the extents of any UsdGeomBoundable that has authored extents
    const auto& stage = notice.GetStage();
    if (stage != getUsdStage()) {
        TF_CODING_ERROR("We shouldn't be receiving notification for other stages than one "
                        "returned by stage provider");
        return;
    }

    for (const auto& changedPath : notice.GetChangedInfoOnlyPaths()) {
        if (!changedPath.IsPrimPropertyPath()) {
            continue;
        }

        const TfToken& changedPropertyToken = changedPath.GetNameToken();
        if (changedPropertyToken == UsdGeomTokens->extent) {
            continue;
        }

        SdfPath          changedPrimPath = changedPath.GetPrimPath();
        const UsdPrim&   changedPrim = stage->GetPrimAtPath(changedPrimPath);
        UsdGeomBoundable boundableObj = UsdGeomBoundable(changedPrim);
        if (!boundableObj) {
            continue;
        }

        // If the attribute is not part of the primitive schema, it does not affect extents
#if PXR_VERSION < 2308
        auto attrDefn
            = changedPrim.GetPrimDefinition().GetSchemaAttributeSpec(changedPropertyToken);
#else
        auto attrDefn
            = changedPrim.GetPrimDefinition().GetAttributeDefinition(changedPropertyToken);
#endif
        if (!attrDefn) {
            continue;
        }

        // Ignore all attributes known to GPrim and its base classes as they
        // are guaranteed not to affect extents:
        static const std::unordered_set<TfToken, TfToken::HashFunctor> ignoredAttributes(
            UsdGeomGprim::GetSchemaAttributeNames(true).cbegin(),
            UsdGeomGprim::GetSchemaAttributeNames(true).cend());
        if (ignoredAttributes.count(changedPropertyToken) > 0) {
            continue;
        }

        UsdAttribute extentsAttr = boundableObj.GetExtentAttr();
        if (extentsAttr.GetNumTimeSamples() > 0) {
            TF_CODING_ERROR(
                "Can not fix animated extents of %s made dirty by a change on %s.",
                changedPrimPath.GetString().c_str(),
                changedPropertyToken.GetText());
            continue;
        }
        if (extentsAttr && extentsAttr.HasValue()) {
            VtVec3fArray extent(2);
            if (UsdGeomBoundable::ComputeExtentFromPlugins(
                    boundableObj, UsdTimeCode::Default(), &extent)) {
                extentsAttr.Set(extent);
            }
        }
    }
}

bool MayaUsdProxyShapeBase::closestPoint(
    const MPoint&  raySource,
    const MVector& rayDirection,
    MPoint&        theClosestPoint,
    MVector&       theClosestNormal,
    bool /*findClosestOnMiss*/,
    double /*tolerance*/)
{
    MProfilingScope profilerScope(
        _shapeBaseProfilerCategory, MProfiler::kColorE_L3, "Compute closest point");

    if (_sharedClosestPointDelegate) {
        GfRay ray(
            GfVec3d(raySource.x, raySource.y, raySource.z),
            GfVec3d(rayDirection.x, rayDirection.y, rayDirection.z));
        GfVec3d hitPoint;
        GfVec3d hitNorm;
        if (_sharedClosestPointDelegate(*this, ray, &hitPoint, &hitNorm)) {
            theClosestPoint = MPoint(hitPoint[0], hitPoint[1], hitPoint[2]);
            theClosestNormal = MVector(hitNorm[0], hitNorm[1], hitNorm[2]);
            return true;
        }
    }

    return false;
}

bool MayaUsdProxyShapeBase::canMakeLive() const { return (bool)_sharedClosestPointDelegate; }

void MayaUsdProxyShapeBase::processPlugDirty(
    MObject& /*observedNode*/,
    MObject& /*dirtiedNode*/,
    MPlug& plug,
    bool   pathChanged)
{
    // Some ancestor plugs affect proxy accessor plugs connected to EditAsMaya primitives
    // (like 'combinedVisibility'). Filter those and trigger proxy accessor recomputation.
    // Also make sure the stage is clean to prevent its wrong validation
    // inside ProxyAccessor::collectAccessorItems
    const auto plugName = plug.partialName();
    const bool isAffecting = (pathChanged || plugName == "v" || plugName == "lodv");
    if (isAffecting && forceCache().isClean(outStageDataAttr))
        ProxyAccessor::forceCompute(_usdAccessor, thisMObject());
}

Ufe::Path MayaUsdProxyShapeBase::ufePath() const
{
    // Build a path segment to proxyShape
    MDagPath thisPath;
    MDagPath::getAPathTo(thisMObject(), thisPath);

    return Ufe::PathString::path(thisPath.fullPathName().asChar());
}

std::atomic<int> MayaUsdProxyShapeBase::in_compute { 0 };

PXR_NAMESPACE_CLOSE_SCOPE
