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

#pragma once
#ifndef __WIL_CPPWINRT_AUTHORING_INCLUDED
#define __WIL_CPPWINRT_AUTHORING_INCLUDED

namespace wil
{
    template <typename T>
    struct read_only_property
    {
        read_only_property(T value) : m_value(value) { }

        operator T() const noexcept { return m_value; }
        const T& operator()() const noexcept { return m_value; }

        read_only_property(const read_only_property& other) noexcept : m_value(other.m_value) { }
        read_only_property(read_only_property&& other) noexcept : m_value(std::move(other.m_value)) { }

        template<typename... TArgs>
        read_only_property(TArgs&&... args) : m_value(std::forward<TArgs>(args)...) { }

    protected:
        T m_value;
    };

    template <typename T>
    struct read_write_property : read_only_property<T>
    {
        read_write_property(T value) : read_only_property<T>(value) { }

        template<typename... TArgs>
        read_write_property(TArgs&&... args) : read_only_property<T>(std::forward<TArgs>(args)...) { }

        using read_only_property<T>::operator T;
        using read_only_property<T>::operator();
        read_write_property& operator()(const T& value) noexcept
        {
            this->m_value = value;
            return *this;
        }

        read_write_property& operator=(T value) noexcept
        {
            this->m_value = value;
            return *this;
        }

        template<typename U, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
        read_write_property& operator=(const read_only_property<U>& other) noexcept
        {
            this->m_value = other.m_value;
            return *this;
        }

        template<typename U, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
        read_write_property& operator=(read_write_property<U>&& other) noexcept
        {
            this->m_value = std::move(other.m_value);
            return *this;
        }
    };

#pragma region WinRT / XAML helpers
#ifdef WINRT_Windows_Foundation_H

    namespace details {
        template<typename T>
        struct event_base {
            winrt::event_token operator()(T const& handler) {
                return m_handler.add(handler);
            }
            auto operator()(const winrt::event_token& token) noexcept { return m_handler.remove(token); }

            template<typename... TArgs>
            auto invoke(TArgs&&... args) {
                return m_handler(args...);
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
    struct simple_event : details::event_base<winrt::Windows::Foundation::EventHandler<T>> {};

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
    struct typed_event : details::event_base<winrt::Windows::Foundation::TypedEventHandler<TSender, TArgs>> {};

#endif
#pragma endregion

#pragma region INotifyPropertyChanged helpers
#ifdef WINRT_Windows_UI_Xaml_Data_H

    /**
     * @brief Helper base class to inherit from to have a simple implementation of [INotifyPropertyChanged](https://docs.microsoft.com/uwp/api/windows.ui.xaml.data.inotifypropertychanged).
     * @tparam T CRTP type
     * @details When you declare your class, make this class a base class and pass your class as a template parameter:
     * @code
     * struct MyPage : MyPageT<MyPage>, wil::notify_property_changed_base<MyPage> {
     *      wil::property_with_notify<int> MyInt;
     *
     *      MyPage() : INIT_PROPERTY(MyInt, 42) { }
     * };
     * @endcode
    */
    template<typename T,
        typename Xaml_Data_PropertyChangedEventHandler = winrt::Windows::UI::Xaml::Data::PropertyChangedEventHandler
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
        typename Xaml_Data_PropertyChangedEventHandler = winrt::Windows::UI::Xaml::Data::PropertyChangedEventHandler,
        typename Xaml_Data_PropertyChangedEventArgs = winrt::Windows::UI::Xaml::Data::PropertyChangedEventArgs>
    struct property_with_notify : read_write_property<T> {
        using Type = T;

        using read_write_property<T>::operator();

        void operator()(const T& value) {
            if (value != operator()()) {
                read_write_property<T>::operator()(value);
                if (m_npc) {
                    (*m_npc)(m_sender, Xaml_Data_PropertyChangedEventArgs{ m_name });
                }
            }
        }
        template<typename... TArgs>
        property_with_notify(
            winrt::event<Xaml_Data_PropertyChangedEventHandler>* npc,
            winrt::Windows::Foundation::IInspectable sender,
            std::wstring_view name,
            TArgs&&... args) :
            read_write_property<T>(std::forward<TArgs...>(args)...) {
            m_name = name;
            m_npc = npc;
            m_sender = sender;
        }

        property_with_notify(const property_with_notify&) = default;
        property_with_notify(property_with_notify&&) = default;
    private:
        std::wstring m_name;
        winrt::event<Xaml_Data_PropertyChangedEventHandler>* m_npc;
        winrt::Windows::Foundation::IInspectable m_sender;
    };

    /**
    * @def INIT_NOTIFY_PROPERTY
    * @brief use this to initialize a wil::property_with_notify in your class constructor.
    */
#define INIT_NOTIFY_PROPERTY(NAME, VALUE)  \
        NAME(&m_propertyChanged, *this, std::wstring_view{ L#NAME }, VALUE)

#endif // WINRT_Windows_UI_Xaml_Data_H
#pragma endregion // INotifyPropertyChanged helpers
} // namespace wil
#endif // __WIL_CPPWINRT_AUTHORING_INCLUDED