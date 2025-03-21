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
#include <mayaUsd_Schemas/MayaReference.h>

#include <pxr/base/tf/pyContainerConversions.h>
#include <pxr/base/tf/pyResultConversions.h>
#include <pxr/base/tf/pyUtils.h>
#include <pxr/base/tf/wrapTypeHelpers.h>
#include <pxr/usd/sdf/primSpec.h>
#include <pxr/usd/usd/pyConversions.h>
#include <pxr/usd/usd/schemaBase.h>
#include <pxr_python.h>

#include <string>

using namespace PXR_BOOST_PYTHON_NAMESPACE;

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

#define WRAP_CUSTOM template <class Cls> static void _CustomWrapCode(Cls& _class)

// fwd decl.
WRAP_CUSTOM;

static UsdAttribute
_CreateMayaReferenceAttr(MayaUsd_SchemasMayaReference& self, object defaultVal, bool writeSparsely)
{
    return self.CreateMayaReferenceAttr(
        UsdPythonToSdfType(defaultVal, SdfValueTypeNames->Asset), writeSparsely);
}

static UsdAttribute
_CreateMayaNamespaceAttr(MayaUsd_SchemasMayaReference& self, object defaultVal, bool writeSparsely)
{
    return self.CreateMayaNamespaceAttr(
        UsdPythonToSdfType(defaultVal, SdfValueTypeNames->String), writeSparsely);
}

static UsdAttribute
_CreateMayaAutoEditAttr(MayaUsd_SchemasMayaReference& self, object defaultVal, bool writeSparsely)
{
    return self.CreateMayaAutoEditAttr(
        UsdPythonToSdfType(defaultVal, SdfValueTypeNames->Bool), writeSparsely);
}

static std::string _Repr(const MayaUsd_SchemasMayaReference& self)
{
    std::string primRepr = TfPyRepr(self.GetPrim());
    return TfStringPrintf("MayaUsd_Schemas.MayaReference(%s)", primRepr.c_str());
}

} // anonymous namespace

void wrapMayaUsd_SchemasMayaReference()
{
    typedef MayaUsd_SchemasMayaReference This;

    class_<This, bases<UsdGeomXformable>> cls("MayaReference");

    cls.def(init<UsdPrim>(arg("prim")))
        .def(init<UsdSchemaBase const&>(arg("schemaObj")))
        .def(TfTypePythonClass())

        .def("Get", &This::Get, (arg("stage"), arg("path")))
        .staticmethod("Get")

        .def("Define", &This::Define, (arg("stage"), arg("path")))
        .staticmethod("Define")

        .def(
            "GetSchemaAttributeNames",
            &This::GetSchemaAttributeNames,
            arg("includeInherited") = true,
            return_value_policy<TfPySequenceToList>())
        .staticmethod("GetSchemaAttributeNames")

        .def(
            "_GetStaticTfType",
            (TfType const& (*)())TfType::Find<This>,
            return_value_policy<return_by_value>())
        .staticmethod("_GetStaticTfType")

        .def(!self)

        .def("GetMayaReferenceAttr", &This::GetMayaReferenceAttr)
        .def(
            "CreateMayaReferenceAttr",
            &_CreateMayaReferenceAttr,
            (arg("defaultValue") = object(), arg("writeSparsely") = false))

        .def("GetMayaNamespaceAttr", &This::GetMayaNamespaceAttr)
        .def(
            "CreateMayaNamespaceAttr",
            &_CreateMayaNamespaceAttr,
            (arg("defaultValue") = object(), arg("writeSparsely") = false))

        .def("GetMayaAutoEditAttr", &This::GetMayaAutoEditAttr)
        .def(
            "CreateMayaAutoEditAttr",
            &_CreateMayaAutoEditAttr,
            (arg("defaultValue") = object(), arg("writeSparsely") = false))

        .def("__repr__", ::_Repr);

    _CustomWrapCode(cls);
}

// ===================================================================== //
// Feel free to add custom code below this line, it will be preserved by
// the code generator.  The entry point for your custom code should look
// minimally like the following:
//
// WRAP_CUSTOM {
//     _class
//         .def("MyCustomMethod", ...)
//     ;
// }
//
// Of course any other ancillary or support code may be provided.
//
// Just remember to wrap code in the appropriate delimiters:
// 'namespace {', '}'.
//
// ===================================================================== //
// --(BEGIN CUSTOM CODE)--

namespace {

WRAP_CUSTOM { }

} // namespace
