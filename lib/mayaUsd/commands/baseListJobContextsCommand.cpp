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
#include "baseListJobContextsCommand.h"

#include <mayaUsd/fileio/jobContextRegistry.h>
#include <mayaUsd/fileio/jobs/jobArgs.h>
#include <mayaUsd/utils/util.h>

#include <maya/MArgDatabase.h>
#include <maya/MArgList.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MSyntax.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAUSD_NS_DEF {

namespace {

const UsdMayaJobContextRegistry::ContextInfo&
_GetInfo(const MArgDatabase& argData, const char* optionName)
{
    static const UsdMayaJobContextRegistry::ContextInfo emptyInfo;
    MString                                             contextName;
    MStatus status = argData.getFlagArgument(optionName, 0, contextName);
    if (status != MS::kSuccess) {
        return emptyInfo;
    }
    for (auto const& c : UsdMayaJobContextRegistry::ListJobContexts()) {
        auto const& info = UsdMayaJobContextRegistry::GetJobContextInfo(c);
        if (info.niceName == contextName.asChar()) {
            return info;
        }
    }
    return emptyInfo;
}

MString convertDictionaryToText(const VtDictionary& settings)
{
    // Would be nice to return a Python dictionary, but we need something MEL-compatible
    // Use the JobContextRegistry Python wrappers to get a dictionary.
    std::ostringstream optionsStream;
    for (const std::pair<std::string, VtValue> keyValue : settings) {

        bool        canConvert;
        std::string valueStr;
        std::tie(canConvert, valueStr) = UsdMayaUtil::ValueToArgument(keyValue.second);
        // Options don't handle empty arrays well preventing users from passing actual
        // values for options with such default value.
        if (canConvert && valueStr != "[]") {
            optionsStream << keyValue.first.c_str() << "=" << valueStr.c_str() << ";";
        }
    }
    return optionsStream.str().c_str();
}

const char* _exportStr = "export";
const char* _exportAnnotationStr = "exportAnnotation";
const char* _exportArgumentsStr = "exportArguments";
const char* _hasExportUIStr = "hasExportUI";
const char* _showExportUIStr = "showExportUI";
const char* _hasImportUIStr = "hasImportUI";
const char* _showImportUIStr = "showImportUI";
const char* _importStr = "import";
const char* _importAnnotationStr = "importAnnotation";
const char* _importArgumentsStr = "importArguments";
const char* _jobContextStr = "jobContext";
} // namespace

