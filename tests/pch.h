#pragma once

// Suppress 'noreturn' attributes from causing issues
#define RESULT_NORETURN
#define RESULT_NORETURN_NULL
#define RESULT_NORETURN_RESULT(expr) return (expr)

// Definition using '__declspec(selectany)' not compatible with -fno-ms-compatibility
#define XMGLOBALCONST inline constexpr const
