#include "pch.h"

// Just verify that Tracelogging.h compiles.
#define PROVIDER_CLASS_NAME TestProvider_PartB

#define _GENERIC_PARTB_FIELDS_ENABLED \
    TraceLoggingWideString(L"1.0.0", "version"), TraceLoggingInt32(1337, "build"), TraceLoggingBool(true, "isInternal")

#include "TraceLoggingTests.h"