MStatus MayaUSDListJobContextsCommand::doIt(const MArgList& args)
{
    MStatus      status;
    MArgDatabase argData(syntax(), args, &status);

    if (status != MS::kSuccess) {
        return status;
    }

    if (argData.isFlagSet(_exportStr)) {
        for (auto const& c : UsdMayaJobContextRegistry::ListJobContexts()) {
            auto const& info = UsdMayaJobContextRegistry::GetJobContextInfo(c);
            if (info.exportEnablerCallback) {
                appendToResult(info.niceName.GetText());
            }
        }
    } else if (argData.isFlagSet(_exportAnnotationStr)) {
        auto const& info = _GetInfo(argData, _exportAnnotationStr);
        if (!info.jobContext.IsEmpty()) {
            setResult(info.exportDescription.GetText());
        }
    } else if (argData.isFlagSet(_exportArgumentsStr)) {
        auto const& info = _GetInfo(argData, _exportArgumentsStr);
        if (info.exportEnablerCallback) {
            setResult(convertDictionaryToText(info.exportEnablerCallback()));
        }
    } else if (argData.isFlagSet(_hasExportUIStr)) {
        auto const& info = _GetInfo(argData, _hasExportUIStr);
        setResult(bool(info.exportUICallback != nullptr));
    } else if (argData.isFlagSet(_showExportUIStr)) {
        auto const& info = _GetInfo(argData, _showExportUIStr);
        if (!info.exportUICallback)
            return MS::kInvalidParameter;

        MString parentUIStr;
        if (argData.getFlagArgument(_showExportUIStr, 1, parentUIStr) != MS::kSuccess)
            return MS::kInvalidParameter;

        MString settingsStr;
        if (argData.getFlagArgument(_showExportUIStr, 2, settingsStr) != MS::kSuccess)
            return MS::kInvalidParameter;

        VtDictionary inputSettings;
        if (UsdMayaJobExportArgs::GetDictionaryFromEncodedOptions(settingsStr, &inputSettings)
            != MS::kSuccess)
            return MS::kInvalidParameter;

        setResult(convertDictionaryToText(
            info.exportUICallback(info.jobContext, parentUIStr.asChar(), inputSettings)));
    } else if (argData.isFlagSet(_hasImportUIStr)) {
        auto const& info = _GetInfo(argData, _hasImportUIStr);
        setResult(bool(info.importUICallback != nullptr));
    } else if (argData.isFlagSet(_showImportUIStr)) {
        auto const& info = _GetInfo(argData, _showImportUIStr);
        if (!info.importUICallback)
            return MS::kInvalidParameter;

        MString parentUIStr;
        if (argData.getFlagArgument(_showImportUIStr, 1, parentUIStr) != MS::kSuccess)
            return MS::kInvalidParameter;

        MString settingsStr;
        if (argData.getFlagArgument(_showImportUIStr, 2, settingsStr) != MS::kSuccess)
            return MS::kInvalidParameter;

        VtDictionary inputSettings;
        if (UsdMayaJobImportArgs::GetDictionaryFromEncodedOptions(settingsStr, &inputSettings)
            != MS::kSuccess)
            return MS::kInvalidParameter;

        setResult(convertDictionaryToText(
            info.importUICallback(info.jobContext, parentUIStr.asChar(), inputSettings)));
    } else if (argData.isFlagSet(_importStr)) {
        for (auto const& c : UsdMayaJobContextRegistry::ListJobContexts()) {
            auto const& info = UsdMayaJobContextRegistry::GetJobContextInfo(c);
            if (info.importEnablerCallback) {
                appendToResult(info.niceName.GetText());
            }
        }
    } else if (argData.isFlagSet(_importAnnotationStr)) {
        auto const& info = _GetInfo(argData, _importAnnotationStr);
        if (!info.jobContext.IsEmpty()) {
            setResult(info.importDescription.GetText());
        }
    } else if (argData.isFlagSet(_importArgumentsStr)) {
        auto const& info = _GetInfo(argData, _importArgumentsStr);
        if (info.importEnablerCallback) {
            setResult(convertDictionaryToText(info.importEnablerCallback()));
        }
    } else if (argData.isFlagSet(_jobContextStr)) {
        auto const& info = _GetInfo(argData, _jobContextStr);
        if (!info.jobContext.IsEmpty()) {
            setResult(info.jobContext.GetText());
        }
    }

    return MS::kSuccess;
}

MSyntax MayaUSDListJobContextsCommand::createSyntax()
{
    MSyntax syntax;
    syntax.addFlag("-ex", "-export", MSyntax::kNoArg);
    syntax.addFlag("-ea", "-exportAnnotation", MSyntax::kString);
    syntax.addFlag("-eg", "-exportArguments", MSyntax::kString);
    syntax.addFlag("-heu", "-hasExportUI", MSyntax::kString);
    syntax.addFlag("-seu", "-showExportUI", MSyntax::kString, MSyntax::kString, MSyntax::kString);
    syntax.addFlag("-hiu", "-hasImportUI", MSyntax::kString);
    syntax.addFlag("-siu", "-showImportUI", MSyntax::kString, MSyntax::kString, MSyntax::kString);
    syntax.addFlag("-im", "-import", MSyntax::kNoArg);
    syntax.addFlag("-ia", "-importAnnotation", MSyntax::kString);
    syntax.addFlag("-ig", "-importArguments", MSyntax::kString);
    syntax.addFlag("-jc", "-jobContext", MSyntax::kString);

    syntax.enableQuery(false);
    syntax.enableEdit(false);

    return syntax;
}

void* MayaUSDListJobContextsCommand::creator() { return new MayaUSDListJobContextsCommand(); }

} // namespace MAYAUSD_NS_DEF
