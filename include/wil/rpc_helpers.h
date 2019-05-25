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
#ifndef __WIL_RPC_HELPERS_INCLUDED
#define __WIL_RPC_HELPERS_INCLUDED

#include "result.h"
#include "resource.h"
#include "wistd_functional.h"
#include "wistd_type_traits.h"

namespace wil
{
    struct default_rpc_policy
    {
    };

    namespace details
    {
        template<typename TReturnType> struct call_adapter
        {
            template<typename... TArgs> static HRESULT call(TArgs&& ... args)
            {
                return wistd::invoke(wistd::forward<TArgs>(args)...);
            }
        };

        template<> struct call_adapter<void>
        {
            template<typename... TArgs> static HRESULT call(TArgs&& ... args)
            {
                wistd::invoke(wistd::forward<TArgs>(args)...);
                return S_OK;
            }
        };

        constexpr static HRESULT map_rpc_exception(DWORD code)
        {
            return IS_ERROR(code) ? code : __HRESULT_FROM_WIN32(code);
        }
    }

    template<typename... TCall> HRESULT invoke_rpc_nothrow(TCall&&... args)
    {
        RpcTryExcept
        {
            // Note: this helper type can be removed with C++17 enabled via
            // 'if constexpr(wistd::is_same_v<void, result_t>)'
            using result_t = typename wistd::__invoke_of<TCall...>::type;
            HRESULT hr = details::call_adapter<result_t>::call(wistd::forward<TCall>(args)...);
            RETURN_HR(hr);
        }
        RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
        {
            RETURN_HR(details::map_rpc_exception(RpcExceptionCode()));
        }
        RpcEndExcept
    }

    template<typename TResult, typename... TCall> HRESULT invoke_rpc_result_nothrow(TResult& result, TCall&&... args)
    {
        RpcTryExcept
        {
            result = wistd::invoke(wistd::forward<TCall>(args)...);
            return S_OK;
        }
        RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
        {
            RETURN_HR(details::map_rpc_exception(RpcExceptionCode()));
        }
        RpcEndExcept
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    template<typename... TCall> void invoke_rpc(TCall&& ... args)
    {
        THROW_IF_FAILED(invoke_rpc_nothrow(wistd::forward<TCall>(args)...));
    }
#endif
}

#endif