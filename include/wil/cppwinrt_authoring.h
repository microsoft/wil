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
//! Helpers that make authoring C++/WinRT components easier.

namespace wil
{
#ifndef __WIL_CPPWINRT_AUTHORING_PROPERTIES_INCLUDED
/// @cond
#define __WIL_CPPWINRT_AUTHORING_PROPERTIES_INCLUDED
namespace details
{
    template <typename T>
    struct single_threaded_property_storage
    {
        T m_value{};
        single_threaded_property_storage() = default;
        single_threaded_property_storage(const T& value) : m_value(value)
        {
        }
        operator T&()
        {
            return m_value;
        }
        operator T const&() const
        {
            return m_value;
        }
        template <typename Q>
        auto operator=(Q&& q)
        {
            m_value = wistd::forward<Q>(q);
            return *this;
        }
    };
} // namespace details
/// @endcond

/** A read-only C++/WinRT property backed by single-threaded (non-atomic) storage.
Wraps a value of type `T` and exposes it as a read-only property on a C++/WinRT runtime class: `operator()` returns the value (the
getter C++/WinRT calls), and no public call-style setter is exposed. Set the value from within your implementation with
`operator=`. For a property that callers can also set, use @ref single_threaded_rw_property instead.
~~~
struct MyClass : MyClassT<MyClass>
{
    wil::single_threaded_property<int> Answer{42}; // a read-only Answer property

    void Recompute()
    {
        Answer = 43; // set internally with operator=; Answer(43) intentionally does not compile
    }
};
~~~
@tparam T The property's value type. */
template <typename T>
struct single_threaded_property
    : std::conditional_t<std::is_scalar_v<T> || std::is_final_v<T>, wil::details::single_threaded_property_storage<T>, T>
{
    single_threaded_property() = default;
    //! Constructs the property, forwarding the arguments to initialize the underlying value.
    //! @tparam TArgs Constructor argument types for `T` (or its storage).
    //! @param value Arguments used to initialize the property's value.
    template <typename... TArgs>
    single_threaded_property(TArgs&&... value) : base_type(std::forward<TArgs>(value)...)
    {
    }

    /// @cond
    using base_type =
        std::conditional_t<std::is_scalar_v<T> || std::is_final_v<T>, wil::details::single_threaded_property_storage<T>, T>;
    /// @endcond

    //! Returns the current property value.
    //! This is the getter C++/WinRT invokes for the projected property.
    //! @return A copy of the current value.
    T operator()() const
    {
        return *this;
    }

    //! Sets the property value from within your implementation.
    //! This is the only exposed setter; there is deliberately no call-style `operator()(Q&&)` setter, because C++/WinRT uses that
    //! form to expose *public* setters and this type models a read-only property. Write `MyProperty = 42;` (not
    //! `MyProperty(42)`). For a publicly settable property, use @ref single_threaded_rw_property.
    //! @tparam Q The assigned value type.
    //! @param q The new value to store.
    //! @return A reference to this property.
    template <typename Q>
    auto& operator=(Q&& q)
    {
        static_cast<base_type&>(*this) = std::forward<Q>(q);
        return *this;
    }
};

/** A read-write C++/WinRT property backed by single-threaded (non-atomic) storage.
Wraps a value of type `T` and exposes it as a read-write property on a C++/WinRT runtime class: `operator()` with no argument gets
the value, `operator()(value)` sets it (the public setter C++/WinRT calls), and `operator=` sets it from within your
implementation. For a read-only property, use @ref single_threaded_property instead.
~~~
struct MyClass : MyClassT<MyClass>
{
    wil::single_threaded_rw_property<winrt::hstring> Title{L"Untitled"}; // a read-write Title property
};
~~~
@tparam T The property's value type. */
template <typename T>
struct single_threaded_rw_property : single_threaded_property<T>
{
    /// @cond
    using base_type = single_threaded_property<T>;
    /// @endcond
    //! Constructs the property, forwarding the arguments to initialize the underlying value.
    //! @tparam TArgs Constructor argument types for `T`.
    //! @param value Arguments used to initialize the property's value.
    template <typename... TArgs>
    single_threaded_rw_property(TArgs&&... value) : base_type(std::forward<TArgs>(value)...)
    {
    }

    using base_type::operator();

    // needed in lieu of deducing-this
    //! Sets the property value; this is the public setter C++/WinRT invokes when a caller assigns the property.
    //! @tparam Q The assigned value type.
    //! @param q The new value to store.
    //! @return A reference to this property.
    template <typename Q>
    auto& operator()(Q&& q)
    {
        return *this = std::forward<Q>(q);
    }

