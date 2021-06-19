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
#ifndef __WIL_COM_APARTMENT_VARIABLE_INCLUDED
#define __WIL_COM_APARTMENT_VARIABLE_INCLUDED

#include <unordered_map>
#include <any>
#include <type_traits>
#include <functional>
#include "cppwinrt.h"
#include <roapi.h>
#include <objidl.h>
#include "result_macros.h"

#ifndef WIL_ENABLE_EXCEPTIONS
#error This header requires exceptions
#endif

namespace wil
{
    // Determine if apartment variables are supported in the current process context.
    // Prior to https://microsoft.visualstudio.com/OS/_git/os.2020/pullrequest/5750493 the
    // APIs needed to implement apartment variables did not work in non-packaged processes.
    inline bool are_apartment_variables_supported()
    {
        unsigned long long apartmentId{};
        return RoGetApartmentIdentifier(&apartmentId) != HRESULT_FROM_WIN32(ERROR_API_UNAVAILABLE);
    }

    namespace details
    {
        inline winrt::slim_mutex s_lock;
        // Apartment id -> variable storage. Variables are stored using the address of
        // the function that produces the value as the key.
        inline std::unordered_map<unsigned long long, std::unordered_map<const void*, std::any>> s_apartmentStorage;

        inline unsigned long long GetApartmentId()
        {
            unsigned long long apartmentId{};
            FAIL_FAST_IF_FAILED(RoGetApartmentIdentifier(&apartmentId));
            return apartmentId;
        }

        // Type erased form of the get_for_current_com_apartment to avoid template bloat.
        inline std::any& get_for_current_com_apartment(void const* variableKey,
            const std::function<std::any()>& createAny)
        {
            const auto apartmentId = GetApartmentId();

            auto lock = winrt::slim_lock_guard(details::s_lock);

            auto storage = details::s_apartmentStorage.find(apartmentId);
            if (storage != details::s_apartmentStorage.end())
            {
                auto& variables = storage->second;
                auto variable = variables.find(variableKey);
                if (variable != variables.end())
                {
                    return variable->second;
                }
                else
                {
                    variable = variables.insert({ variableKey, createAny() }).first;
                    return variable->second;
                }
            }
            else
            {
                struct ApartmentObserver : public winrt::implements<ApartmentObserver, IApartmentShutdown>
                {
                    void STDMETHODCALLTYPE OnUninitialize(unsigned long long apartmentId) noexcept override
                    {
                        // This code runs at apartment rundown so be careful to avoid deadlocks by
                        // extracting the variables under the lock then release them outside.
                        auto variables = [apartmentId]()
                        {
                            auto lock = winrt::slim_lock_guard(details::s_lock);
                            return details::s_apartmentStorage.extract(apartmentId);
                        }();
                    }
                };

                auto observer = winrt::make<ApartmentObserver>();
                unsigned long long id{};
                APARTMENT_SHUTDOWN_REGISTRATION_COOKIE cookie{}; // leaked, COM run down releases all instances
                THROW_IF_FAILED(RoRegisterForApartmentShutdown(observer.get(), &id, &cookie));
                FAIL_FAST_IF(apartmentId != id);

                auto variables = details::s_apartmentStorage.insert({ apartmentId,
                    { { variableKey, createAny() } } }).first;
                return variables->second.begin()->second; // new map entry is first
            }
        }

        inline void reset_for_current_com_apartment(void const* variableKey)
        {
            const auto apartmentId = GetApartmentId();
            auto lock = winrt::slim_lock_guard(details::s_lock);
            auto storage = details::s_apartmentStorage.find(apartmentId);
            if (storage != details::s_apartmentStorage.end())
            {
                auto& variables = storage->second;
                variables.erase(variableKey);
            }
        }
        // Replace a variable with a new value. This requires (ensured via fail fast) that the
        // variable is already stored based on a call to get_for_current_com_apartment in the
        // current apartment.
        inline void reset_for_current_com_apartment(void const* variableKey, std::any&& newAny)
        {
            const auto apartmentId = GetApartmentId();

            // release newAny, with the swapped value, outside of the lock
            {
                auto lock = winrt::slim_lock_guard(details::s_lock);
                auto storage = details::s_apartmentStorage.find(apartmentId);
                FAIL_FAST_IF(storage == details::s_apartmentStorage.end());
                auto& variables = storage->second;
                auto variable = variables.find(variableKey);
                FAIL_FAST_IF(variable == variables.end());
                variable->second.swap(newAny);
            }
        }
    }

    // Get the apartment variable, returning the value stored in the per apartment
    // storage or use the create function to create and store it in the current apartment.
    // This variables lifetime is tied to the COM apartment lifetime and will
    // be safely cleaned up. Call reset_for_current_com_apartment if you want to
    // rundown the variable before the apartment exits.
    template<typename F>
    auto& get_for_current_com_apartment(F const* createFn)
    {
        return std::any_cast<std::invoke_result_t<F>&>(details::get_for_current_com_apartment(std::addressof(createFn), [createFn]()
        {
            return createFn();
        }));
    }

    // Remove the variable including its entry in the map. This has no
    // effect if the variable was not stored based on a call to get_for_current_com_apartment
    // from the current apartment.
    template<typename F>
    void reset_for_current_com_apartment(F const* createFn)
    {
        details::reset_for_current_com_apartment(std::addressof(createFn));
    }

    // Replace a variable with a new value. This requires (ensured via fail fast) that the
    // variable is already stored based on a call to get_for_current_com_apartment in the
    // current apartment.
    template<typename F>
    void reset_for_current_com_apartment(F const* createFn, std::invoke_result_t<F>&& newValue)
    {
        details::reset_for_current_com_apartment(std::addressof(createFn), std::forward<std::invoke_result_t<F>>(newValue));
    }

    // for testing
    size_t apartment_count() { return details::s_apartmentStorage.size(); }
    size_t apartment_variable_count()
    {
        const auto apartmentId = details::GetApartmentId();

        auto lock = winrt::slim_lock_guard(details::s_lock);
        auto storage = details::s_apartmentStorage.find(apartmentId);
        if (storage != details::s_apartmentStorage.end())
        {
            return storage->second.size();
        }
        return 0;
    }
}

#endif // __WIL_COM_APARTMENT_VARIABLE_INCLUDED
