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
            template<typename TLambda> static HRESULT Call(TLambda&& lambda)
            {
                return lambda();
            }
        };

        template<> struct call_adapter<void>
        {
            template<typename TLambda> static HRESULT Call(TLambda&& lambda)
            {
                lambda();
                return S_OK;
            }
        };

        HRESULT map_rpcexception_code(DWORD exception = GetExceptionCode())
        {
            if (IS_ERROR(exception))
            {
                return static_cast<HRESULT>(exception);
            }
            else
            {
                return __HRESULT_FROM_WIN32(exception);
            }
        }
    }

    template<typename TLambda> HRESULT call_rpc_nothrow(TLambda&& call)
    {
        RpcTryExcept
        {
            HRESULT hr = details::call_adapter<decltype(call())>::Call(call);
            RETURN_HR(hr);
        }
        RpcExcept(I_RpcExceptionFilter(RpcExceptionCode()))
        {
            RETURN_HR(details::map_rpcexception_code());
        }
        RpcEndExcept
    }

    template<typename TLambda, typename TResult> HRESULT call_rpc_nothrow(TResult& result, TLambda&& call)
    {
        RpcTryExcept
        {
            result = call();
            return S_OK;
        }
        RpcExcept(I_RpcExceptionFilter(RpcExceptionCode()))
        {
            RETURN_HR(details::map_rpcexception_code());
        }
        RpcEndExcept
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    template<typename TLambda> void call_rpc(TLambda&& call)
    {
        THROW_IF_FAILED(call_rpc_nothrow(wistd::forward<TLambda>(call)));
    }

    template<typename TResult, typename TLambda> auto call_rpc(TLambda&& call)
    {
        TResult result{};
        THROW_IF_FAILED(call_rpc_nothrow(result, wistd::forward<TLambda>(call)));
        return result;
    }
#endif
}

#endif