    // needed in lieu of deducing-this
    //! Sets the property value from within your implementation (for example `Title = L"Home";`).
    //! @tparam Q The assigned value type.
    //! @param q The new value to store.
    //! @return A reference to this property.
    template <typename Q>
    auto& operator=(Q&& q)
    {
        base_type::operator=(std::forward<Q>(q));
        return *this;
    }
};

#endif // __WIL_CPPWINRT_AUTHORING_PROPERTIES_INCLUDED

#if (!defined(__WIL_CPPWINRT_AUTHORING_INCLUDED_ICLASSFACTORY) && defined(__IClassFactory_INTERFACE_DEFINED__)) || \
    defined(WIL_DOXYGEN) // class factory
/// @cond
#define __WIL_CPPWINRT_AUTHORING_INCLUDED_ICLASSFACTORY
/// @endcond

/** A COM class factory (`IClassFactory`) that creates instances of a C++/WinRT (`winrt::implements`) class.
Creates instances of `T` with `winrt::make_self<T>`, and implements `LockServer` against the C++/WinRT module lock so the host
module stays loaded while the factory is locked. Hand it to whatever needs an `IClassFactory` (for example
`CoRegisterClassObject`, or a `DllGetClassObject` implementation); @ref register_com_server wraps the `CoRegisterClassObject` case
for you.
@tparam T The C++/WinRT class the factory creates.
@tparam Rest Additional C++/WinRT `implements` markers for the factory (not interfaces to implement), appended to its
             `winrt::implements` base. For example @ref register_com_server passes `winrt::no_module_lock` so the factory object
             itself holds no module lock. */
template <typename T, typename... Rest>
struct class_factory : winrt::implements<class_factory<T, Rest...>, IClassFactory, winrt::no_weak_ref, Rest...>
{
    //! Creates an instance of `T` and returns the requested interface.
    //! Implements `IClassFactory::CreateInstance`. Aggregation is not supported, so a non-null `outer` fails with
    //! `CLASS_E_NOAGGREGATION`.
    //! @param outer The aggregating object's controlling `IUnknown`; must be null, since aggregation is unsupported.
    //! @param iid Interface to return in `result`.
    //! @param result Receives the requested interface on success, or null on failure.
    //! @return `S_OK` on success; `CLASS_E_NOAGGREGATION` if `outer` is non-null; otherwise a failure `HRESULT`.
    HRESULT __stdcall CreateInstance(IUnknown* outer, GUID const& iid, void** result) noexcept final
    try
    {
        *result = nullptr;

        if (!outer)
        {
            return winrt::make_self<T>().as(iid, result);
        }
        else
        {
            return CLASS_E_NOAGGREGATION;
        }
    }
    CATCH_RETURN()

