//
// Copyright 2025 Autodesk, Inc. All rights reserved.
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
#ifndef PXRUSDMAYA_SPLINEUTILS_H
#define PXRUSDMAYA_SPLINEUTILS_H
#include <pxr/pxr.h>

#if PXR_VERSION >= 2411

#include <mayaUsd/base/api.h>
#include <mayaUsd/fileio/primReaderContext.h>
#include <mayaUsd/fileio/utils/writeUtil.h>
#include <mayaUsd/utils/util.h>

#include <pxr/base/tf/type.h>
#include <pxr/base/ts/knot.h>
#include <pxr/base/ts/spline.h>
#include <pxr/base/ts/tangentConversions.h>
#include <pxr/base/ts/types.h>

#include <maya/MDistance.h>
#include <maya/MDoubleArray.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

#include <cmath>

PXR_NAMESPACE_OPEN_SCOPE

/// This struct contains helpers for writing USD (thus reading Maya data).
struct UsdMayaSplineUtils
{
    /**
     * @brief Extracts knot data from a Maya animation curve and converts it into a USD knot map.
     *
     * This function retrieves the animation curve associated with a specified Maya attribute,
     * processes its keyframes, and converts the tangent and value data into a USD knot
     * map.
     *
     * Template parameters:
     * T - The type of the value stored in the knot tangent (e.g., float, double).
     * V - The type of the value stored in the knot value (e.g., float, double).
     *
     * @param depNode The Maya dependency node containing the attribute.
     * @param name The name of the Maya attribute to retrieve the animation curve from.
     * @param scaling A scaling factor applied to the values extracted from the curve (default
     * is 1.0).
     * @return TsKnotMap A USD knot map containing the processed keyframe data from the Maya
     * animation curve.
     */
    template <typename T, typename V = T>
    static TsKnotMap GetKnotsFromMayaCurve(
        const MFnDependencyNode& depNode,
        const MString&           name,
        float                    scaling = 1.f)
    {
        TsKnotMap knots;
        auto      valueType = TfType::Find<T>();

        MStatus status;
        depNode.attribute(name, &status);
        CHECK_MSTATUS_AND_RETURN(status, knots)
        MPlug plug = depNode.findPlug(name, true, &status);
        CHECK_MSTATUS_AND_RETURN(status, knots)

        // get the animation curve for the given maya attribute if there's one
        MFnAnimCurve flAnimCurve(plug, &status);
        if (status != MStatus::kSuccess) {
            return knots;
        }

        auto numKeys = flAnimCurve.numKeys();
        for (unsigned int k = 0; k < numKeys; ++k) {
            auto time = flAnimCurve.time(k).value();

            V      value = static_cast<T>(flAnimCurve.value(k));
            MTime  convert(1.0, MTime::kSeconds);
            double inTangentX {}, outTangentX {};
            double inTangentY {}, outTangentY {};
            flAnimCurve.getTangent(k, inTangentX, inTangentY, true);
            flAnimCurve.getTangent(k, outTangentX, outTangentY, false);

            // This was taken from the .getTangent() docs:
            // Need to multiply the value with the time unit conversion factor
            inTangentX *= convert.as(MTime::uiUnit());
            outTangentX *= convert.as(MTime::uiUnit());

            TsTime inTime {}, outTime {};
            T      inSlope {}, outSlope {};

            // Converting from maya tangent to standard (Usd) tangent:
            // Usd tangents are specified by slope and length and Slopes are "rise over run": height
            // divided by length.
            // Maya tangents are specified by height and length. Height and length
            // are both specified multiplied by 3 Heights are positive for upward-sloping
            // post-tangents, and negative for upward-sloping pre-tangents.
            TsConvertToStandardTangent(
                inTangentX, T(inTangentY), true, true, true, &inTime, &inSlope);

            if (std::isnan(inSlope)) {
                inSlope = T(0);
            }

            TsKnot     knot(valueType);
            const auto outTanType = flAnimCurve.outTangentType(k);
            const auto convertedValue = value * scaling;

            // Deal with the case where slope would be infinite,
            // Because when there's a step on the curve is discontinuous
            if (outTanType == MFnAnimCurve::kTangentStepNext) {
                // Maya's step next is a special case where the value jumps to the next key's value.
                // If this is the last key, then set the value to the current value, making it
                // behave like a step.
                knot.SetPreValue(convertedValue);
                if (k + 1 < numKeys) {
                    knot.SetValue(static_cast<V>(flAnimCurve.value(k + 1)) * scaling);
                } else {
                    knot.SetValue(convertedValue);
                }
            } else if (outTanType == MFnAnimCurve::kTangentStep) {
                // no need to convert tangent in this case because it is 0 for the step.
                knot.SetValue(value * scaling);
            } else {
                TsConvertToStandardTangent(
                    outTangentX, T(outTangentY), true, true, false, &outTime, &outSlope);
                if (std::isnan(outSlope)) {
                    outSlope = T(0);
                }
                knot.SetValue(value * scaling);
            }

            knot.SetTime(time);
            knot.SetPostTanSlope(outSlope);
            knot.SetPreTanSlope(inSlope);
            knot.SetPostTanWidth(outTime);
            knot.SetPreTanWidth(inTime);
            knot.SetNextInterpolation(_ConvertMayaTanTypeToUsdTanType(outTanType));

            knots.insert(knot);
        }

        return knots;
    }

