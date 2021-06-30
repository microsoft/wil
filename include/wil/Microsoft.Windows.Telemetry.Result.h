#pragma once
//! @file
//! WIL Error Handling Helpers: a family of macros and functions designed to uniformly handle and report
//! errors across return codes, fail fast, exceptions and logging

#ifndef __MICROSOFT_WINDOWS_TELEMETRY_RESULT_INCLUDED
#define __MICROSOFT_WINDOWS_TELEMETRY_RESULT_INCLUDED

// Common.h contains logic and configuration macros common to all consumers in Windows and therefore needs to be
// included before any 'opensource' headers
#include "Microsoft.Windows.Telemetry.Common.h"

// Note that we avoid pulling in STL's memory header from Result.h through Resource.h as we have
// Result.h customers who are still on older versions of STL (without std::shared_ptr<>).
#pragma push_macro("RESOURCE_SUPPRESS_STL")
#ifndef RESOURCE_SUPPRESS_STL
#define RESOURCE_SUPPRESS_STL
#endif

#include <wil/result.h>

// Internal WIL header files that were historically included before WIL was split into public and internal versions.
// These exist only to maintain back-compat
#include "Microsoft.Windows.Telemetry.Resource.h"

#pragma pop_macro("RESOURCE_SUPPRESS_STL")
#endif // __MICROSOFT_WINDOWS_TELEMETRY_RESULT_INCLUDED
