#pragma once

// Suppress 'noreturn' attributes from causing issues
#define RESULT_NORETURN
#define RESULT_NORETURN_NULL
#define RESULT_NORETURN_RESULT(expr) return (expr)