    /**
     * @brief Retrieves a USD spline from a Maya curve attribute.
     *
     * This function extracts the spline data from a specified Maya attribute and converts it into
     * a USD spline. The USD spline includes pre- and post-extrapolation settings based on the Maya
     * curve's infinity types.
     *
     * @param depNode The Maya dependency node containing the attribute.
     * @param name The name of the Maya attribute to retrieve the spline data from.
     * @return TsSpline The USD spline created from the Maya curve attribute.
     */
    template <typename T>
    static TsSpline GetSplineFromMayaCurve(const MFnDependencyNode& depNode, const MString& name)
    {
        auto spline = TsSpline(TfType::Find<T>());

        MStatus status;
        depNode.attribute(name, &status);
        CHECK_MSTATUS_AND_RETURN(status, spline)
        MPlug plug = depNode.findPlug(name, true, &status);
        CHECK_MSTATUS_AND_RETURN(status, spline)

        // get the animation curve for the given maya attribute
        MFnAnimCurve flAnimCurve(plug, &status);
        CHECK_MSTATUS_AND_RETURN(status, spline)

        TsExtrapolation preExtrapolation(
            _ConvertUsdExtrapolationToMaya(flAnimCurve.preInfinityType()));
        TsExtrapolation postExtrapolation(
            _ConvertUsdExtrapolationToMaya(flAnimCurve.postInfinityType()));
        spline.SetPreExtrapolation(preExtrapolation);
        spline.SetPostExtrapolation(postExtrapolation);

        return spline;
    }

    template <typename T>
    static bool WriteUsdSplineToPlug(
        MPlug&                          plug,
        TsSpline                        spline,
        class UsdMayaPrimReaderContext* context,
        const MDistance::Unit           convertToUnit = MDistance::kMillimeters)
    {
        return WriteUsdSplineToPlug(plug, spline, context, TfType::Find<T>(), convertToUnit);
    }

    /**
     * @brief Writes a USD spline to a Maya plug.
     *
     * This function converts a USD spline into a Maya animation curve and writes it to the
     * specified plug. It handles tangent conversion, knot mapping, and unit conversion as needed.
     *
     * @param plug The Maya plug where the animation curve will be written.
     * @param spline The USD spline containing the knot data to be converted.
     * @param context The context used for undo/redo operations (optional).
     * @param valueType The type of values stored in the spline.
     * @param convertToUnit The unit to which the values should be converted (default is
     * millimeters).
     * @return bool Returns true if the spline was successfully written to the plug, false
     * otherwise.
     */
    MAYAUSD_CORE_PUBLIC static bool WriteUsdSplineToPlug(
        MPlug&                          plug,
        const TsSpline&                 spline,
        class UsdMayaPrimReaderContext* context,
        const TfType&                   valueType,
        const MDistance::Unit           convertToUnit = MDistance::kMillimeters);

    /**
     * @brief Writes a Maya spline attribute to a USD attribute.
     *
     * This function retrieves the knots and spline data from a Maya attribute and writes them to
     * the corresponding USD attribute. If the Maya attribute does not have a spline, it writes the
     * constant value instead.
     *
     * Template parameters:
     * T - The type of the value stored in the knot tangent (e.g., float, double).
     * V - The type of the value stored in the knot value (e.g., float, double).
     *
     * @param depNode The Maya dependency node containing the attribute.
     * @param prim The USD primitive to which the attribute will be written.
     * @param mayaAttrName The name of the Maya attribute to retrieve the spline data from.
     * @param usdAttrName The name of the USD attribute to write the spline data to.
     * @return bool Returns true if the attribute was successfully written, false otherwise.
     */
    template <typename T, typename V = T>
    static bool WriteSplineAttribute(
        const MFnDependencyNode& depNode,
        const UsdPrim&           prim,
        const std::string&       mayaAttrName,
        const TfToken&           usdAttrName)
    {
        auto usdAttr = prim.GetAttribute(usdAttrName);
        if (!usdAttr) {
            return false;
        }

        TsKnotMap knots
            = UsdMayaSplineUtils::GetKnotsFromMayaCurve<T, V>(depNode, mayaAttrName.c_str());
        if (knots.empty()) {
            MStatus status;
            auto    plug = depNode.findPlug(mayaAttrName.c_str(), true, &status);
            V       val;
            plug.getValue(val);
            if (UsdMayaWriteUtil::SetAttribute(usdAttr, val, UsdTimeCode::Default())) {
                return true;
            }

            return false;
        }

        TsSpline spline
            = UsdMayaSplineUtils::GetSplineFromMayaCurve<T>(depNode, mayaAttrName.c_str());
        spline.SetKnots(knots);

        if (!usdAttr.SetSpline(spline)) {
            return false;
        }

        return true;
    }

