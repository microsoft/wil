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

namespace wil
{
#ifndef __WIL_CPPWINRT_AUTHORING_INCLUDED
#define __WIL_CPPWINRT_AUTHORING_INCLUDED
    namespace details
    {
        template<typename T>
        struct single_threaded_property_storage
        {
            T m_value{};
            single_threaded_property_storage() = default;
            single_threaded_property_storage(const T& value) : m_value(value) {}
            operator T& () { return m_value; }
            operator T const& () const { return m_value; }
            template<typename Q> auto operator=(Q&& q)
            {
                m_value = wistd::forward<Q>(q);
                return *this;
            }
        };
    }

    template <typename T>
    struct single_threaded_ro_property : std::conditional_t<std::is_scalar_v<T>, wil::details::single_threaded_property_storage<T>, T>
    {
        single_threaded_ro_property() = default;
        single_threaded_ro_property(const T& t) : base_type(t) { }
        
        const auto& operator()()
        {
            return *this;
        }
        template<typename Q> auto operator=(Q&& q) = delete;
    private:
        using base_type = std::conditional_t<std::is_scalar_v<T>, wil::details::single_threaded_property_storage<T>, T>;
    };

    template <typename T>
    struct single_threaded_rw_property : single_threaded_ro_property<T>
    {
        template<typename... TArgs>
        single_threaded_rw_property(TArgs&&... args) : single_threaded_ro_property<T>(std::forward<TArgs>(args)...) { }

        using single_threaded_ro_property<T>::operator();

        auto& operator()()
        {
            return *this;
        }
        template<typename Q> auto& operator()(Q&& q)
        {
            *this = std::forward<Q>(q);
            return *this;
        }


        template<typename Q> auto operator=(Q&& q)
        {
            return static_cast<T&>(*this) = std::forward<Q>(q);
        }
    };

#endif // __WIL_CPPWINRT_AUTHORING_INCLUDED

#if !defined(__WIL_CPPWINRT_AUTHORING_INCLUDED_FOUNDATION) && defined(WINRT_Windows_Foundation_H) // WinRT / XAML helpers
#define __WIL_CPPWINRT_AUTHORING_INCLUDED_FOUNDATION
    namespace details {
        template<typename T>
        struct event_base {
            winrt::event_token operator()(T&& handler) {
                return m_handler.add(std::forward<T>(handler));
            }
            auto operator()(const winrt::event_token& token) noexcept { return m_handler.remove(token); }

            template<typename... TArgs>
            auto invoke(TArgs&&... args) {
                return m_handler(std::forward<TArgs>(args)...);
            }
        private:
            winrt::event<T> m_handler;
        };
    }

    /**
     * @brief A default event handler that maps to [Windows.Foundation.EventHandler](https://docs.microsoft.com/uwp/api/windows.foundation.eventhandler-1).
     * @tparam T The event data type.
    */
    template<typename T>
    struct simple_event : wil::details::event_base<winrt::Windows::Foundation::EventHandler<T>> {};

    /**
     * @brief A default event handler that maps to [Windows.Foundation.TypedEventHandler](https://docs.microsoft.com/uwp/api/windows.foundation.typedeventhandler-2).
     * @tparam T The event data type.
     * @details Usage example:
     * @code
     *         // In IDL, this corresponds to:
     *         //   event Windows.Foundation.TypedEventHandler<ModalPage, String> OkClicked;
     *         wil::typed_event<MarkupSample::ModalPage, winrt::hstring> OkClicked;
     * @endcode
    */
    template<typename TSender, typename TArgs>
    struct typed_event : wil::details::event_base<winrt::Windows::Foundation::TypedEventHandler<TSender, TArgs>> {};

#endif // !defined(__WIL_CPPWINRT_AUTHORING_INCLUDED_FOUNDATION) && defined(WINRT_Windows_Foundation_H)

#if !defined(__WIL_CPPWINRT_AUTHORING_INCLUDED_XAML_DATA) // INotifyPropertyChanged helpers
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
}