    //! Increments or decrements the C++/WinRT module lock to keep the host module loaded.
    //! Implements `IClassFactory::LockServer`.
    //! @param lock `TRUE` to take a module lock, `FALSE` to release one.
    //! @return `S_OK` - this function cannot fail.
    HRESULT __stdcall LockServer(BOOL lock) noexcept final
    try
    {
        if (lock)
        {
            ++winrt::get_module_lock();
        }
        else
        {
            --winrt::get_module_lock();
        }

        return S_OK;
    }
    CATCH_RETURN()
};

#endif // !defined(__WIL_CPPWINRT_AUTHORING_INCLUDED_ICLASSFACTORY) && defined(__IClassFactory_INTERFACE_DEFINED__)

#if (!defined(__WIL_CPPWINRT_AUTHORING_INCLUDED_COM_SERVER) && defined(__WIL_CPPWINRT_AUTHORING_INCLUDED_ICLASSFACTORY) && defined(__WIL__COMBASEAPI_H_)) || \
    defined(WIL_DOXYGEN) // COM server
/// @cond
#define __WIL_CPPWINRT_AUTHORING_INCLUDED_COM_SERVER
/// @endcond

/** Registers a C++/WinRT class as a COM class object for an out-of-process server, returning an RAII un-registration cookie.
Creates a @ref class_factory for `T` and registers it with `CoRegisterClassObject`. The returned cookie unregisters the class
object when it is destroyed, so keep it alive for as long as the class should remain creatable.
@tparam T The C++/WinRT class to register.
@param guid The CLSID to register the class under.
@param context The `CLSCTX` execution context; defaults to `CLSCTX_LOCAL_SERVER`.
@param flags The `REGCLS` registration flags; defaults to `REGCLS_MULTIPLEUSE`.
@return A `wil::unique_com_class_object_cookie` that unregisters the class object when destroyed.
@note Throws a `winrt::hresult_error` if registration fails. */
template <typename T>
WI_NODISCARD_REASON("The class is unregistered when the returned value is destructed")
unique_com_class_object_cookie
    register_com_server(GUID const& guid, DWORD context = CLSCTX_LOCAL_SERVER, DWORD flags = REGCLS_MULTIPLEUSE)
{
    unique_com_class_object_cookie registration;
    winrt::check_hresult(CoRegisterClassObject(
        guid, winrt::make<class_factory<T, winrt::no_module_lock>>().get(), context, flags, registration.put()));
    return registration;
}

/** Registers several C++/WinRT classes as COM class objects at once, returning a `std::vector` of RAII un-registration cookies.
Registers each class in `Ts...` under the matching CLSID in the `guids` array. Keep the returned cookies alive for as long as the
classes should remain creatable.
@tparam Ts The C++/WinRT classes to register, in the same order as `guids`.
@param guids The CLSIDs to register the classes under, one per class in `Ts`.
@param context The `CLSCTX` execution context; defaults to `CLSCTX_LOCAL_SERVER`.
@param flags The `REGCLS` registration flags; defaults to `REGCLS_MULTIPLEUSE`.
@return A `std::vector` of `wil::unique_com_class_object_cookie`, one per registered class.
@note Throws a `winrt::hresult_error` if any registration fails. */
template <typename... Ts>
WI_NODISCARD_REASON("The classes are unregistered when the returned value is destructed")
std::vector<unique_com_class_object_cookie> register_com_server(
    std::array<GUID, sizeof...(Ts)> const& guids, DWORD context = CLSCTX_LOCAL_SERVER, DWORD flags = REGCLS_MULTIPLEUSE)
{
    std::vector<wil::unique_com_class_object_cookie> registrations;
    registrations.reserve(sizeof...(Ts));

    std::size_t i = 0;
    (registrations.push_back(wil::register_com_server<Ts>(guids[i++], context, flags | REGCLS_SUSPENDED)), ...);

    // allow the user to keep class objects suspended if they've explicitly passed REGCLS_SUSPENDED.
    if (!WI_IsFlagSet(flags, REGCLS_SUSPENDED))
    {
        winrt::check_hresult(CoResumeClassObjects());
    }

    return registrations;
}

#endif // (!defined(__WIL_CPPWINRT_AUTHORING_INCLUDED_COM_SERVER) && defined(__WIL_CPPWINRT_AUTHORING_INCLUDED_ICLASSFACTORY) && defined(__WIL__COMBASEAPI_H_))

#if (!defined(__WIL_CPPWINRT_AUTHORING_INCLUDED_FOUNDATION) && defined(WINRT_Windows_Foundation_H)) || \
    defined(WIL_DOXYGEN) // WinRT / XAML helpers
/// @cond
#define __WIL_CPPWINRT_AUTHORING_INCLUDED_FOUNDATION
namespace details
{
    template <typename T>
    struct event_base
    {
        winrt::event_token operator()(const T& handler)
        {
            return m_handler.add(handler);
        }

        auto operator()(const winrt::event_token& token) noexcept
        {
            return m_handler.remove(token);
        }

        template <typename... TArgs>
        auto invoke(TArgs&&... args)
        {
            return m_handler(std::forward<TArgs>(args)...);
        }

