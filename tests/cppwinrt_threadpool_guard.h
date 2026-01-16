#pragma once

#include <winrt/base.h>

#include "mocking.h"

// NOTE: Thanks to https://github.com/microsoft/cppwinrt/issues/1513, we need to ensure that COM remains initialized while
// C++/WinRT is running code on the threadpool. This will block in the destructor until all threadpool callbacks are done
// while keeping COM initialized.
struct cppwinrt_threadpool_guard
{
    cppwinrt_threadpool_guard()
    {
        m_detouredSubmit = [this](PTP_SIMPLE_CALLBACK callback, void* ctxt, PTP_CALLBACK_ENVIRON env) -> BOOL {
            auto data = std::make_unique<callback_data>();
            *data = {this, callback, ctxt};

            auto result = ::TrySubmitThreadpoolCallback(detoured_callback, data.get(), env);
            if (result)
            {
                ++m_callsInFlight;
                std::ignore = data.release();
            }

            return result;
        };
    }

    ~cppwinrt_threadpool_guard()
    {
        while (m_callsInFlight > 0)
        {
            // Could use other synchronization, however this is expected to be very short, so spinning is likely better anyway
            std::this_thread::yield();
        }
    }

private:
    struct callback_data
    {
        cppwinrt_threadpool_guard* pThis;
        PTP_SIMPLE_CALLBACK callback;
        void* ctxt;
    };

    static void __stdcall detoured_callback(PTP_CALLBACK_INSTANCE inst, void* ctxt)
    {
        std::unique_ptr<callback_data> data(static_cast<callback_data*>(ctxt));

        // Ensure COM is initialized for the entirety of the callback
        wil::unique_mta_usage_cookie cookie;
        LOG_IF_FAILED(::CoIncrementMTAUsage(cookie.put()));

        data->callback(inst, data->ctxt);
        --data->pThis->m_callsInFlight;
    }

    std::atomic_size_t m_callsInFlight{0};
    witest::detoured_global_function<&::TrySubmitThreadpoolCallback> m_detouredSubmit;
};