#if defined(WINRT_Microsoft_UI_Xaml_Data_H) || defined(WINRT_Windows_UI_Xaml_Data_H)

    /**
     * @brief Helper base class to inherit from to have a simple implementation of [INotifyPropertyChanged](https://docs.microsoft.com/uwp/api/windows.ui.xaml.data.inotifypropertychanged).
     * @tparam T CRTP type
     * @details When you declare your class, make this class a base class and pass your class as a template parameter:
     * @code
     * struct MyPage : MyPageT<MyPage>, wil::notify_property_changed_base<MyPage> {
     *      wil::single_threaded_notifying_property<int> MyInt;
     *
     *      MyPage() : INIT_PROPERTY(MyInt, 42) { }
     * };
     * @endcode
    */
    template<typename T,
        typename Xaml_Data_PropertyChangedEventHandler = wil::details::Xaml_Data_PropertyChangedEventHandler
        // , typename = std::enable_if_t<std::is_convertible_v<T, winrt::Windows::Foundation::IInspectable>>
    >
    struct notify_property_changed_base {
    public:
        using Type = T;
        auto PropertyChanged(Xaml_Data_PropertyChangedEventHandler const& value) {
            return m_propertyChanged.add(value);
        }
        void PropertyChanged(winrt::event_token const& token) {
            m_propertyChanged.remove(token);
        }
        Type& self() {
            return *static_cast<Type*>(this);
        }

        /**
         * @brief Raises a property change notification event
         * @param name The name of the property
         * @return
         * @details Usage example\n
         * C++
         * @code
         * void MyPage::DoSomething() {
         *  // modify MyInt
         *  // MyInt = ...
         *
         *  // now send a notification to update the bound UI elements
         *  RaisePropertyChanged(L"MyInt");
         * }
         * @endcode
        */
        auto RaisePropertyChanged(std::wstring_view name) {
            return m_propertyChanged(self(), Xaml_Data_PropertyChangedEventHandler{ name });
        }
    protected:
        winrt::event<Xaml_Data_PropertyChangedEventHandler> m_propertyChanged;
    };

    /**
     * @brief Implements a property type with notifications
     * @tparam T the property type
     * @details Use the #INIT_NOTIFY_PROPERTY macro to initialize this property in your class constructor. This will set up the right property name, and bind it to the `notify_property_changed_base` implementation.
    */
    template<typename T,
        typename Xaml_Data_PropertyChangedEventHandler = wil::details::Xaml_Data_PropertyChangedEventHandler,
        typename Xaml_Data_PropertyChangedEventArgs = wil::details::Xaml_Data_PropertyChangedEventArgs>
    struct single_threaded_notifying_property : single_threaded_rw_property<T> {
        using Type = T;

        using single_threaded_rw_property<T>::operator();

        void operator()(const T& value) {
            if (value != this->m_value) {
                single_threaded_rw_property<T>::operator()(value);
                Raise();
            }
        }
        template<typename... TArgs>
        single_threaded_notifying_property(
            winrt::event<Xaml_Data_PropertyChangedEventHandler>* npc,
            winrt::Windows::Foundation::IInspectable sender,
            std::wstring_view name,
            TArgs&&... args) :
            single_threaded_rw_property<T>(std::forward<TArgs...>(args)...),
            m_name(name),
            m_npc(npc),
            m_sender(sender)
        {}

        single_threaded_notifying_property(const single_threaded_notifying_property&) = default;
        single_threaded_notifying_property(single_threaded_notifying_property&&) = default;
        std::wstring_view Name() const noexcept { return m_name; }
        auto& Raise()
        {
            if (m_npc)
            {
                (*m_npc)(m_sender, Xaml_Data_PropertyChangedEventArgs{ m_name });
            }
            return *this;
        }
    private:
        std::wstring_view m_name;
        winrt::event<Xaml_Data_PropertyChangedEventHandler>* m_npc;
        winrt::Windows::Foundation::IInspectable m_sender;
    };

    /**
    * @def INIT_NOTIFY_PROPERTY
    * @brief use this to initialize a wil::single_threaded_notifying_property in your class constructor.
    */
#define INIT_NOTIFY_PROPERTY(NAME, VALUE)  \
        NAME(&m_propertyChanged, *this, L#NAME, VALUE)

    namespace details
    {
        template<typename T>
        inline constexpr bool make_false(T)
        {
            return false;
        }
    }

#if defined(_MSC_VER)
// Gets the name of a member (such as a property) as a constexpr string literal. 
// This is commonly needed when checking which property changed after receiving a PropertyChanged event.
// It will enforce at compile time that propertyName is a member of typeName, then return propertyName as a string literal. 
#define WIL_NAMEOF_MEMBER(typeName, propertyName) \
    __pragma(warning(push)) \
    __pragma(warning(disable:6237)) \
    std::wstring_view( \
        (false && wil::details::make_false(std::add_pointer_t<typeName>{nullptr}->propertyName) \
        ? L"" : (L"" #propertyName))) \
    __pragma(warning(pop))
#elif defined(__clang__)
// Gets the name of a member (such as a property) as a constexpr string literal. 
// This is commonly needed when checking which property changed after receiving a PropertyChanged event.
// It will enforce at compile time that propertyName is a member of typeName, then return propertyName as a string literal. 
#define WIL_NAMEOF_MEMBER(typeName, propertyName) \
    _Pragma("clang diagnostic push") \
    _Pragma("clang diagnostic ignored \"-Wtautological-constant-out-of-range-compare\"") 
    std::wstring_view( \
        (false && wil::details::make_false(std::add_pointer_t<typeName>{nullptr}->propertyName) \
        ? L"" : (L"" #propertyName))) \
    _Pragma("clang diagnostic pop")
#endif

#endif // defined(WINRT_Microsoft_UI_Xaml_Data_H) || defined(WINRT_Windows_UI_Xaml_Data_H)

#endif // !defined(__WIL_CPPWINRT_AUTHORING_INCLUDED_XAML_DATA)
} // namespace wil
