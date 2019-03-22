#pragma once

#include <windows.h>

#include "catch.hpp"

// MACRO double evaluation check.
// The following macro illustrates a common problem with writing macros:
//      #define MY_MAX(a, b) (((a) > (b)) ? (a) : (b))
// The issue is that whatever code is being used for both a and b is being executed twice.
// This isn't harmful when thinking of constant numerics, but consider this example:
//      MY_MAX(4, InterlockedIncrement(&cCount))
// This evaluates the (B) parameter twice and results in incrementing the counter twice.
// We use MDEC in unit tests to verify that this kind of pattern is not present.  A test
// of this kind:
//      MY_MAX(MDEC(4), MDEC(InterlockedIncrement(&cCount))
// will verify that the parameters are not evaluated more than once.
#define MDEC(PARAM) (witest::details::MacroDoubleEvaluationCheck(__LINE__, #PARAM), PARAM)

namespace witest
{
    namespace details
    {
        inline void MacroDoubleEvaluationCheck(size_t uLine, _In_ const char* pszCode)
        {
            struct SEval
            {
                size_t uLine;
                const char* pszCode;
            };

            static SEval rgEval[15] = {};
            static size_t nOffset = 0;

            for (auto& eval : rgEval)
            {
                if ((eval.uLine == uLine) && (eval.pszCode != nullptr) && (0 == strcmp(pszCode, eval.pszCode)))
                {
                    // This verification indicates that macro-double-evaluation check is firing for a particular usage of MDEC().
                    FAIL("Expression '" << pszCode << "' double evaluated in macro on line " << uLine);
                }
            }

            rgEval[nOffset].uLine = uLine;
            rgEval[nOffset].pszCode = pszCode;
            nOffset = (nOffset + 1) % ARRAYSIZE(rgEval);
        }

        // template <typename T>
        // class AssignTemporaryValueCleanup
        // {
        // public:
        //     AssignTemporaryValueCleanup(_In_ AssignTemporaryValueCleanup const &) = delete;
        //     AssignTemporaryValueCleanup & operator=(_In_ AssignTemporaryValueCleanup const &) = delete;

        //     explicit AssignTemporaryValueCleanup(_Inout_ T *pVal, T val) WI_NOEXCEPT :
        //         m_pVal(pVal),
        //         m_valOld(*pVal)
        //     {
        //         *pVal = val;
        //     }

        //     AssignTemporaryValueCleanup(_Inout_ AssignTemporaryValueCleanup && other) WI_NOEXCEPT :
        //         m_pVal(other.m_pVal),
        //         m_valOld(other.m_valOld)
        //     {
        //         other.m_pVal = nullptr;
        //     }

        //     ~AssignTemporaryValueCleanup() WI_NOEXCEPT
        //     {
        //         operator()();
        //     }

        //     void operator()() WI_NOEXCEPT
        //     {
        //         if (m_pVal != nullptr)
        //         {
        //             *m_pVal = m_valOld;
        //             m_pVal = nullptr;
        //         }
        //     }

        //     void Dismiss() WI_NOEXCEPT
        //     {
        //         m_pVal = nullptr;
        //     }

        // private:
        //     T *m_pVal;
        //     T m_valOld;
        // };
    }

    //! Global class which tracks objects that derive from @ref AllocatedObject.
    //! Use `witest::g_objectCount.Leaked()` to determine if an object deriving from `AllocatedObject` has been leaked.
    // class GlobalCount
    // {
    // public:
    //     int m_count = 0;

    //     //! Returns `true` if there are any objects that derive from @ref AllocatedObject still in memory.
    //     bool Leaked() const
    //     {
    //         return (m_count != 0);
    //     }

    //     ~GlobalCount()
    //     {
    //         if (Leaked())
    //         {
    //             // GTEST_FATAL_FAILURE_("oops");
    //             __debugbreak();
    //         }
    //     }
    // };
    // __declspec(selectany) GlobalCount g_objectCount;

    //! Derive an allocated test object from witest::AllocatedObject to ensure that those objects aren't leaked in the test.
    //! Note that you can call g_objectCount.Leaked() at any point to determine if a leak has already occurred (assuming that
    //! all objects should have been destroyed at that point.
    // class AllocatedObject
    // {
    // public:
    //     AllocatedObject()   { g_objectCount.m_count++; }
    //     ~AllocatedObject()  { g_objectCount.m_count--; }
    // };

//     template <typename Lambda>
//     bool ThrowsException(Lambda&& callOp)
//     {
// #ifdef WIL_ENABLE_EXCEPTIONS
//         try
// #endif
//         {
//             callOp();
//         }
// #ifdef WIL_ENABLE_EXCEPTIONS
//         catch (...)
//         {
//             return true;
//         }
// #endif

//         return false;
//     }

    // [[noreturn]]
    // inline void __stdcall TranslateFailFastException(PEXCEPTION_RECORD rec, PCONTEXT, DWORD)
    // {
    //     ::RaiseException(rec->ExceptionCode, rec->ExceptionFlags, rec->NumberParameters, rec->ExceptionInformation);
    // }

    // template <typename Lambda>
    // bool ReportsError(wistd::false_type, Lambda&& callOp)
    // {
    //     // We're expecting either an exception to be thrown or a fail fast to occur. Fail fast exceptions are
    //     // non-continuable, and therefore we can't identify it with a __try ... __except. There's also no opportunity
    //     // for us to suppress the exception, so we instead need to "translate" it to a different exception that we _can_
    //     // identify with __try ... __except.
    //     auto restore = wil::details::g_pfnRaiseFailFastException;
    //     wil::details::g_pfnRaiseFailFastException = TranslateFailFastException;

    //     bool result = false;
    //     __try
    //     {
    //         return ThrowsException(callOp);
    //     }
    //     __except(EXCEPTION_EXECUTE_HANDLER)
    //     {
    //         result = true;
    //     }

    //     wil::details::g_pfnRaiseFailFastException = restore;
    //     return result;
    // }

    // template <typename Lambda>
    // bool ReportsError(wistd::true_type, Lambda&& callOp)
    // {
    //     return FAILED(callOp());
    // }
}
