//
// Copyright 2016 Pixar
// Copyright 2019 Autodesk
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
#include "importTranslator.h"

#include <mayaUsd/fileio/jobs/jobArgs.h>
#include <mayaUsd/fileio/jobs/readJob.h>
#include <mayaUsd/fileio/shading/shadingModeRegistry.h>
#include <mayaUsd/fileio/jobs/writeJob.h>

#include "pxr/base/gf/interval.h"
#include "pxr/base/vt/dictionary.h"

#include <maya/MFileObject.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>

#include <map>
#include <string>


PXR_NAMESPACE_OPEN_SCOPE


/* static */
void*
UsdMayaImportTranslator::creator()
{
    return new UsdMayaImportTranslator();
}

UsdMayaImportTranslator::UsdMayaImportTranslator() : MPxFileTranslator()
{
}

/* virtual */
UsdMayaImportTranslator::~UsdMayaImportTranslator()
{
}

/* virtual */
MStatus
UsdMayaImportTranslator::reader(
        const MFileObject& file,
        const MString& optionsString,
        MPxFileTranslator::FileAccessMode  /*mode*/)
{
    std::string fileName(file.fullName().asChar(), file.fullName().length());
    std::string primPath("/");
    std::map<std::string, std::string> variants;

    bool readAnimData = true;
    bool useCustomFrameRange = false;
    GfInterval timeInterval(1.0, 1.0);

    VtDictionary userArgs;

    if (optionsString.length() > 0) {
        MStringArray optionList;
        MStringArray theOption;
        optionsString.split(';', optionList);
        for(int i=0, n=optionList.length(); i<n; ++i) {
            theOption.clear();
            optionList[i].split('=', theOption);
            if (theOption.length() != 2) {
                continue;
            }

            std::string argName(theOption[0].asChar(), theOption[0].length());
            if (argName == "readAnimData") {
                readAnimData = (theOption[1].asInt() != 0);
            } else if (argName == "useCustomFrameRange") {
                useCustomFrameRange = (theOption[1].asInt() != 0);
            } else if (argName == "startTime") {
                timeInterval.SetMin(theOption[1].asDouble());
            } else if (argName == "endTime") {
                timeInterval.SetMax(theOption[1].asDouble());
            } else {
                userArgs[argName] =
                    UsdMayaUtil::ParseArgumentValue(
                        argName,
                        theOption[1].asChar(),
                        UsdMayaJobImportArgs::GetDefaultDictionary());
            }
        }
    }

    if (readAnimData) {
        if (!useCustomFrameRange) {
            timeInterval = GfInterval::GetFullInterval();
        }
    }
    else {
        timeInterval = GfInterval();
    }

    UsdMayaJobImportArgs jobArgs =
        UsdMayaJobImportArgs::CreateFromDictionary(
            userArgs,
            /* importWithProxyShapes = */ false,
            timeInterval);
    UsdMaya_ReadJob mUsdReadJob(fileName, primPath, variants, jobArgs);
    std::vector<MDagPath> addedDagPaths;
    bool success = mUsdReadJob.Read(&addedDagPaths);
    return (success) ? MS::kSuccess : MS::kFailure;
}

/* virtual */
MPxFileTranslator::MFileKind
UsdMayaImportTranslator::identifyFile(
        const MFileObject& file,
        const char*  /*buffer*/,
        short  /*size*/) const
{
    MFileKind retValue = kNotMyFileType;
    const MString fileName = file.fullName();
    const int lastIndex = fileName.length() - 1;

    const int periodIndex = fileName.rindex('.');
    if (periodIndex < 0 || periodIndex >= lastIndex) {
        return retValue;
    }

    const MString fileExtension = fileName.substring(periodIndex + 1, lastIndex);

    if (fileExtension == UsdMayaTranslatorTokens->UsdFileExtensionDefault.GetText() ||
        fileExtension == UsdMayaTranslatorTokens->UsdFileExtensionASCII.GetText() ||
        fileExtension == UsdMayaTranslatorTokens->UsdFileExtensionCrate.GetText()) {
        retValue = kIsMyFileType;
    }

    return retValue;
}

/* static */
const std::string&
UsdMayaImportTranslator::GetDefaultOptions()
{
    static std::string defaultOptions;
    static std::once_flag once;
    std::call_once(once, []() {
        std::vector<std::string> entries;
        for (const std::pair<std::string, VtValue> keyValue :
                UsdMayaJobImportArgs::GetDefaultDictionary()) {
            if (keyValue.second.IsHolding<bool>()) {
                entries.push_back(TfStringPrintf("%s=%d",
                        keyValue.first.c_str(),
                        static_cast<int>(keyValue.second.Get<bool>())));
            }
            else if (keyValue.second.IsHolding<std::string>()) {
                entries.push_back(TfStringPrintf("%s=%s",
                        keyValue.first.c_str(),
                        keyValue.second.Get<std::string>().c_str()));
            }
        }
        entries.push_back("readAnimData=0");
        entries.push_back("useCustomFrameRange=0");
        defaultOptions = TfStringJoin(entries, ";");
    });

    return defaultOptions;
}


PXR_NAMESPACE_CLOSE_SCOPE
