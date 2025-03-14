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
#ifndef MTLXUSDTRANSLATORS_SHADING_TOKENS_H
#define MTLXUSDTRANSLATORS_SHADING_TOKENS_H

#include <pxr/base/tf/staticTokens.h>
#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
#define TR_USD_COMMON \
    (varname)

// XXX: We duplicate these tokens here rather than create a dependency on
// usdImaging in case the plugin is being built with imaging disabled.
// If/when they move out of usdImaging to a place that is always available,
// they should be pulled from there instead.
#define TR_USD_TEXTURE \
    (UsdUVTexture) \
    (bias) \
    (fallback) \
    (file) \
    (scale) \
    (st) \
    (wrapS) \
    (wrapT) \
    (black) \
    (mirror) \
    (repeat) \
    (sourceColorSpace) \
    (raw) \
    (sRGB) \
    ((RGBOutputName, "rgb")) \
    ((RedOutputName, "r")) \
    ((GreenOutputName, "g")) \
    ((BlueOutputName, "b")) \
    ((AlphaOutputName, "a"))

#define TR_USD_PRIMVAR \
    (UsdPrimvarReader_float2) \
    (displayColor) \
    (result)

#define TR_USD_XFORM \
    (UsdTransform2d) \
    (in) \
    (rotation) \
    (translation)

// clang-format on

TF_DECLARE_PUBLIC_TOKENS(TrUsdTokens, , TR_USD_COMMON TR_USD_TEXTURE TR_USD_PRIMVAR TR_USD_XFORM);

// clang-format off
#define TR_MAYA_MATERIALS \
    (usdPreviewSurface) \
    (lambert) \
    (phong) \
    (blinn) \
    (eccentricity) \
    (specularRollOff) \
    (cosinePower) \
    (roughness) \
    (color) \
    (inColor) \
    (transparency) \
    (diffuse) \
    (incandescence) \
    (normalCamera) \
    (displacement) \
    (outColor) \
    (outColorR) \
    (outColorG) \
    (outColorB) \
    (outAlpha) \
    (outTransparency) \
    (outTransparencyR) \
    (outTransparencyG) \
    (outTransparencyB)

#define TR_MAYA_NODES \
    (colorCorrect) \
    (floatCorrect) \
    (clamp)

#define TR_MAYA_STANDARD_SURFACE \
    (standardSurface) \
    (base) \
    (baseColor) \
    (diffuseRoughness) \
    (metalness) \
    (specular) \
    (specularColor) \
    (specularRoughness) \
    (specularIOR) \
    (specularAnisotropy) \
    (specularRotation) \
    (transmission) \
    (transmissionColor) \
    (transmissionDepth) \
    (transmissionScatter) \
    (transmissionScatterAnisotropy) \
    (transmissionDispersion) \
    (transmissionExtraRoughness) \
    (subsurface) \
    (subsurfaceColor) \
    (subsurfaceRadius) \
    (subsurfaceScale) \
    (subsurfaceAnisotropy) \
    (sheen) \
    (sheenColor) \
    (sheenRoughness) \
    (coat) \
    (coatColor) \
    (coatRoughness) \
    (coatAnisotropy) \
    (coatRotation) \
    (coatIOR) \
    (coatNormal) \
    (coatAffectColor) \
    (coatAffectRoughness) \
    (thinFilmThickness) \
    (thinFilmIOR) \
    (emission) \
    (emissionColor) \
    (opacity) \
    (opacityR) \
    (opacityG) \
    (opacityB) \
    (thinWalled) \
    (tangentUCamera)

#define TR_MAYA_FILE \
    (file) \
    (alphaGain) \
    (alphaOffset) \
    (alphaIsLuminance) \
    (colorGain) \
    (colorOffset) \
    (colorSpace) \
    (defaultColor) \
    (exposure) \
    (fileTextureName) \
    (filterType) \
    (invert) \
    ((UDIMTag, "<UDIM>")) \
    (uvTilingMode)   \
    ((utilityRaw, "Utility - Raw"))  \
    (Raw) \
    (sRGB)

