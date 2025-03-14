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

#include <pxr/usd/usdGeom/camera.h>
#include <pxr_python.h>

#include <maya/MTypes.h>

// Hack because MDGModifier assign operator is not public.
#undef OPENMAYA_PRIVATE
#define OPENMAYA_PRIVATE public

#include <maya/MDGModifier.h>
#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MPlug.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {
// Required because MObject::classname does not exists
template <class MayaClass> const char* classname() { return MayaClass::className(); }
template <> const char*                classname<MObject>() { return "MObject"; }

template <class MayaClass> void copyOperator(void* dst, const MayaClass& object)
{
    *((MayaClass*)(dst)) = object;
}

/* Should be detectable detecting maya_useNewAPI in plugin

# Using the Maya Python API 2.0.
def maya_useNewAPI():
    pass
*/

#ifdef PYTHON_API_V1
typedef void* (*swig_converter_func)(void*, int*);
typedef struct swig_type_info* (*swig_dycast_func)(void**);
typedef struct
{
    PyObject_HEAD void* ptr;
    swig_type_info*     ty;
    int                 own;
    PyObject*           next;
    PyObject*           dict;
} SwigPyObject;

template <class MayaClass> struct MayaClassConverter
{
    // to-python conversion of const MayaClass.
    static PyObject* convert(const MayaClass& object)
    {
        TfPyLock                           lock;
        PXR_BOOST_PYTHON_NAMESPACE::object classInstance
            = PXR_BOOST_PYTHON_NAMESPACE::import("maya.OpenMaya").attr(classname<MayaClass>())();
        PXR_BOOST_PYTHON_NAMESPACE::object theSwigObject = classInstance.attr("this");
        SwigPyObject*                      theSwigPyObject = (SwigPyObject*)theSwigObject.ptr();
        copyOperator(theSwigPyObject->ptr, object);
        return PXR_BOOST_PYTHON_NAMESPACE::incref(classInstance.ptr());
    }
};
#else
template <class M> struct MPyObject : public PyObject
{
public:
    typedef M MayaType;
    M*        fPtr;
};

template <class MayaClass> struct MayaClassConverter
{
    // to-python conversion of const MayaClass.
    static PyObject* convert(const MayaClass& object)
    {
        TfPyLock                           lock;
        PXR_BOOST_PYTHON_NAMESPACE::object classInstance
            = PXR_BOOST_PYTHON_NAMESPACE::import("maya.api.OpenMaya")
                  .attr(classname<MayaClass>())();
        MPyObject<MayaClass>* thePyObject = (MPyObject<MayaClass>*)classInstance.ptr();
        copyOperator(thePyObject->fPtr, object);
        return PXR_BOOST_PYTHON_NAMESPACE::incref(classInstance.ptr());
    }
};
#endif

// Register both converters from and to Python for a type T
template <class T> struct registerConverter
{
public:
    registerConverter()
    {
        PXR_BOOST_PYTHON_NAMESPACE::to_python_converter<T, MayaClassConverter<T>>();
        PXR_BOOST_PYTHON_NAMESPACE::converter::registry::push_back(
            &convertible, &construct, typeid(T));
    }

private:
    static void* convertible(PyObject* obj) { return obj; }

    static void construct(
        PyObject*                                                              obj,
        PXR_BOOST_PYTHON_NAMESPACE::converter::rvalue_from_python_stage1_data* data)
    {
        MPyObject<T>* thePyObject = (MPyObject<T>*)obj;
        data->convertible = thePyObject->fPtr;
    }
};

} // namespace

void wrapOpenMaya()
{
    registerConverter<MObject>();
    registerConverter<MDagPath>();
    registerConverter<MDagPathArray>();
    registerConverter<MPlug>();
    registerConverter<MDGModifier>();
}