    /**
     * @brief Combines two Maya curves into a single USD spline by applying a lambda function to
     * their values.
     *
     * This function retrieves spline data from two Maya attributes, applies a user-defined lambda
     * function to combine their values, and returns the resulting USD spline. If one of the
     * attributes does not have a curve, the constant value from the plug is used instead. If both
     * attributes lack curves, an empty spline is returned.
     *
     * @param depNode The Maya dependency node containing the attributes.
     * @param attrName1 The name of the first Maya attribute.
     * @param attrName2 The name of the second Maya attribute.
     * @param lambda A function that takes two values (from the two attributes) and
     * returns the computed value.
     * @return TsSpline The resulting USD spline after combining the values of the two Maya
     * attributes.
     */
    template <typename T>
    static TsSpline CombineMayaCurveToUsdSpline(
        const MFnDependencyNode&      depNode,
        const MString&                attrName1,
        const MString&                attrName2,
        const std::function<T(T, T)>& lambda)
    {
        // Retrieve spline for the first attribute
        TsSpline  spline1 = GetSplineFromMayaCurve<T>(depNode, attrName1);
        TsKnotMap knots = GetKnotsFromMayaCurve<T>(depNode, attrName1);
        spline1.SetKnots(knots);
        T    constantValue1 = T();
        bool hasCurve1 = !spline1.IsEmpty();

        // Retrieve spline for the second attribute
        TsSpline spline2 = GetSplineFromMayaCurve<T>(depNode, attrName2);
        knots = GetKnotsFromMayaCurve<T>(depNode, attrName2);
        spline2.SetKnots(knots);
        T    constantValue2 = T();
        bool hasCurve2 = !spline2.IsEmpty();

        // If both curves are empty, return an empty spline
        if (!hasCurve1 && !hasCurve2) {
            return TsSpline(TfType::Find<T>());
        }

        if (!hasCurve1) {
            // If no curve, retrieve the constant value from the plug
            MPlug plug1 = depNode.findPlug(attrName1, true);
            if (!plug1.isNull()) {
                plug1.getValue(constantValue1);
            }
        }

        if (!hasCurve2) {
            // If no curve, retrieve the constant value from the plug
            MPlug plug2 = depNode.findPlug(attrName2, true);
            if (!plug2.isNull()) {
                plug2.getValue(constantValue2);
            }
        }

        TsSpline resultSpline;
        TsSpline secondarySpline;
        T        constVal = T();

        // Make sure we call pass the argument to the lambda in the correct order
        bool lambdaArgOrder = true;

        // Arbitrarily choose the spline with more knots as the result spline
        if (spline1.GetKnots().size() >= spline2.GetKnots().size()) {
            resultSpline = spline1;
            secondarySpline = spline2;
            constVal = constantValue2;
        } else {
            resultSpline = spline2;
            secondarySpline = spline1;
            constVal = constantValue1;
            lambdaArgOrder = false;
        }
        // Iterate through the knots and apply the lambda function
        auto resKnots = resultSpline.GetKnots();
        for (auto& knot : resKnots) {
            T v = T();
            knot.GetValue<T>(&v);

            T v2 = T();
            if (!secondarySpline.IsEmpty()) {
                // Find the knot in the secondary spline that matches the time of the current knot
                auto   time = knot.GetTime();
                TsKnot secondaryKnot;
                if (secondarySpline.GetKnot(time, &secondaryKnot)) {
                    secondaryKnot.GetValue<T>(&v2);
                } else {
                    secondarySpline.Eval<T>(time, &v2);
                }
            } else {
                v2 = constVal;
            }

            T resultValue = T();
            if (lambdaArgOrder) {
                resultValue = lambda(v, v2);
            } else {
                resultValue = lambda(v2, v);
            }
            knot.SetValue<T>(resultValue);
        }
        resultSpline.SetKnots(resKnots);
        return resultSpline;
    }