    private:
        winrt::event<T> m_handler;
    };
} // namespace details
/// @endcond

/**
 * @brief A default event handler that maps to
 * [Windows.Foundation.EventHandler](https://docs.microsoft.com/uwp/api/windows.foundation.eventhandler-1).
 * @tparam T The event data type.
 */
template <typename T>
struct untyped_event : wil::details::event_base<winrt::Windows::Foundation::EventHandler<T>>
{
};

/**
 * @brief A default event handler that maps to
 * [Windows.Foundation.TypedEventHandler](https://docs.microsoft.com/uwp/api/windows.foundation.typedeventhandler-2).
 * @tparam T The event data type.
 * @details Usage example:
 * @code
 *         // In IDL, this corresponds to:
 *         //   event Windows.Foundation.TypedEventHandler<ModalPage, String> OkClicked;
 *         wil::typed_event<MarkupSample::ModalPage, winrt::hstring> OkClicked;
 * @endcode
 */
template <typename TSender, typename TArgs>
struct typed_event : wil::details::event_base<winrt::Windows::Foundation::TypedEventHandler<TSender, TArgs>>
{
};

#endif // !defined(__WIL_CPPWINRT_AUTHORING_INCLUDED_FOUNDATION) && defined(WINRT_Windows_Foundation_H)

#if (!defined(__WIL_CPPWINRT_AUTHORING_INCLUDED_XAML_DATA) && (defined(WINRT_Microsoft_UI_Xaml_Data_H) || defined(WINRT_Windows_UI_Xaml_Data_H))) || \
    defined(WIL_DOXYGEN) // INotifyPropertyChanged helpers
/// @cond
#define __WIL_CPPWINRT_AUTHORING_INCLUDED_XAML_DATA
namespace details
{
#ifdef WINRT_Microsoft_UI_Xaml_Data_H
    using Xaml_Data_PropertyChangedEventHandler = winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler;
    using Xaml_Data_PropertyChangedEventArgs = winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs;
#elif defined(WINRT_Windows_UI_Xaml_Data_H)
    using Xaml_Data_PropertyChangedEventHandler = winrt::Windows::UI::Xaml::Data::PropertyChangedEventHandler;
    using Xaml_Data_PropertyChangedEventArgs = winrt::Windows::UI::Xaml::Data::PropertyChangedEventArgs;
#endif
} // namespace details
/// @endcond

/**
 * @brief Helper base class to inherit from to have a simple implementation of
 * [INotifyPropertyChanged](https://docs.microsoft.com/uwp/api/windows.ui.xaml.data.inotifypropertychanged).
 * @tparam T CRTP type
 * @details When you declare your class, make this class a base class and pass your class as a template parameter:
 * @code
 * struct MyPage : MyPageT<MyPage>, wil::notify_property_changed_base<MyPage>
 * {
 *     wil::single_threaded_notifying_property<int> MyInt;
 *     MyPage() : INIT_NOTIFYING_PROPERTY(MyInt, 42) { }
 *     // or
 *     WIL_NOTIFYING_PROPERTY(int, MyInt, 42);
 * };
 * @endcode
 */
template <typename T, typename Xaml_Data_PropertyChangedEventHandler = wil::details::Xaml_Data_PropertyChangedEventHandler, typename Xaml_Data_PropertyChangedEventArgs = wil::details::Xaml_Data_PropertyChangedEventArgs>
struct notify_property_changed_base
{
    /// @cond
    using Type = T;
    /// @endcond

    //! Registers a handler for the `INotifyPropertyChanged.PropertyChanged` event.
    //! @param value The handler to add.
    //! @return An `event_token` identifying the registration, for later removal with the token overload.
    auto PropertyChanged(Xaml_Data_PropertyChangedEventHandler const& value)
    {
        return m_propertyChanged.add(value);
    }

    //! Unregisters a previously registered `PropertyChanged` handler.
    //! @param token The token returned when the handler was registered.
    void PropertyChanged(winrt::event_token const& token)
    {
        m_propertyChanged.remove(token);
    }

    /// @cond
    Type& self()
    {
        return *static_cast<Type*>(this);
    }
    /// @endcond

    /**
     * @brief Raises a property change notification event
     * @param name The name of the property
     * @return
     * @details Usage example\n
     * C++
     * @code
     * void MyPage::DoSomething()
     * {
     *     // modify MyInt
     *     // MyInt = ...
     *
     *     // now send a notification to update the bound UI elements
     *     RaisePropertyChanged(L"MyInt");
     * }
     * @endcode
     */
    auto RaisePropertyChanged(std::wstring_view name)
    {
        return m_propertyChanged(self(), Xaml_Data_PropertyChangedEventArgs{name});
    }

    /// @cond
protected:
    winrt::event<Xaml_Data_PropertyChangedEventHandler> m_propertyChanged;
    /// @endcond
};

/**
 * @brief Implements a property type with notifications
 * @tparam T the property type
 * @details Use the INIT_NOTIFY_PROPERTY macro to initialize this property in your class constructor. This will set up the
 *          right property name, and bind it to the `notify_property_changed_base` implementation.
 */
template <typename T, typename Xaml_Data_PropertyChangedEventHandler = wil::details::Xaml_Data_PropertyChangedEventHandler, typename Xaml_Data_PropertyChangedEventArgs = wil::details::Xaml_Data_PropertyChangedEventArgs>
struct single_threaded_notifying_property : single_threaded_rw_property<T>
{
    /// @cond
    using Type = T;
    using base_type = single_threaded_rw_property<T>;
    /// @endcond
    using base_type::operator();

