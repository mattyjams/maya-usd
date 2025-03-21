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
#include <usdUfe/utils/editRouter.h>
#include <usdUfe/utils/editRouterContext.h>

#include <pxr/base/tf/pyError.h>
#include <pxr/base/tf/pyLock.h>
#include <pxr_python.h>

#include <exception>
#include <iostream>

using namespace PXR_BOOST_PYTHON_NAMESPACE;

namespace {

std::string handlePythonException()
{
    PyObject* exc = nullptr;
    PyObject* val = nullptr;
    PyObject* tb = nullptr;
    PyErr_Fetch(&exc, &val, &tb);
    if (!exc)
        return "unknown Python exception";

    handle<> hexc(exc);
    handle<> hval(allow_null(val));
    handle<> htb(allow_null(tb));
    object   traceback(import("traceback"));
    object   format_exception_only(traceback.attr("format_exception_only"));
    object   formatted_list = val ? format_exception_only(hexc, hval) : format_exception_only(hexc);
    object   formatted = str("\n").join(formatted_list);

    return extract<std::string>(formatted);
}

class PyEditRouter : public UsdUfe::EditRouter
{
public:
    PyEditRouter(PyObject* pyCallable)
        : _pyCb(pyCallable)
    {
        if (_pyCb)
            Py_INCREF(_pyCb);
    }

    ~PyEditRouter() override
    {
        PXR_NS::TfPyLock pyLock;
        if (_pyCb)
            Py_DECREF(_pyCb);
    }

    void operator()(const PXR_NS::VtDictionary& context, PXR_NS::VtDictionary& routingData) override
    {
        // Note: necessary to compile the TF_WARN macro as it refers to USD types without using
        //       the namespace prefix.
        PXR_NAMESPACE_USING_DIRECTIVE;

        PXR_NS::TfPyLock pyLock;
        if (!PyCallable_Check(_pyCb)) {
            return;
        }
        PXR_BOOST_PYTHON_NAMESPACE::dict dictObject(routingData);
        try {
            call<void>(_pyCb, context, dictObject);
        } catch (const PXR_BOOST_PYTHON_NAMESPACE::error_already_set&) {
            const std::string errorMessage = handlePythonException();
            PXR_BOOST_PYTHON_NAMESPACE::handle_exception();
            PyErr_Clear();
            TF_WARN("%s", errorMessage.c_str());
            throw std::runtime_error(errorMessage);
        } catch (const std::exception& ex) {
            TF_WARN("%s", ex.what());
            throw;
        }

        // Extract keys and values individually so that we can extract
        // PXR_NS::UsdEditTarget correctly from PXR_NS::TfPyObjWrapper.
        const PXR_BOOST_PYTHON_NAMESPACE::object items = dictObject.items();
        for (PXR_BOOST_PYTHON_NAMESPACE::ssize_t i = 0; i < len(items); ++i) {

            PXR_BOOST_PYTHON_NAMESPACE::extract<std::string> keyExtractor(items[i][0]);
            if (!keyExtractor.check()) {
                continue;
            }

            PXR_BOOST_PYTHON_NAMESPACE::extract<PXR_NS::VtValue> valueExtractor(items[i][1]);
            if (!valueExtractor.check()) {
                continue;
            }

            auto vtvalue = valueExtractor();

            if (vtvalue.IsHolding<PXR_NS::TfPyObjWrapper>()) {
                const auto wrapper = vtvalue.Get<PXR_NS::TfPyObjWrapper>();

                PXR_NS::TfPyLock                                           lock;
                PXR_BOOST_PYTHON_NAMESPACE::extract<PXR_NS::UsdEditTarget> editTargetExtractor(
                    wrapper.Get());
                if (editTargetExtractor.check()) {
                    auto editTarget = editTargetExtractor();
                    routingData[keyExtractor()] = PXR_NS::VtValue(editTarget);
                }
            } else {
                routingData[keyExtractor()] = vtvalue;
            }
        }
    }

private:
    PyObject* _pyCb;
};

UsdUfe::OperationEditRouterContext*
OperationEditRouterContextInit(const PXR_NS::TfToken& operationName, const PXR_NS::UsdPrim& prim)
{
    return new UsdUfe::OperationEditRouterContext(operationName, prim);
}

UsdUfe::AttributeEditRouterContext*
AttributeEditRouterContextInit(const PXR_NS::UsdPrim& prim, const PXR_NS::TfToken& attributeName)
{
    return new UsdUfe::AttributeEditRouterContext(prim, attributeName);
}

// Note: due to a limitation of boost::python, we cannot pass shared-pointer
//       between Python and C++. See this stack overflow answer for an explanation:
//       https://stackoverflow.com/questions/20825662/boost-python-argument-types-did-not-match-c-signature
//
//       That is why stages and layers are passed by raw C++ references and a
//       smart pointer is created on-the-fly. Otherwise, the stage you pass it
//       in Python would become invalid during the call.

void registerStageLayerEditRouterFromLayerName(
    const PXR_NS::TfToken& operation,
    PXR_NS::UsdStage&      stage,
    const std::string&     layerName)
{
    PXR_NS::UsdStageRefPtr stagePtr(&stage);
    auto                   layer = PXR_NS::SdfLayer::Find(layerName);
    UsdUfe::registerStageLayerEditRouter(operation, stagePtr, layer);
}

void registerStageLayerEditRouterFromLayer(
    const PXR_NS::TfToken& operation,
    PXR_NS::UsdStage&      stage,
    PXR_NS::SdfLayer&      layer)
{
    PXR_NS::UsdStageRefPtr stagePtr(&stage);
    return UsdUfe::registerStageLayerEditRouter(
        operation, stagePtr, PXR_NS::SdfLayerHandle(&layer));
}

} // namespace