#define TR_MAYA_UV \
    (place2dTexture) \
    (coverage) \
    (coverageU) \
    (coverageV) \
    (translateFrame) \
    (rotateFrame) \
    (mirrorU) \
    (mirrorV) \
    (stagger) \
    (wrapU) \
    (wrapV) \
    (repeatUV) \
    (offset) \
    (rotateUV) \
    (noiseUV) \
    (vertexUvOne) \
    (vertexUvTwo) \
    (vertexUvThree) \
    (vertexCameraOne) \
    (outUvFilterSize) \
    (uvFilterSize) \
    (outUV) \
    (uvCoord)

#define TR_MAYA_PRIMVAR \
    (cpvColor)

// clang-format on

TF_DECLARE_PUBLIC_TOKENS(
    TrMayaTokens,
    ,
    TR_MAYA_MATERIALS TR_MAYA_NODES TR_MAYA_STANDARD_SURFACE TR_MAYA_FILE TR_MAYA_UV
        TR_MAYA_PRIMVAR);

// clang-format off
#define TR_MAYA_OPENPBR_SURFACE \
    (openPBRSurface) \
    (baseWeight) \
    (baseColor) \
    (baseDiffuseRoughness) \
    (baseMetalness) \
    (specularWeight) \
    (specularColor) \
    (specularRoughness) \
    (specularIOR) \
    (specularRoughnessAnisotropy) \
    (transmissionWeight) \
    (transmissionColor) \
    (transmissionDepth) \
    (transmissionScatter) \
    (transmissionScatterAnisotropy) \
    (transmissionDispersionScale) \
    (transmissionDispersionAbbeNumber) \
    (subsurfaceWeight) \
    (subsurfaceColor) \
    (subsurfaceRadius) \
    (subsurfaceRadiusScale) \
    (subsurfaceScatterAnisotropy) \
    (fuzzWeight) \
    (fuzzColor) \
    (fuzzRoughness) \
    (coatWeight) \
    (coatColor) \
    (coatRoughness) \
    (coatRoughnessAnisotropy) \
    (coatIOR) \
    (coatDarkening) \
    (thinFilmWeight) \
    (thinFilmThickness) \
    (thinFilmIOR) \
    (emissionWeight) \
    (emissionLuminance) \
    (emissionColor) \
    (geometryOpacity) \
    (geometryThinWalled) \
    (normalCamera) \
    (geometryCoatNormal) \
    (tangentUCamera) \
    (geometryCoatTangent)
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(TrMayaOpenPBRTokens, , TR_MAYA_OPENPBR_SURFACE);

#ifdef WANT_MATERIALX_TRANSLATORS

// clang-format off
#define TR_MTLX_COMMON \
    ((conversionName, "MaterialX")) \
    ((contextName, "mtlx")) \
    ((niceName, "MaterialX Shading")) \
    ((exportDescription, "Exports bound shaders as a MaterialX UsdShade network.")) \
    ((importDescription, "Search for a MaterialX UsdShade network to import.")) \
    ((ConstructorPrefix, "MayaCTOR")) \
    ((CombinePrefix, "ND_combine"))


#define TR_MTLX_NODE_DEFS \
    (MayaND_lambert_surfaceshader) \
    (MayaND_phong_surfaceshader) \
    (MayaND_blinn_surfaceshader) \
    (MayaND_place2dTexture_vector2) \
    (MayaND_fileTexture_float) \
    (MayaND_fileTexture_color3) \
    (MayaND_fileTexture_color4) \
    (MayaND_fileTexture_vector2) \
    (MayaND_fileTexture_vector3) \
    (MayaND_fileTexture_vector4) \
    (MayaND_clamp_vector3) \
    (LdkND_FloatCorrect_float) \
    (LdkND_ColorCorrect_color4) \
    (ND_standard_surface_surfaceshader) \
    (ND_UsdPreviewSurface_surfaceshader) \
    (ND_image_float) \
    (ND_image_vector2) \
    (ND_image_vector3) \
    (ND_image_vector4) \
    (ND_image_color3) \
    (ND_image_color4) \
    (ND_geompropvalue_vector2) \
    (ND_luminance_color3) \
    (ND_luminance_color4) \
    (ND_convert_color3_vector3) \
    (ND_normalmap)

