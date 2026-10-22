//*********************************************************
//
//    Copyright (c) Microsoft. All rights reserved.
//    This code is licensed under the MIT License.
//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
//    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
//    PARTICULAR PURPOSE AND NONINFRINGEMENT.
//
//*********************************************************
//! @file
//! Provides interoperability between C++/WinRT types and the WRL Module system.
#ifndef __WIL_CPPWINRT_WRL_INCLUDED
#define __WIL_CPPWINRT_WRL_INCLUDED

#include "cppwinrt.h"
#include <winrt/base.h>

#include "result_macros.h"
#include <wrl/module.h>

// wil::wrl_factory_for_winrt_com_class provides interoperability between a
// C++/WinRT class and the WRL Module system, allowing the winrt class to be
// CoCreatable.
//
// Usage:
//   - In your cpp, add:
//         CoCreatableCppWinRtClass(className)
//
//   - In the dll.cpp (or equivalent) for the module containing your class, add:
//         CoCreatableClassWrlCreatorMapInclude(className)
//
namespace wil
{
/// @cond
namespace details
{
    template <typename TCppWinRTClass>
    class module_count_wrapper : public TCppWinRTClass
    {
    public:
        module_count_wrapper()
        {
            if (auto modulePtr = ::Microsoft::WRL::GetModuleBase())
            {
                modulePtr->IncrementObjectCount();
            }
        }

        virtual ~module_count_wrapper()
        {
            if (auto modulePtr = ::Microsoft::WRL::GetModuleBase())
            {
                modulePtr->DecrementObjectCount();
            }
        }
    };
} // namespace details
/// @endcond

/** A WRL class factory that makes a C++/WinRT runtime class creatable through the WRL module system.
Wrapping a C++/WinRT class in this factory lets it participate in WRL's `Module` object-count and class-registration machinery, so
the class becomes CoCreatable (for example via `CoCreateInstance`). Each instance this factory creates is wrapped so that, for its
lifetime, it holds a reference on the WRL module and keeps that module from unloading.

Don't name this factory directly; register the class with the @ref CoCreatableCppWinRtClass macro instead.
@tparam TCppWinRTClass The C++/WinRT class implementation to expose as a CoCreatable COM class. */
template <typename TCppWinRTClass>
class wrl_factory_for_winrt_com_class : public ::Microsoft::WRL::ClassFactory<>
{
public:
    //! Creates an instance of the wrapped C++/WinRT class and returns the requested interface.
    //! Implements `IClassFactory::CreateInstance`. Aggregation is not supported, so a non-null `unknownOuter` fails with
    //! `CLASS_E_NOAGGREGATION`. The created object holds a reference on the WRL module for its lifetime.
    //! @param unknownOuter The aggregating object's controlling `IUnknown`; must be null, since aggregation is unsupported.
    //! @param riid Identifies the interface to retrieve.
    //! @param object Receives the requested interface on success, or null on failure.
    //! @return `S_OK` on success; `CLASS_E_NOAGGREGATION` if `unknownOuter` is non-null; otherwise a failure `HRESULT` from
    //!         creating the object or querying for `riid`.
    IFACEMETHODIMP CreateInstance(_In_opt_ ::IUnknown* unknownOuter, REFIID riid, _COM_Outptr_ void** object) noexcept
    try
    {
        *object = nullptr;
        RETURN_HR_IF(CLASS_E_NOAGGREGATION, unknownOuter != nullptr);

        return winrt::make<details::module_count_wrapper<TCppWinRTClass>>().as(riid, object);
    }
    CATCH_RETURN()
};
} // namespace wil

//! Registers a C++/WinRT class with WRL's creator map so it can be created through @ref wil::wrl_factory_for_winrt_com_class.
//! Place this in the `.cpp` that defines `className`, and pair it with `CoCreatableClassWrlCreatorMapInclude(className)` in the
//! module's `dll.cpp` (or equivalent). Expands to WRL's `CoCreatableClassWithFactory` using this header's factory.
//! ~~~
//! // Define MyClass as an ordinary C++/WinRT class; note that its definition never names wrl_factory_for_winrt_com_class:
//! struct MyClass : winrt::implements<MyClass, IMyThing>
//! {
//!     // ... IMyThing implementation ...
//! };
//!
//! // In the .cpp that defines the MyClass class (this macro applies the factory for you):
//! CoCreatableCppWinRtClass(MyClass)
//!
//! // In the module's dll.cpp (or equivalent):
//! CoCreatableClassWrlCreatorMapInclude(MyClass)
//! ~~~
//! @param className The C++/WinRT class to make CoCreatable.
//! @note `className` must not be `final`; the factory creates each instance through a wrapper that derives from it.
#define CoCreatableCppWinRtClass(className) \
    CoCreatableClassWithFactory(className, ::wil::wrl_factory_for_winrt_com_class<className>)

#endif // __WIL_CPPWINRT_WRL_INCLUDED