void wrapEditRouter()
{
    // As per
    // https://stackoverflow.com/questions/18889028/a-positive-lambda-what-sorcery-is-this
    // the plus (+) sign before the lambda triggers a conversion to a plain old
    // function pointer for the lambda, which is required for successful Boost
    // Python compilation of the lambda and its argument.

    def(
        "registerEditRouter", +[](const PXR_NS::TfToken& operation, PyObject* editRouter) {
            return UsdUfe::registerEditRouter(
                operation, std::make_shared<PyEditRouter>(editRouter));
        });

    def("registerStageLayerEditRouter", &registerStageLayerEditRouterFromLayerName);
    def("registerStageLayerEditRouter", &registerStageLayerEditRouterFromLayer);

    def("restoreDefaultEditRouter", &UsdUfe::restoreDefaultEditRouter);

    def("restoreAllDefaultEditRouters", &UsdUfe::restoreAllDefaultEditRouters);

    def("clearAllEditRouters", &UsdUfe::clearAllEditRouters);

    using OpThis = UsdUfe::OperationEditRouterContext;
    class_<OpThis, PXR_BOOST_PYTHON_NAMESPACE::noncopyable>("OperationEditRouterContext", no_init)
        .def("__init__", make_constructor(OperationEditRouterContextInit));

    using AttrThis = UsdUfe::AttributeEditRouterContext;
    class_<AttrThis, PXR_BOOST_PYTHON_NAMESPACE::noncopyable>("AttributeEditRouterContext", no_init)
        .def("__init__", make_constructor(AttributeEditRouterContextInit));

    using PrimMdThis = UsdUfe::PrimMetadataEditRouterContext;
    class_<PrimMdThis, PXR_BOOST_PYTHON_NAMESPACE::noncopyable>(
        "PrimMetadataEditRouterContext", no_init)
        .def(init<const PXR_NS::UsdPrim&, const PXR_NS::TfToken&>())
        .def(init<const PXR_NS::UsdPrim&, const PXR_NS::TfToken&, const PXR_NS::TfToken&>())
        .def(init<
             const PXR_NS::UsdPrim&,
             const PXR_NS::TfToken&,
             const PXR_NS::TfToken&,
             const PXR_NS::SdfLayerHandle&>());
}
