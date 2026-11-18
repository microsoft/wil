#pragma once

// Forward declare since some tests have include order dependencies
extern "C" __declspec(noreturn) void __fastfail(unsigned int);

// The tests want to validate that we fail-fast in specific situations without actually crashing the test process. In order to do
// that, we detour the 'wil::details::ReportFailure_NoReturn<wil::FailureType::FailFast>' function to log the failure and then
// return control back to the caller. This requires us to disable the 'noreturn' attribute on various internal functions that
// would otherwise cause the compiler to insert 'int 3' instructions to ensure that execution does not continue. Unfortunately,
// this is not enough since some compilers will perform static analysis to determine if a function is 'noreturn' even when no
// attribute is present, leading to (1) compiler warnings for unreachable code, and (2) the same 'int 3' instructions in liu of
// something like returning from the function. To work around this, we replace direct calls to the '__fastfail' intrinsic with an
// indirect call through a function pointer that the compiler can't (easily) see through.
inline void (*g_fastfail)(int) = [](int code) {
    __fastfail(code);
};

#define RESULT_NORETURN
#define RESULT_NORETURN_NULL
#define RESULT_NORETURN_RESULT(expr) return (expr)
#define RESULT_FASTFAIL(code) g_fastfail(code)

// Definition using '__declspec(selectany)' not compatible with -fno-ms-compatibility
#define XMGLOBALCONST inline constexpr const