    template <typename T>
    static TsSpline CombineUsdAttrsSplines(
        const UsdAttribute&           attr1,
        const UsdAttribute&           attr2,
        const std::function<T(T, T)>& lambda,
        UsdTimeCode                   timeCode = UsdTimeCode::Default())
    {
        TsSpline spline1 = attr1.GetSpline();
        TsSpline spline2 = attr2.GetSpline();

        if (spline1.IsEmpty() && spline2.IsEmpty()) {
            return TsSpline(TfType::Find<T>());
        }

        TsSpline resultSpline;
        TsSpline secondarySpline;
        VtValue  constVal;

        // Make sure we call pass the argument to the lambda in the correct order
        bool lambdaArgOrder = true;

        // Arbitrarily choose the spline with more knots as the result spline
        if (spline1.GetKnots().size() >= spline2.GetKnots().size()) {
            resultSpline = spline1;
            secondarySpline = spline2;
            attr2.Get(&constVal, timeCode);
        } else {
            resultSpline = spline2;
            secondarySpline = spline1;
            attr1.Get(&constVal, timeCode);
            lambdaArgOrder = false;
        }

        // Iterate through the knots and apply the lambda function
        auto resKnots = resultSpline.GetKnots();
        for (auto& knot : resKnots) {
            T v = T();
            knot.GetValue<T>(&v);

            T v2 = T();
            if (!secondarySpline.IsEmpty()) {
                // Find the knot in the secondary spline that matches the time of the current knot
                auto   time = knot.GetTime();
                TsKnot secondaryKnot;
                if (secondarySpline.GetKnot(time, &secondaryKnot)) {
                    secondaryKnot.GetValue<T>(&v2);
                } else {
                    secondarySpline.Eval<T>(time, &v2);
                }
            } else {
                v2 = constVal.Get<T>();
            }

            T resultValue = T();
            if (lambdaArgOrder) {
                resultValue = lambda(v, v2);
            } else {
                resultValue = lambda(v2, v);
            }
            knot.SetValue<T>(resultValue);
        }
        resultSpline.SetKnots(resKnots);
        return resultSpline;
    }

private:
    static TsInterpMode _ConvertMayaTanTypeToUsdTanType(MFnAnimCurve::TangentType mayaTangentType)
    {
        switch (mayaTangentType) {
        case MFnAnimCurve::TangentType::kTangentStep:
        case MFnAnimCurve::TangentType::kTangentStepNext: return TsInterpHeld;
        case MFnAnimCurve::TangentType::kTangentLinear: return TsInterpLinear;
        default: return TsInterpCurve;
        }
    }

    static TsExtrapMode _ConvertUsdExtrapolationToMaya(MFnAnimCurve::InfinityType mayaExtrapolation)
    {
        switch (mayaExtrapolation) {
        case MFnAnimCurve::InfinityType::kLinear: return TsExtrapMode::TsExtrapLinear;
        case MFnAnimCurve::InfinityType::kCycle: return TsExtrapMode::TsExtrapLoopReset;
        case MFnAnimCurve::InfinityType::kOscillate: return TsExtrapMode::TsExtrapLoopOscillate;
        case MFnAnimCurve::InfinityType::kCycleRelative: return TsExtrapMode::TsExtrapLoopRepeat;
        case MFnAnimCurve::InfinityType::kConstant:
        default: return TsExtrapMode::TsExtrapHeld;
        }
    }

    static MFnAnimCurve::InfinityType
    _ConvertUsdExtrapolationTypeToMaya(TsExtrapMode usdExtrapolation)
    {
        switch (usdExtrapolation) {
        case TsExtrapLinear: return MFnAnimCurve::InfinityType::kLinear;
        case TsExtrapLoopReset: return MFnAnimCurve::InfinityType::kCycle;
        case TsExtrapLoopOscillate: return MFnAnimCurve::InfinityType::kOscillate;
        case TsExtrapLoopRepeat: return MFnAnimCurve::InfinityType::kCycleRelative;
        case TsExtrapHeld:
        default: return MFnAnimCurve::InfinityType::kConstant;
        }
    }

    static MFnAnimCurve::TangentType _ConvertUsdTanTypeToMayaTanType(TsInterpMode usdTanType)
    {
        switch (usdTanType) {
        case TsInterpMode::TsInterpHeld: return MFnAnimCurve::TangentType::kTangentStep;
        case TsInterpMode::TsInterpLinear: return MFnAnimCurve::TangentType::kTangentLinear;
        default: return MFnAnimCurve::TangentType::kTangentAuto;
        }
    }
};

PXR_NAMESPACE_CLOSE_SCOPE
#endif
#endif