    //! Sets the property value and raises a change notification if the value changed.
    //! This is the public C++/WinRT setter.
    //! @tparam Q The assigned value type.
    //! @param q The new value to store.
    //! @return A reference to this property.
    template <typename Q>
    auto& operator()(Q&& q)
    {
        return *this = std::forward<Q>(q);
    }

    //! Sets the property value and raises a change notification if the value changed.
    //! @tparam Q The assigned value type.
    //! @param q The new value to store.
    //! @return A reference to this property.
    template <typename Q>
    auto& operator=(Q&& q)
    {
        if (q != this->operator()())
        {
            static_cast<base_type&>(*this) = std::forward<Q>(q);
            if (auto strong = m_sender.get(); (m_npc != nullptr) && (strong != nullptr))
            {
                (*m_npc)(strong, Xaml_Data_PropertyChangedEventArgs{m_name});
            }
        }
        return *this;
    }

    //! Constructs a notifying property bound to its owner's `PropertyChanged` event.
    //! Usually invoked for you by the `INIT_NOTIFYING_PROPERTY` macro rather than directly.
    //! ~~~
    //! struct MyPage : MyPageT<MyPage>, wil::notify_property_changed_base<MyPage>
    //! {
    //!     wil::single_threaded_notifying_property<int> Value;
    //!
    //!     // INIT_NOTIFYING_PROPERTY binds Value to this type's PropertyChanged event and names it "Value",
    //!     // expanding to Value(&m_propertyChanged, *this, L"Value", 42):
    //!     MyPage() : INIT_NOTIFYING_PROPERTY(Value, 42) { }
    //! };
    //! ~~~
    //! @tparam TArgs Argument types used to initialize the property's value.
    //! @param npc The owner's `PropertyChanged` event to raise when the value changes.
    //! @param sender The object reported as the sender of the change notification (typically the owning control).
    //! @param name The property name reported in the change notification.
    //! @param args Arguments used to initialize the property's value.
    template <typename... TArgs>
    single_threaded_notifying_property(
        winrt::event<Xaml_Data_PropertyChangedEventHandler>* npc,
        const winrt::Windows::Foundation::IInspectable& sender,
        std::wstring_view name,
        TArgs&&... args) :
        single_threaded_rw_property<T>(std::forward<TArgs...>(args)...), m_name(name), m_npc(npc), m_sender(sender)
    {
    }

    //! Copy-constructs the property, including its value and change-notification binding.
    single_threaded_notifying_property(const single_threaded_notifying_property&) = default;
    //! Move-constructs the property from another instance.
    single_threaded_notifying_property(single_threaded_notifying_property&&) = default;
    //! Returns the property name reported in change notifications.
    //! @return The property name, as a `std::wstring_view` valid for the lifetime of this property.
    std::wstring_view Name() const noexcept
    {
        return m_name;
    }

private:
    std::wstring_view m_name;
    winrt::event<Xaml_Data_PropertyChangedEventHandler>* m_npc;
    winrt::weak_ref<winrt::Windows::Foundation::IInspectable> m_sender;
};

/**
 * @def WIL_NOTIFYING_PROPERTY
 * @brief use this to stamp out a property that calls RaisePropertyChanged upon changing its value. This is a zero-storage
 *        alternative to wil::single_threaded_notifying_property<T>.
 * @details You can pass an initializer list for the initial property value in the variadic arguments to this macro.
 */
#define WIL_NOTIFYING_PROPERTY(type, name, ...) \
    type m_##name{__VA_ARGS__}; \
    auto name() const noexcept \
    { \
        return m_##name; \
    } \
    auto& name(type value) \
    { \
        if (m_##name != value) \
        { \
            m_##name = std::move(value); \
            RaisePropertyChanged(L"" #name); \
        } \
        return *this; \
    }

/**
 * @def INIT_NOTIFYING_PROPERTY
 * @brief use this to initialize a wil::single_threaded_notifying_property in your class constructor.
 */
#define INIT_NOTIFYING_PROPERTY(NAME, VALUE) NAME(&m_propertyChanged, *this, L"" #NAME, VALUE)

#endif // !defined(__WIL_CPPWINRT_AUTHORING_INCLUDED_XAML_DATA) && (defined(WINRT_Microsoft_UI_Xaml_Data_H) || defined(WINRT_Windows_UI_Xaml_Data_H))
} // namespace wil