#define TR_MTLX_STANDARD_SURFACE \
    (base) \
    (base_color) \
    (diffuse_roughness) \
    (metalness) \
    (specular) \
    (specular_color) \
    (specular_roughness) \
    (specular_IOR) \
    (specular_anisotropy) \
    (specular_rotation) \
    (transmission) \
    (transmission_color) \
    (transmission_depth) \
    (transmission_scatter) \
    (transmission_scatter_anisotropy) \
    (transmission_dispersion) \
    (transmission_extra_roughness) \
    (subsurface) \
    (subsurface_color) \
    (subsurface_radius) \
    (subsurface_scale) \
    (subsurface_anisotropy) \
    (sheen) \
    (sheen_color) \
    (sheen_roughness) \
    (coat) \
    (coat_color) \
    (coat_roughness) \
    (coat_anisotropy) \
    (coat_rotation) \
    (coat_IOR) \
    (coat_normal) \
    (coat_affect_color) \
    (coat_affect_roughness) \
    (thin_film_thickness) \
    (thin_film_IOR) \
    (emission) \
    (emission_color) \
    (opacity) \
    (thin_walled) \
    (normal) \
    (tangent)

#define TR_MTLX_IMAGE \
    (file) \
    ((paramDefault, "default")) \
    (texcoord) \
    (uaddressmode) \
    (vaddressmode) \
    (filtertype) \
    (constant) \
    (clamp) \
    (periodic) \
    (mirror) \
    (closest) \
    (linear) \
    (cubic)

#define TR_MTLX_ATTRIBUTES \
    (varnameStr) \
    (geomprop) \
    (channels) \
    (in) \
    (in1) \
    (in2) \
    (out)

// clang-format on

TF_DECLARE_PUBLIC_TOKENS(
    TrMtlxTokens,
    ,
    TR_MTLX_COMMON TR_MTLX_NODE_DEFS TR_MTLX_STANDARD_SURFACE TR_MTLX_IMAGE TR_MTLX_ATTRIBUTES);

// clang-format off
#define TR_MTLX_OPENPBR_SURFACE \
    (ND_open_pbr_surface_surfaceshader) \
    (base_weight) \
    (base_color) \
    (base_diffuse_roughness) \
    (base_metalness) \
    (specular_weight) \
    (specular_color) \
    (specular_roughness) \
    (specular_ior) \
    (specular_roughness_anisotropy) \
    (transmission_weight) \
    (transmission_color) \
    (transmission_depth) \
    (transmission_scatter) \
    (transmission_scatter_anisotropy) \
    (transmission_dispersion_scale) \
    (transmission_dispersion_abbe_number) \
    (subsurface_weight) \
    (subsurface_color) \
    (subsurface_radius) \
    (subsurface_radius_scale) \
    (subsurface_scatter_anisotropy) \
    (fuzz_weight) \
    (fuzz_color) \
    (fuzz_roughness) \
    (coat_weight) \
    (coat_color) \
    (coat_roughness) \
    (coat_roughness_anisotropy) \
    (coat_ior) \
    (coat_darkening) \
    (thin_film_weight) \
    (thin_film_thickness) \
    (thin_film_ior) \
    (emission_luminance) \
    (emission_color) \
    (geometry_opacity) \
    (geometry_thin_walled) \
    (geometry_normal) \
    (geometry_coat_normal) \
    (geometry_tangent) \
    (geometry_coat_tangent)
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(TrMtlxOpenPBRTokens, , TR_MTLX_OPENPBR_SURFACE);

#endif

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HD_VP2_TOKENS_H
