# Overview {#mainpage}

The Windows Implementation Libraries (wil) were created to improve productivity and solve problems commonly seen by Windows developers.
This guide will focus on what developers need to know to consume this functionality.
See the @ref page_principles for more information regarding WIL's role or structure.

You can also find more documentation on the [public GitHub wiki](https://github.com/microsoft/wil/wiki).

> **Microsoft internal**: [Click here (one-click join)](http://idwebelements/GroupManagement.aspx?Group=wildisc&Operation=join) to join the Windows Implementation Libraries discussion alias (wildisc).

## What is it?

These libraries are simply header files that can be directly included and used by any Windows product.
They are built with the minimum possible dependencies for the area each covered.
For more information, visit the [public GitHub repository](https://github.com/microsoft/wil).

## How are errors handled (exceptions, fail-fast, HRESULTs, etc)?

Callers naturally select the kind of error handling they want based upon functions and classes they use.
Wil supports exceptions, fail-fast, and error-code (HRESULT) error handling strategies.
Wil does not force use of exceptions or fail-fast and is safe to add to existing projects.

See [Error Handling](@ref page_errors) for in-depth information on how wil approaches error handling.

## Where is the code (what are the namespaces)?

    wil::

All wil classes and functions are available behind the `wil` namespace name.

    wistd::

Some core c++ concepts from STL are made available from wil for callers that are not using STL.
Wil selectively pulls in necessary exception-neutral functionality from STL with almost no modification.
This functionality is available behind the `wistd::` namespace and exactly mirrors the same functionality behind `std::`.
Exception-based callers should always prefer using the `std::` counterparts.




@page page_principles Guiding Principles of WIL

One of the key principles of WIL is that it can just be picked up and directly used without adding new dependencies or otherwise affecting the performance of your application.

## Is there a WIL.LIB or WIL.DLL?

There is no LIB or DLL for WIL yet.

Most of WIL is lightweight C++ wrapping around system functionality already present in a DLL or template-driven functionality that itself can&apos;t be placed in a DLL.

A LIB could help with compilation for a smaller percentage of the code, but adds friction and has its own problems as you would need a LIB for every unique combination of settings that people use WIL with (exceptions, no exceptions, C++/cx, no/cx, private apis allowed/not, etc...) -- the permutations are higher than you might think.

A DLL could help with a reduction in code size.
Most DLLs using WIL inspected to date have between 1-3 pages of code attributed to them from WIL, so per DLL the costs are not that high.
In aggregate the costs are significant, but pushing code behind DLLs will not save all of that 1-3 pages, but some smaller portion of it.




@page page_errors Error handling in WIL

Wil natively supports exceptions, fail-fast, and error-code (HRESULT) error handling strategies uniformly.
Callers naturally select the kind of error handling they want based upon the functions and classes used.

## Error Handling Patterns
Each wil function or class in wil can be classified in one (and only one) of the following ways:

### Error Handling Neutral
* Behavior:  Will *never* produce an error (no HRESULTs, exceptions or fail-fast)
* Availability:  All callers
* Naming:  Routines have the "best name" without any suffix decoration (see error code and fail fast counter-examples below)
* Examples:
    * wil::unique_handle
    * wil::assign_to_opt_param

### Exception Based
* Behavior:  All errors will throw an exception.  C++/cx will throw a PlatformException, straight C++ will throw a wil::ResultException.
* Availability:  Only callers with exceptions enabled in their binary.  Can also be suppressed explicitly even with exceptions enabled via @ref WIL_SUPPRESS_EXCEPTIONS.
* Naming:  Routines have the "best name" without any suffix decoration (see error code and fail fast counter-examples below)
* Examples:
    * wil::com_ptr
    * wil::unique_event_watcher

### Fail-fast Based
* Behavior:  Any error will fail-fast the process and produce a Watson.
* Availability:  All callers
* Naming:  Routines or classes with this behavior are always clearly identified with a `FailFast` or `_failfast` suffix.
* Examples:
    * wil::make_unique_failfast
    * wil::com_ptr_failfast
    * wil::com_query_failfast

### Error-code Based
* Behavior:  All errors will be returned as an HRESULT.
* Availability:  All callers
* Naming:  Routines or classes with this behavior are always clearly identified with a `NoThrow` or `_nothrow` suffix.
* Examples:
    * wil::make_unique_nothrow
    * wil::com_ptr_nothrow
    * wil::com_query_to_nothrow
    * wil::event_nothrow

## Error Handling Helpers

Wil provides a family of macros and functions that are designed to handle errors in C++ and C++/cx.
With these macros, callers can handle errors in a consistent manner across exceptions, fail-fast, error-codes, and logging (ignoring errors).

See [Windows Error Handling Helpers](https://github.com/microsoft/wil/wiki/Error-handling-helpers) for in-depth discussion.

## RoOriginateError Helpers

Wil provides the ability to automatically call RoOriginateError whenever a return-based or exception-based error is encountered by a Wil error-handling macro.
This is especially useful for WinRT components that are called by app processes, because it will cause the full call stack to be observed from the point where the error was first observed.
Uncaught exceptions that terminate the process with RoFailFastWithErrorContext will see the error information and will include it in the !analyze output.

To opt into this behavior simply include the following header once per DLL.
The rest is automatic.

    #include <wil\result_o_riginate.h>

## TraceLogging Helpers (reporting telemetry for errors)

With the Error Handling Helpers (result.h) wil enables a consistent way for callers to handle errors within their code by performing error control flow with macros such as RETURN_IF_FAILED or THROW_IF_FAILED.
These macros collect information from their call sites and can automatically report failures that happen within your application through telemetry.




@page page_comptr COM pointer comparison (WIL vs WRL)

## WIL pointers

### COM pointer (wil::com_ptr)

This is the core exception-based WIL COM pointer class.
Also see wil::com_ptr_failfast and wil::com_ptr_nothrow for other error handling
strategies.

### Agile references (wil::com_agile_ref) and Weak references (wil::com_weak_ref)

These are simply typedefs of wil::com_ptr<IAgileReference> and wil::com_ptr<IWeakReference>.
All query methods resolve the reference, rather than simply issuing a query interface.
Also see wil::com_agile_ref_failfast, wil::com_weak_ref_failfast, wil::com_agile_ref_nothrow and wil::com_weak_ref_nothrow for other error handling strategies.

## Primary differences from WRL

The key difference between wil's offerings and WRL's ComPtr is the addition of exception-based and fail-fast based COM pointer classes.
With an exception-based smart pointer, many traditional patterns are simplified.
For example, querying for a particular supported interface and then calling a method on it, becomes a single fluent operation:

    ptr.query<IFoo>()->Method();

This extends to multiple method calls without the traditional error handling artifacts:

    auto foo = ptr.query<IFoo>();
    foo->Method1();
    foo->Method2();

Similarly, new try methods exist to support probing for support of a given interface:

    auto foo = ptr.try_query<IFoo>();
    if (foo)
    {
        foo->Method();
    }

The try methods are an example of some of the enhancements that were made to WIL's com_ptr over the initial WRL design.
See @ref page_query for a more in depth review of the new mechanisms available for querying.

Given this opportunity for change, WIL has also corrected all known bugs an inconsistencies in WRL's ComPtr.
A simple example of this is ComPtr's CopyTo method.
It had several overloads which were inconsistent with respect to how they treated null (crashing or success).

WIL also chooses to be more aggressive about eliding QI where possible.
All queries automatically use casting and AddRef where the interfaces are
compatible.

## Comparing the implementation

### Identical behavior

Places where wil's com_ptr behavior is the exact same as WRL's ComPtr will not be discussed.
These are things like constructors, assignment operators, etc.
If it's not listed below, you can assume there is no behavioral difference between WIL and WRL.

### Name changes with identical behavior

WIL's naming, where applicable, mirrors that of the STL, so there are some minor naming differences (primarily capitalization) for methods that behave identically:

WRL                         | WIL
--------------------------- | ---------------------------
Swap(other)                 | swap(other)
Get()                       | get()
GetAddressOf()              | addressof()
Detach()                    | detach()
Attach(ptr)                 | attach(ptr)
Reset()                     | reset()

### Behavioral changes:

Each of these mappings have some minor behavioral changes from their WRL counterpart (explained further below the table):

WRL                         | WIL
--------------------------- | ---------------------------
CopyTo(...)                 | query_to(...)
AsWeak()                    | wil::com_weak_query(...)
ReleaseAndGetAddressOf()    | &ptr
&ptr (ComPtr<T>*)           | wistd::addressof(ptr)
As(...)                     | query_to(...)
AsIID(...)                  | ptr.get()->QueryInterface(...);

#### CopyTo methods
WRL's ComPtr<T> has inconsistent overloads regarding whether or not CopyTo on a null pointer would succeed with null (like an assignment) or crash (like a call to QI).
WIL's com_ptr<T> breaks up these two behaviors into `copy_to` and `query_to` methods.
The `copy_to` method behaves like assignment when the pointer is null (succeeding), but can still fail when non-null if the QI fails.
The `query_to` method behaves like a direct call that will crash if the pointer is null.

#### AsWeak method
Both weak and agile references are handled through global functions that accept a COM pointer (raw, wil, or WRL) and return the corresponding weak or agile reference.
See @ref wil::com_weak_query and @ref wil::com_agile_query for more details.

#### operator&
The most common usage is identical.
Just like WRL's ComPtr, you should use the `&` operator to accept an output COM pointer from a function call.
The internal pointer is intentionally released ahead of assignment to avoid accidental leaks.

The `ReleaseAndGetAddressOf()` routine does not exist in `com_ptr`.
Just use the `&` operator for this behavior.
Avoid this where reasonable by avoiding direct re-use (prefer to use locals and swap instead).

Though it's a rare need, if you need to pass an `_Inout_ wil::com_ptr<T>*` pointer to a function, you should use `wistd::addressof(ptr)`, rather than `&ptr`.
WRL's ComPtr<T> used to enable this by creating a temporary in response to `&` which allowed an automatic cast to `WRL::ComPtr<IFace>*`, but this temporary introduced a number of other problems (broken template argument resolution, implicit casts to IUnknown** and IInspectable**).
Minor difference: note that WRL also cleared the pointer (incorrectly) in this case.

#### As method
There is no direct replacement for the `As` method, but given the way that the `&` operator works, `query_to` results in identical behavior, so there is no need for a replacement.

#### AsIID method
There is no direct replacement for this rarely used function.
If needed, use QueryInterface directly to achieve the same results: `ptr.get()->QueryInterface(iid, reinterpret_cast<void**>(&unknown));`.

#### Implicit bool cast
Wil's bool cast operator is marked as explicit (C++ 11 feature) to avoid questionable type conversions.
This still enables the usual `if (ptr)` checks, but could require a `static_cast<bool>` in some rare situations (such as the pointer being directly assigned to a `bool` variable).

### Compatibility:

WIL's com_ptr<T> fully supports implicit construction, assignment and swap from a compatible WRL ComPtr<T>.




@page page_query COM queries with WIL
@section page_query_key Key Concepts
WIL has several available pivots to consider when querying com pointers:

1.  The `query` vs `copy` methods:

    * Both produce an error (see @ref page_errors "error handling") when the requested interface is unsupported.
    * `query` methods crash if you attempt to query a null pointer
    * `copy` methods return null if you attempt to query a null pointer without an error.
    @par

2.  The `try_xxx` methods (`try_query` and `try_copy`):

    * Never produce an error (using `try_query` on a null pointer will still crash).
    * Return `true` (or a valid pointer) only when the interface is supported.
    @par

3.  The stand-alone functions (`com_xxx`):

    * All `query` and `copy` routines are available as BOTH com_ptr members AND stand-alone functions:
        * `auto foo = m_ptr.query<IFoo>();`
        * `auto foo = wil::com_query<IFoo>(m_ptr);`
    * Stand-alone functions accept raw com pointers, WIL's com_ptr, and WRL's ComPtr.
    @par

4.  The three output patterns:

    * Direct return:  `query<T>()` or `copy<T>()`:
        * Directly returns a wil com_ptr by explicitly stating the desired interface as a template parameter.
            * Member functions return a com_ptr of the same type as queried
                * `auto foo = m_ptr.query<IFoo>();  // foo is the same type as m_ptr`
            * Stand-alone functions return a com_ptr matching the error handling of the function
                * `auto foo = wil::com_query_failfast<IFoo>();  // foo is wil::com_ptr_failfast<IFoo>`
    * Typed out parameter:  `query_to(T**)` or `copy_to(T**)`:
        * Returns the result via an output parameter
        * Deduces the iid for the query from the interface type of the parameter
    * The iid, ppv pattern:  `query_to(refiid, ppv)` or `copy_to(refiid, ppv)`
        * Uses the same pattern as QueryInterface (explicitly specify IID, out parameter is void**)
        * Prefer `query_to(T**)` over this pattern when possible (it performs better)
    @par

5.  Agile and Weak references:

    * All `query` and `copy` routines will resolve references when used against IWeakReference or IAgileReference
    * See @ref wil::com_weak_ref and @ref wil::com_agile_ref (aliases of com_ptr to weak or agile references)
    .
.

@section page_query_ref Available Types and Functions
The three core smart pointer types are:
* @ref wil::com_ptr - Exception-based com pointer
* @ref wil::com_ptr_nothrow - Error-code based com pointer
* @ref wil::com_ptr_failfast - Fail-fast based com pointer

Available query routines for each of these pointer types are:
* @ref wil::com_ptr_t::query "query<T>()" - Query for T, return smart pointer
* @ref wil::com_ptr_t::query_to "query_to(T**)" - Query for T, return out param
* @ref wil::com_ptr_t::query_to "query_to(iid, ppv)" - Query for iid, return out param
@par
* @ref wil::com_ptr_t::try_query "try_query<T>" - Query for T, return smart pointer (null if unsupported)
* @ref wil::com_ptr_t::try_query_to "try_query_to(T**)" - Query for T, return out param (null and returns false if unsupported)
* @ref wil::com_ptr_t::try_query_to "try_query_to(iid, ppv)" - Query for iid, return out param (null and returns false if unsupported)
@par
* @ref wil::com_ptr_t::copy "copy<T>" - Query for T, return smart pointer (preserves null)
* @ref wil::com_ptr_t::copy_to "copy_to(T**)" - Query for T, return out param (preserves null)
* @ref wil::com_ptr_t::copy_to "copy_to(iid, ppv)" - Query for iid, return out param (preserves null)
@par
* @ref wil::com_ptr_t::try_copy "try_copy<T>()" - Query for T, return smart pointer (null if unsupported) (preserves null)
* @ref wil::com_ptr_t::try_copy_to "try_copy_to(T**)" - Query for T, return out param (null and returns false if unsupported) (preserves null)
* @ref wil::com_ptr_t::try_copy_to "try_copy_to(iid, ppv)" - Query for iid, return out param (null and returns false if unsupported) (preserves null)

Available stand-alone query functions are:

* @ref wil::com_query "wil::com_query<T>()" - Query for T, return smart pointer
* @ref wil::com_query_failfast "wil::com_query_failfast<T>()"
* @ref wil::com_query_to "wil::com_query_to(T**)" - Query for T, return out param, throw on failure
* @ref wil::com_query_to_nothrow "wil::com_query_to_nothrow(T**)"
* @ref wil::com_query_to_failfast "wil::com_query_to_failfast(T**)"
* @ref wil::com_query_to "wil::com_query_to(iid, ppv)" - Query for iid, return out param, throw on failure
* @ref wil::com_query_to_nothrow "wil::com_query_to_nothrow(iid, ppv)"
* @ref wil::com_query_to_failfast "wil::com_query_to_failfast(iid, ppv)"
@par
* @ref wil::try_com_query "wil::try_com_query<T>" - Query for T, return smart pointer (null if unsupported)
* @ref wil::try_com_query_to "wil::try_com_query_to(T**)" - Query for T, return out param (null and returns false if unsupported)
* @ref wil::try_com_query_to "wil::try_com_query_to(iid, ppv)" - Query for iid, return out param (null and returns false if unsupported)
@par
* @ref wil::com_copy "wil::com_copy<T>()" - Query for T, return smart pointer (preserves null)
* @ref wil::com_copy_failfast "wil::com_copy_failfast<T>()"
* @ref wil::com_copy_to "wil::com_copy_to(T**)" - Query for T, return out param (preserves null)
* @ref wil::com_copy_to_nothrow "wil::com_copy_to_nothrow(T**)"
* @ref wil::com_copy_to_failfast "wil::com_copy_to_failfast(T**)"
* @ref wil::com_copy_to "wil::com_copy_to(iid, ppv)" - Query for iid, return out param (preserves null)
* @ref wil::com_copy_to_nothrow "wil::com_copy_to_nothrow(iid, ppv)"
* @ref wil::com_copy_to_failfast "wil::com_copy_to_failfast(iid, ppv)"
@par
* @ref wil::try_com_copy "wil::try_com_copy<T>()" - Query for T, return smart pointer (null if unsupported) (preserves null)
* @ref wil::try_com_copy_to "wil::try_com_copy_to(T**)" - Query for T, return out param (null and returns false if unsupported) (preserves null)
* @ref wil::try_com_copy_to "wil::try_com_copy_to(iid, ppv)" - Query for iid, return out param (null and returns false if unsupported) (preserves null)

@par
@section page_query_example Examples
Here are various examples illustrating a subset of the concepts from above in common situations:
~~~~{.h}
HRESULT InputPointerUse(_In_ IUnknown* unknown)
{
    // query and call a single method
    wil::com_query<IFoo>(unknown)->Method1();

    // query and use local result
    auto foo1 = wil::com_query<IFoo>(unknown);
    foo1->Method1();
    foo1->Method2();

    // attempt a query and if supported use it
    auto foo2 = wil::try_com_query<IFoo>(unknown);
    if (foo2)
    {
        foo2->Method1();
    }

    // query to a member variable...
    wil::com_ptr_nothrow<IFoo> m_ptr;
    RETURN_IF_FAILED(wil::com_query_to_nothrow(unknown, &m_ptr));

    return S_OK;
}

HRESULT GetPointer(_COM_Outptr_ IFoo** foo)
{
    // assume member exists and is assigned
    wil::com_ptr_nothrow<IUnknown> m_ptr;

    // query and return the value
    RETURN_IF_FAILED(m_ptr.query_to(foo));

    // stand-alone function can also be used to do the same (this one with fail-fast)
    wil::com_query_to_failfast(m_ptr, foo);

    return S_OK;
}

void MaybeGetPointer(_COM_Outptr_result_maybenull_ IFoo** foo)
{
    // assume member exists and may or may not be assigned
    wil::com_ptr<IUnknown> m_ptr;

    // query and return the value (preserve null)
    m_ptr.copy_to(foo);

    // stand-alone function can also be used to do the same
    wil::com_copy_to(m_ptr, foo);
}

HRESULT GetInterface(REFIID riid, _Outptr_result_nullonfailure_ void** outPtr)
{
    // assume member exists and is assigned
    wil::com_ptr_nothrow<IUnknown> m_ptr;

    // query for the given interface and return
    RETURN_IF_FAILED(m_ptr.query_to(riid, outPtr));

    // stand-alone function to do the same:
    RETURN_IF_FAILED(wil::com_query_to_nothrow(m_ptr, riid, outPtr));

    return S_OK;
}

HRESULT WeakOrAgile(_In_ IUnknown* unknown)
{
    // assume class members
    wil::com_weak_ref weakRef;
    wil::com_agile_ref_failfast agileRef;

    // stash references
    weakRef = wil::com_weak_query(unknown);
    agileRef = wil::com_agile_query_failfast(unknown);

    // resolve references and use...

    weakRef.query<IFoo>()->Method1();
    wil::com_ptr_nothrow<IFoo> foo;
    RETURN_IF_FAILED(wil::com_query_to_nothrow(agileRef, &foo));
    foo->Method1();

    return S_OK;
}
~~~~
