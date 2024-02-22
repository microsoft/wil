
#include <wil/Tracelogging.h>

#include <string>

class PROVIDER_CLASS_NAME : wil::TraceLoggingProvider
{
    // 1f9acafe-7501-4da1-84f0-d5312ac4c5fe
    IMPLEMENT_TRACELOGGING_CLASS(
        PROVIDER_CLASS_NAME, "WIL.UnitTests", (0x1f9acafe, 0x7501, 0x4da1, 0x84, 0xf0, 0xd5, 0x31, 0x2a, 0xc4, 0xc5, 0xfe));

public:
    DEFINE_CUSTOM_ACTIVITY(Activity);
    DEFINE_CUSTOM_ACTIVITY(Activity_Params, wil::ActivityOptions::None, WINEVENT_KEYWORD_WDI_DIAG, WINEVENT_LEVEL_VERBOSE);

    BEGIN_CUSTOM_ACTIVITY_CLASS(CustomActivity)
    DEFINE_TAGGED_EVENT_METHOD(Custom)(const std::wstring& str)
    {
        TraceLoggingClassWriteTagged(Custom, TraceLoggingValue(str.c_str(), "str"));
    }
    END_ACTIVITY_CLASS()

    DEFINE_TRACELOGGING_EVENT(Event0);
    DEFINE_TRACELOGGING_EVENT_CV(Event0_CV);
    DEFINE_TRACELOGGING_EVENT_PARAM1(Event1, int, param0);
    DEFINE_TRACELOGGING_EVENT_PARAM1_CV(Event1_CV, int, param0);
    DEFINE_TRACELOGGING_EVENT_PARAM2(Event2, int, param0, double, param1);
    DEFINE_TRACELOGGING_EVENT_PARAM2_CV(Event2_CV, int, param0, double, param1);
    DEFINE_TRACELOGGING_EVENT_PARAM3(Event3, int, param0, double, param1, PCWSTR, param2);
    DEFINE_TRACELOGGING_EVENT_PARAM3_CV(Event3_CV, int, param0, double, param1, PCWSTR, param2);
    DEFINE_TRACELOGGING_EVENT_PARAM4(Event4, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_TRACELOGGING_EVENT_PARAM4_CV(Event4_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_TRACELOGGING_EVENT_PARAM5(Event5, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_TRACELOGGING_EVENT_PARAM5_CV(Event5_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_TRACELOGGING_EVENT_PARAM6(Event6, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_TRACELOGGING_EVENT_PARAM6_CV(Event6_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_TRACELOGGING_EVENT_PARAM7(
        Event7, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_TRACELOGGING_EVENT_PARAM7_CV(
        Event7_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_TRACELOGGING_EVENT_PARAM8(
        Event8, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7);
    DEFINE_TRACELOGGING_EVENT_PARAM8_CV(
        Event8_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7);
    DEFINE_TRACELOGGING_EVENT_PARAM9(
        Event9, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7, float, param8);
    DEFINE_TRACELOGGING_EVENT_PARAM9_CV(
        Event9_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7, float, param8);
    DEFINE_TRACELOGGING_EVENT_PARAM10(
        Event10, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7, float, param8, DWORD, param9);
    DEFINE_TRACELOGGING_EVENT_UINT32(EventUInt32, value);
    DEFINE_TRACELOGGING_EVENT_BOOL(EventBool, value);
    DEFINE_TRACELOGGING_EVENT_STRING(EventString, value);
    DEFINE_EVENT_METHOD(Custom)(const std::wstring& str)
    {
        TraceLoggingWrite(Provider(), "Custom", TraceLoggingValue(str.c_str(), "str"));
    }

    DEFINE_TRACELOGGING_ACTIVITY(TraceloggingActivity);
    DEFINE_TRACELOGGING_ACTIVITY_WITH_LEVEL(TraceloggingActivity_Level, WINEVENT_LEVEL_VERBOSE);

    BEGIN_TRACELOGGING_ACTIVITY_CLASS(CustomTraceloggingActivity)
    DEFINE_TAGGED_TRACELOGGING_EVENT(Event0);
    DEFINE_TAGGED_TRACELOGGING_EVENT_CV(Event0_CV);
    DEFINE_TAGGED_TRACELOGGING_EVENT_PARAM1(Event1, int, param0);
    DEFINE_TAGGED_TRACELOGGING_EVENT_PARAM1_CV(Event1_CV, int, param0);
    DEFINE_TAGGED_TRACELOGGING_EVENT_PARAM2(Event2, int, param0, double, param1);
    DEFINE_TAGGED_TRACELOGGING_EVENT_PARAM2_CV(Event2_CV, int, param0, double, param1);
    DEFINE_TAGGED_TRACELOGGING_EVENT_PARAM3(Event3, int, param0, double, param1, PCWSTR, param2);
    DEFINE_TAGGED_TRACELOGGING_EVENT_PARAM3_CV(Event3_CV, int, param0, double, param1, PCWSTR, param2);
    DEFINE_TAGGED_TRACELOGGING_EVENT_PARAM4(Event4, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_TAGGED_TRACELOGGING_EVENT_PARAM4_CV(Event4_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_TAGGED_TRACELOGGING_EVENT_PARAM5(Event5, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_TAGGED_TRACELOGGING_EVENT_PARAM5_CV(Event5_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_TAGGED_TRACELOGGING_EVENT_PARAM6(Event6, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_TAGGED_TRACELOGGING_EVENT_PARAM6_CV(Event6_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_TAGGED_TRACELOGGING_EVENT_PARAM7(
        Event7, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_TAGGED_TRACELOGGING_EVENT_PARAM7_CV(
        Event7_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_TAGGED_TRACELOGGING_EVENT_PARAM8(
        Event8, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7);
    DEFINE_TAGGED_TRACELOGGING_EVENT_PARAM8_CV(
        Event8_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7);
    DEFINE_TAGGED_TRACELOGGING_EVENT_PARAM9(
        Event9, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7, float, param8);
    DEFINE_TAGGED_TRACELOGGING_EVENT_UINT32(EventUInt32, value);
    DEFINE_TAGGED_TRACELOGGING_EVENT_BOOL(EventBool, value);
    DEFINE_TAGGED_TRACELOGGING_EVENT_STRING(EventString, value);
    END_ACTIVITY_CLASS()

    DEFINE_TELEMETRY_EVENT(TelemetryEvent0);
    DEFINE_TELEMETRY_EVENT_CV(TelemetryEvent0_CV);
    DEFINE_TELEMETRY_EVENT_PARAM1(TelemetryEvent1, int, param0);
    DEFINE_TELEMETRY_EVENT_PARAM1_CV(TelemetryEvent1_CV, int, param0);
    DEFINE_TELEMETRY_EVENT_PARAM2(TelemetryEvent2, int, param0, double, param1);
    DEFINE_TELEMETRY_EVENT_PARAM2_CV(TelemetryEvent2_CV, int, param0, double, param1);
    DEFINE_TELEMETRY_EVENT_PARAM3(TelemetryEvent3, int, param0, double, param1, PCWSTR, param2);
    DEFINE_TELEMETRY_EVENT_PARAM3_CV(TelemetryEvent3_CV, int, param0, double, param1, PCWSTR, param2);
    DEFINE_TELEMETRY_EVENT_PARAM4(TelemetryEvent4, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_TELEMETRY_EVENT_PARAM4_CV(TelemetryEvent4_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_TELEMETRY_EVENT_PARAM5(TelemetryEvent5, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_TELEMETRY_EVENT_PARAM5_CV(TelemetryEvent5_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_TELEMETRY_EVENT_PARAM6(TelemetryEvent6, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_TELEMETRY_EVENT_PARAM6_CV(TelemetryEvent6_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_TELEMETRY_EVENT_PARAM7(
        TelemetryEvent7, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_TELEMETRY_EVENT_PARAM7_CV(
        TelemetryEvent7_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_TELEMETRY_EVENT_PARAM8(
        TelemetryEvent8, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7);
    DEFINE_TELEMETRY_EVENT_PARAM8_CV(
        TelemetryEvent8_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7);
    DEFINE_TELEMETRY_EVENT_UINT32(TelemetryEventUInt32, value);
    DEFINE_TELEMETRY_EVENT_BOOL(TelemetryEventBool, value);
    DEFINE_TELEMETRY_EVENT_STRING(TelemetryEventString, value);

    DEFINE_COMPLIANT_TELEMETRY_EVENT(CompliantTelemetryEvent0, PDT_ProductAndServiceUsage);
    DEFINE_COMPLIANT_TELEMETRY_EVENT_CV(CompliantTelemetryEvent0_CV, PDT_ProductAndServiceUsage);
    DEFINE_COMPLIANT_TELEMETRY_EVENT_PARAM1(CompliantTelemetryEvent1, PDT_ProductAndServiceUsage, int, param0);
    DEFINE_COMPLIANT_TELEMETRY_EVENT_PARAM1_CV(CompliantTelemetryEvent1_CV, PDT_ProductAndServiceUsage, int, param0);
    DEFINE_COMPLIANT_TELEMETRY_EVENT_PARAM2(CompliantTelemetryEvent2, PDT_ProductAndServiceUsage, int, param0, double, param1);
    DEFINE_COMPLIANT_TELEMETRY_EVENT_PARAM2_CV(CompliantTelemetryEvent2_CV, PDT_ProductAndServiceUsage, int, param0, double, param1);
    DEFINE_COMPLIANT_TELEMETRY_EVENT_PARAM3(CompliantTelemetryEvent3, PDT_ProductAndServiceUsage, int, param0, double, param1, PCWSTR, param2);
    DEFINE_COMPLIANT_TELEMETRY_EVENT_PARAM3_CV(CompliantTelemetryEvent3_CV, PDT_ProductAndServiceUsage, int, param0, double, param1, PCWSTR, param2);
    DEFINE_COMPLIANT_TELEMETRY_EVENT_PARAM4(
        CompliantTelemetryEvent4, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_COMPLIANT_TELEMETRY_EVENT_PARAM4_CV(
        CompliantTelemetryEvent4_CV, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_COMPLIANT_TELEMETRY_EVENT_PARAM5(
        CompliantTelemetryEvent5, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_COMPLIANT_TELEMETRY_EVENT_PARAM5_CV(
        CompliantTelemetryEvent5_CV, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_COMPLIANT_TELEMETRY_EVENT_PARAM6(
        CompliantTelemetryEvent6, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_COMPLIANT_TELEMETRY_EVENT_PARAM6_CV(
        CompliantTelemetryEvent6_CV, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_COMPLIANT_TELEMETRY_EVENT_PARAM7(
        CompliantTelemetryEvent7, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_COMPLIANT_TELEMETRY_EVENT_PARAM7_CV(
        CompliantTelemetryEvent7_CV, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_COMPLIANT_TELEMETRY_EVENT_PARAM8(
        CompliantTelemetryEvent8,
        PDT_ProductAndServiceUsage,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4,
        HRESULT,
        param5,
        char,
        param6,
        GUID,
        param7);
    DEFINE_COMPLIANT_TELEMETRY_EVENT_PARAM8_CV(
        CompliantTelemetryEvent8_CV,
        PDT_ProductAndServiceUsage,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4,
        HRESULT,
        param5,
        char,
        param6,
        GUID,
        param7);
    DEFINE_COMPLIANT_TELEMETRY_EVENT_UINT32(CompliantTelemetryEventUInt32, PDT_ProductAndServiceUsage, value);
    DEFINE_COMPLIANT_TELEMETRY_EVENT_BOOL(CompliantTelemetryEventBool, PDT_ProductAndServiceUsage, value);
    DEFINE_COMPLIANT_TELEMETRY_EVENT_STRING(CompliantTelemetryEventString, PDT_ProductAndServiceUsage, value);

    DEFINE_COMPLIANT_EVENTTAGGED_TELEMETRY_EVENT_CV(CompliantEventTaggedTelemetryEvent0_CV, PDT_ProductAndServiceUsage, MICROSOFT_EVENTTAG_MARK_PII);
    DEFINE_COMPLIANT_EVENTTAGGED_TELEMETRY_EVENT_PARAM1_CV(
        CompliantEventTaggedTelemetryEvent1_CV, PDT_ProductAndServiceUsage, MICROSOFT_EVENTTAG_MARK_PII, int, param0);
    DEFINE_COMPLIANT_EVENTTAGGED_TELEMETRY_EVENT_PARAM2_CV(
        CompliantEventTaggedTelemetryEvent2_CV, PDT_ProductAndServiceUsage, MICROSOFT_EVENTTAG_MARK_PII, int, param0, double, param1);
    DEFINE_COMPLIANT_EVENTTAGGED_TELEMETRY_EVENT_PARAM3_CV(
        CompliantEventTaggedTelemetryEvent3_CV, PDT_ProductAndServiceUsage, MICROSOFT_EVENTTAG_MARK_PII, int, param0, double, param1, PCSTR, param2);
    DEFINE_COMPLIANT_EVENTTAGGED_TELEMETRY_EVENT_PARAM4_CV(
        CompliantEventTaggedTelemetryEvent4_CV, PDT_ProductAndServiceUsage, MICROSOFT_EVENTTAG_MARK_PII, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_COMPLIANT_EVENTTAGGED_TELEMETRY_EVENT_PARAM5_CV(
        CompliantEventTaggedTelemetryEvent5_CV,
        PDT_ProductAndServiceUsage,
        MICROSOFT_EVENTTAG_MARK_PII,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4);
    DEFINE_COMPLIANT_EVENTTAGGED_TELEMETRY_EVENT_PARAM6_CV(
        CompliantEventTaggedTelemetryEvent6_CV,
        PDT_ProductAndServiceUsage,
        MICROSOFT_EVENTTAG_MARK_PII,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4,
        HRESULT,
        param5);
    DEFINE_COMPLIANT_EVENTTAGGED_TELEMETRY_EVENT_PARAM7_CV(
        CompliantEventTaggedTelemetryEvent7_CV,
        PDT_ProductAndServiceUsage,
        MICROSOFT_EVENTTAG_MARK_PII,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4,
        HRESULT,
        param5,
        char,
        param6);
    DEFINE_COMPLIANT_EVENTTAGGED_TELEMETRY_EVENT_PARAM8_CV(
        CompliantEventTaggedTelemetryEvent8_CV,
        PDT_ProductAndServiceUsage,
        MICROSOFT_EVENTTAG_MARK_PII,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4,
        HRESULT,
        param5,
        char,
        param6,
        GUID,
        param7);

    DEFINE_TELEMETRY_ACTIVITY(TelemetryActivity);
    DEFINE_COMPLIANT_TELEMETRY_ACTIVITY(CompliantTelemetryActivity, PDT_ProductAndServiceUsage);
    DEFINE_TELEMETRY_ACTIVITY_WITH_LEVEL(TelemetryActivity_Level, WINEVENT_LEVEL_VERBOSE);
    DEFINE_COMPLIANT_TELEMETRY_ACTIVITY_WITH_LEVEL(CompliantTelemetryActivity_Level, PDT_ProductAndServiceUsage, WINEVENT_LEVEL_VERBOSE);

    BEGIN_TELEMETRY_ACTIVITY_CLASS(CustomTelemetryActivity)
    DEFINE_TAGGED_TELEMETRY_EVENT(Event0);
    DEFINE_TAGGED_TELEMETRY_EVENT_CV(Event0_CV);
    DEFINE_TAGGED_TELEMETRY_EVENT_PARAM1(Event1, int, param0);
    DEFINE_TAGGED_TELEMETRY_EVENT_PARAM1_CV(Event1_CV, int, param0);
    DEFINE_TAGGED_TELEMETRY_EVENT_PARAM2(Event2, int, param0, double, param1);
    DEFINE_TAGGED_TELEMETRY_EVENT_PARAM2_CV(Event2_CV, int, param0, double, param1);
    DEFINE_TAGGED_TELEMETRY_EVENT_PARAM3(Event3, int, param0, double, param1, PCWSTR, param2);
    DEFINE_TAGGED_TELEMETRY_EVENT_PARAM3_CV(Event3_CV, int, param0, double, param1, PCWSTR, param2);
    DEFINE_TAGGED_TELEMETRY_EVENT_PARAM4(Event4, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_TAGGED_TELEMETRY_EVENT_PARAM4_CV(Event4_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_TAGGED_TELEMETRY_EVENT_PARAM5(Event5, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_TAGGED_TELEMETRY_EVENT_PARAM5_CV(Event5_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_TAGGED_TELEMETRY_EVENT_PARAM6(Event6, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_TAGGED_TELEMETRY_EVENT_PARAM6_CV(Event6_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_TAGGED_TELEMETRY_EVENT_PARAM7(
        Event7, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_TAGGED_TELEMETRY_EVENT_PARAM7_CV(
        Event7_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_TAGGED_TELEMETRY_EVENT_PARAM8(
        Event8, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7);
    DEFINE_TAGGED_TELEMETRY_EVENT_PARAM8_CV(
        Event8_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7);
    DEFINE_TAGGED_TELEMETRY_EVENT_UINT32(EventUInt32, value);
    DEFINE_TAGGED_TELEMETRY_EVENT_BOOL(EventBool, value);
    DEFINE_TAGGED_TELEMETRY_EVENT_STRING(EventString, value);

    DEFINE_TAGGED_COMPLIANT_TELEMETRY_EVENT(CompliantEvent0, PDT_ProductAndServiceUsage);
    DEFINE_TAGGED_COMPLIANT_TELEMETRY_EVENT_PARAM1(CompliantEvent1, PDT_ProductAndServiceUsage, int, param0);
    DEFINE_TAGGED_COMPLIANT_TELEMETRY_EVENT_PARAM2(CompliantEvent2, PDT_ProductAndServiceUsage, int, param0, double, param1);
    DEFINE_TAGGED_COMPLIANT_TELEMETRY_EVENT_PARAM3(CompliantEvent3, PDT_ProductAndServiceUsage, int, param0, double, param1, PCWSTR, param2);
    DEFINE_TAGGED_COMPLIANT_TELEMETRY_EVENT_PARAM4(
        CompliantEvent4, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_TAGGED_COMPLIANT_TELEMETRY_EVENT_PARAM5(
        CompliantEvent5, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_TAGGED_COMPLIANT_TELEMETRY_EVENT_PARAM6(
        CompliantEvent6, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_TAGGED_COMPLIANT_TELEMETRY_EVENT_PARAM7(
        CompliantEvent7, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_TAGGED_COMPLIANT_TELEMETRY_EVENT_PARAM8(
        CompliantEvent8, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7);
    DEFINE_TAGGED_COMPLIANT_TELEMETRY_EVENT_UINT32(CompliantEventUInt32, PDT_ProductAndServiceUsage, value);
    DEFINE_TAGGED_COMPLIANT_TELEMETRY_EVENT_BOOL(CompliantEventBool, PDT_ProductAndServiceUsage, value);
    DEFINE_TAGGED_COMPLIANT_TELEMETRY_EVENT_STRING(CompliantEventString, PDT_ProductAndServiceUsage, value);
    END_ACTIVITY_CLASS()

    DEFINE_MEASURES_EVENT(MeasuresEvent0);
    DEFINE_MEASURES_EVENT_CV(MeasuresEvent0_CV);
    DEFINE_MEASURES_EVENT_PARAM1(MeasuresEvent1, int, param0);
    DEFINE_MEASURES_EVENT_PARAM1_CV(MeasuresEvent1_CV, int, param0);
    DEFINE_MEASURES_EVENT_PARAM2(MeasuresEvent2, int, param0, double, param1);
    DEFINE_MEASURES_EVENT_PARAM2_CV(MeasuresEvent2_CV, int, param0, double, param1);
    DEFINE_MEASURES_EVENT_PARAM3(MeasuresEvent3, int, param0, double, param1, PCWSTR, param2);
    DEFINE_MEASURES_EVENT_PARAM3_CV(MeasuresEvent3_CV, int, param0, double, param1, PCWSTR, param2);
    DEFINE_MEASURES_EVENT_PARAM4(MeasuresEvent4, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_MEASURES_EVENT_PARAM4_CV(MeasuresEvent4_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_MEASURES_EVENT_PARAM5(MeasuresEvent5, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_MEASURES_EVENT_PARAM5_CV(MeasuresEvent5_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_MEASURES_EVENT_PARAM6(MeasuresEvent6, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_MEASURES_EVENT_PARAM6_CV(MeasuresEvent6_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_MEASURES_EVENT_PARAM7(
        MeasuresEvent7, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_MEASURES_EVENT_PARAM7_CV(
        MeasuresEvent7_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_MEASURES_EVENT_PARAM8(
        MeasuresEvent8, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7);
    DEFINE_MEASURES_EVENT_PARAM8_CV(
        MeasuresEvent8_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7);
    DEFINE_MEASURES_EVENT_UINT32(MeasuresEventUInt32, value);
    DEFINE_MEASURES_EVENT_BOOL(MeasuresEventBool, value);
    DEFINE_MEASURES_EVENT_STRING(MeasuresEventString, value);

    DEFINE_COMPLIANT_MEASURES_EVENT(CompliantMeasuresEvent0, PDT_ProductAndServiceUsage);
    DEFINE_COMPLIANT_MEASURES_EVENT_CV(CompliantMeasuresEvent0_CV, PDT_ProductAndServiceUsage);
    DEFINE_COMPLIANT_MEASURES_EVENT_PARAM1(CompliantMeasuresEvent1, PDT_ProductAndServiceUsage, int, param0);
    DEFINE_COMPLIANT_MEASURES_EVENT_PARAM1_CV(CompliantMeasuresEvent1_CV, PDT_ProductAndServiceUsage, int, param0);
    DEFINE_COMPLIANT_MEASURES_EVENT_PARAM2(CompliantMeasuresEvent2, PDT_ProductAndServiceUsage, int, param0, double, param1);
    DEFINE_COMPLIANT_MEASURES_EVENT_PARAM2_CV(CompliantMeasuresEvent2_CV, PDT_ProductAndServiceUsage, int, param0, double, param1);
    DEFINE_COMPLIANT_MEASURES_EVENT_PARAM3(CompliantMeasuresEvent3, PDT_ProductAndServiceUsage, int, param0, double, param1, PCWSTR, param2);
    DEFINE_COMPLIANT_MEASURES_EVENT_PARAM3_CV(CompliantMeasuresEvent3_CV, PDT_ProductAndServiceUsage, int, param0, double, param1, PCWSTR, param2);
    DEFINE_COMPLIANT_MEASURES_EVENT_PARAM4(
        CompliantMeasuresEvent4, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_COMPLIANT_MEASURES_EVENT_PARAM4_CV(
        CompliantMeasuresEvent4_CV, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_COMPLIANT_MEASURES_EVENT_PARAM5(
        CompliantMeasuresEvent5, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_COMPLIANT_MEASURES_EVENT_PARAM5_CV(
        CompliantMeasuresEvent5_CV, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_COMPLIANT_MEASURES_EVENT_PARAM6(
        CompliantMeasuresEvent6, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_COMPLIANT_MEASURES_EVENT_PARAM6_CV(
        CompliantMeasuresEvent6_CV, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_COMPLIANT_MEASURES_EVENT_PARAM7(
        CompliantMeasuresEvent7, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_COMPLIANT_MEASURES_EVENT_PARAM7_CV(
        CompliantMeasuresEvent7_CV, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_COMPLIANT_MEASURES_EVENT_PARAM8(
        CompliantMeasuresEvent8,
        PDT_ProductAndServiceUsage,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4,
        HRESULT,
        param5,
        char,
        param6,
        GUID,
        param7);
    DEFINE_COMPLIANT_MEASURES_EVENT_PARAM8_CV(
        CompliantMeasuresEvent8_CV,
        PDT_ProductAndServiceUsage,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4,
        HRESULT,
        param5,
        char,
        param6,
        GUID,
        param7);
    DEFINE_COMPLIANT_MEASURES_EVENT_PARAM9(
        CompliantMeasuresEvent9,
        PDT_ProductAndServiceUsage,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4,
        HRESULT,
        param5,
        char,
        param6,
        GUID,
        param7,
        float,
        param8);
    DEFINE_COMPLIANT_MEASURES_EVENT_PARAM10(
        CompliantMeasuresEvent10,
        PDT_ProductAndServiceUsage,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4,
        HRESULT,
        param5,
        char,
        param6,
        GUID,
        param7,
        float,
        param8,
        DWORD,
        param9);
    DEFINE_COMPLIANT_MEASURES_EVENT_UINT32(CompliantMeasuresEventUInt32, PDT_ProductAndServiceUsage, value);
    DEFINE_COMPLIANT_MEASURES_EVENT_BOOL(CompliantMeasuresEventBool, PDT_ProductAndServiceUsage, value);
    DEFINE_COMPLIANT_MEASURES_EVENT_STRING(CompliantMeasuresEventString, PDT_ProductAndServiceUsage, value);

    DEFINE_COMPLIANT_EVENTTAGGED_MEASURES_EVENT_CV(CompliantEventTaggedMeasuresEvent0_CV, PDT_ProductAndServiceUsage, MICROSOFT_EVENTTAG_MARK_PII);
    DEFINE_COMPLIANT_EVENTTAGGED_MEASURES_EVENT_PARAM1_CV(
        CompliantEventTaggedMeasuresEvent1_CV, PDT_ProductAndServiceUsage, MICROSOFT_EVENTTAG_MARK_PII, int, param0);
    DEFINE_COMPLIANT_EVENTTAGGED_MEASURES_EVENT_PARAM2_CV(
        CompliantEventTaggedMeasuresEvent2_CV, PDT_ProductAndServiceUsage, MICROSOFT_EVENTTAG_MARK_PII, int, param0, double, param1);
    DEFINE_COMPLIANT_EVENTTAGGED_MEASURES_EVENT_PARAM3_CV(
        CompliantEventTaggedMeasuresEvent3_CV, PDT_ProductAndServiceUsage, MICROSOFT_EVENTTAG_MARK_PII, int, param0, double, param1, PCSTR, param2);
    DEFINE_COMPLIANT_EVENTTAGGED_MEASURES_EVENT_PARAM4_CV(
        CompliantEventTaggedMeasuresEvent4_CV, PDT_ProductAndServiceUsage, MICROSOFT_EVENTTAG_MARK_PII, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_COMPLIANT_EVENTTAGGED_MEASURES_EVENT_PARAM5_CV(
        CompliantEventTaggedMeasuresEvent5_CV,
        PDT_ProductAndServiceUsage,
        MICROSOFT_EVENTTAG_MARK_PII,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4);
    DEFINE_COMPLIANT_EVENTTAGGED_MEASURES_EVENT_PARAM6_CV(
        CompliantEventTaggedMeasuresEvent6_CV,
        PDT_ProductAndServiceUsage,
        MICROSOFT_EVENTTAG_MARK_PII,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4,
        HRESULT,
        param5);
    DEFINE_COMPLIANT_EVENTTAGGED_MEASURES_EVENT_PARAM7_CV(
        CompliantEventTaggedMeasuresEvent7_CV,
        PDT_ProductAndServiceUsage,
        MICROSOFT_EVENTTAG_MARK_PII,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4,
        HRESULT,
        param5,
        char,
        param6);
    DEFINE_COMPLIANT_EVENTTAGGED_MEASURES_EVENT_PARAM8_CV(
        CompliantEventTaggedMeasuresEvent8_CV,
        PDT_ProductAndServiceUsage,
        MICROSOFT_EVENTTAG_MARK_PII,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4,
        HRESULT,
        param5,
        char,
        param6,
        GUID,
        param7);
    DEFINE_COMPLIANT_EVENTTAGGED_MEASURES_EVENT_PARAM9_CV(
        CompliantEventTaggedMeasuresEvent9_CV,
        PDT_ProductAndServiceUsage,
        MICROSOFT_EVENTTAG_MARK_PII,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4,
        HRESULT,
        param5,
        char,
        param6,
        GUID,
        param7,
        float,
        param8);

    DEFINE_MEASURES_ACTIVITY(MeasuresActivity);
    DEFINE_COMPLIANT_MEASURES_ACTIVITY(CompliantMeasuresActivity, PDT_ProductAndServiceUsage);
    DEFINE_MEASURES_ACTIVITY_WITH_LEVEL(MeasuresActivity_Level, WINEVENT_LEVEL_VERBOSE);
    DEFINE_COMPLIANT_MEASURES_ACTIVITY_WITH_LEVEL(CompliantMeasuresActivity_Level, PDT_ProductAndServiceUsage, WINEVENT_LEVEL_VERBOSE);

    BEGIN_MEASURES_ACTIVITY_CLASS(CustomMeasuresActivity)
    DEFINE_TAGGED_MEASURES_EVENT(Event0);
    DEFINE_TAGGED_MEASURES_EVENT_CV(Event0_CV);
    DEFINE_TAGGED_MEASURES_EVENT_PARAM1(Event1, int, param0);
    DEFINE_TAGGED_MEASURES_EVENT_PARAM1_CV(Event1_CV, int, param0);
    DEFINE_TAGGED_MEASURES_EVENT_PARAM2(Event2, int, param0, double, param1);
    DEFINE_TAGGED_MEASURES_EVENT_PARAM2_CV(Event2_CV, int, param0, double, param1);
    DEFINE_TAGGED_MEASURES_EVENT_PARAM3(Event3, int, param0, double, param1, PCWSTR, param2);
    DEFINE_TAGGED_MEASURES_EVENT_PARAM3_CV(Event3_CV, int, param0, double, param1, PCWSTR, param2);
    DEFINE_TAGGED_MEASURES_EVENT_PARAM4(Event4, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_TAGGED_MEASURES_EVENT_PARAM4_CV(Event4_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_TAGGED_MEASURES_EVENT_PARAM5(Event5, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_TAGGED_MEASURES_EVENT_PARAM5_CV(Event5_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_TAGGED_MEASURES_EVENT_PARAM6(Event6, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_TAGGED_MEASURES_EVENT_PARAM6_CV(Event6_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_TAGGED_MEASURES_EVENT_PARAM7(
        Event7, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_TAGGED_MEASURES_EVENT_PARAM7_CV(
        Event7_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_TAGGED_MEASURES_EVENT_PARAM8(
        Event8, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7);
    DEFINE_TAGGED_MEASURES_EVENT_PARAM8_CV(
        Event8_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7);
    DEFINE_TAGGED_MEASURES_EVENT_UINT32(EventUInt32, value);
    DEFINE_TAGGED_MEASURES_EVENT_BOOL(EventBool, value);
    DEFINE_TAGGED_MEASURES_EVENT_STRING(EventString, value);

    DEFINE_TAGGED_COMPLIANT_MEASURES_EVENT(CompliantEvent0, PDT_ProductAndServiceUsage);
    DEFINE_TAGGED_COMPLIANT_MEASURES_EVENT_PARAM1(CompliantEvent1, PDT_ProductAndServiceUsage, int, param0);
    DEFINE_TAGGED_COMPLIANT_MEASURES_EVENT_PARAM2(CompliantEvent2, PDT_ProductAndServiceUsage, int, param0, double, param1);
    DEFINE_TAGGED_COMPLIANT_MEASURES_EVENT_PARAM3(CompliantEvent3, PDT_ProductAndServiceUsage, int, param0, double, param1, PCWSTR, param2);
    DEFINE_TAGGED_COMPLIANT_MEASURES_EVENT_PARAM4(
        CompliantEvent4, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_TAGGED_COMPLIANT_MEASURES_EVENT_PARAM5(
        CompliantEvent5, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_TAGGED_COMPLIANT_MEASURES_EVENT_PARAM6(
        CompliantEvent6, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_TAGGED_COMPLIANT_MEASURES_EVENT_PARAM7(
        CompliantEvent7, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_TAGGED_COMPLIANT_MEASURES_EVENT_PARAM8(
        CompliantEvent8, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7);
    DEFINE_TAGGED_COMPLIANT_MEASURES_EVENT_UINT32(CompliantEventUInt32, PDT_ProductAndServiceUsage, value);
    DEFINE_TAGGED_COMPLIANT_MEASURES_EVENT_BOOL(CompliantEventBool, PDT_ProductAndServiceUsage, value);
    DEFINE_TAGGED_COMPLIANT_MEASURES_EVENT_STRING(CompliantEventString, PDT_ProductAndServiceUsage, value);
    END_ACTIVITY_CLASS()

    DEFINE_CRITICAL_DATA_EVENT(CriticalDataEvent0);
    DEFINE_CRITICAL_DATA_EVENT_CV(CriticalDataEvent0_CV);
    DEFINE_CRITICAL_DATA_EVENT_PARAM1(CriticalDataEvent1, int, param0);
    DEFINE_CRITICAL_DATA_EVENT_PARAM1_CV(CriticalDataEvent1_CV, int, param0);
    DEFINE_CRITICAL_DATA_EVENT_PARAM2(CriticalDataEvent2, int, param0, double, param1);
    DEFINE_CRITICAL_DATA_EVENT_PARAM2_CV(CriticalDataEvent2_CV, int, param0, double, param1);
    DEFINE_CRITICAL_DATA_EVENT_PARAM3(CriticalDataEvent3, int, param0, double, param1, PCWSTR, param2);
    DEFINE_CRITICAL_DATA_EVENT_PARAM3_CV(CriticalDataEvent3_CV, int, param0, double, param1, PCWSTR, param2);
    DEFINE_CRITICAL_DATA_EVENT_PARAM4(CriticalDataEvent4, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_CRITICAL_DATA_EVENT_PARAM4_CV(CriticalDataEvent4_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_CRITICAL_DATA_EVENT_PARAM5(CriticalDataEvent5, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_CRITICAL_DATA_EVENT_PARAM5_CV(CriticalDataEvent5_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_CRITICAL_DATA_EVENT_PARAM6(CriticalDataEvent6, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_CRITICAL_DATA_EVENT_PARAM6_CV(
        CriticalDataEvent6_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_CRITICAL_DATA_EVENT_PARAM7(
        CriticalDataEvent7, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_CRITICAL_DATA_EVENT_PARAM7_CV(
        CriticalDataEvent7_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_CRITICAL_DATA_EVENT_PARAM8(
        CriticalDataEvent8, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7);
    DEFINE_CRITICAL_DATA_EVENT_PARAM8_CV(
        CriticalDataEvent8_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7);
    DEFINE_CRITICAL_DATA_EVENT_UINT32(CriticalDataEventUInt32, value);
    DEFINE_CRITICAL_DATA_EVENT_BOOL(CriticalDataEventBool, value);
    DEFINE_CRITICAL_DATA_EVENT_STRING(CriticalDataEventString, value);

    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT(CompliantCriticalDataEvent0, PDT_ProductAndServiceUsage);
    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT_CV(CompliantCriticalDataEvent0_CV, PDT_ProductAndServiceUsage);
    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT_PARAM1(CompliantCriticalDataEvent1, PDT_ProductAndServiceUsage, int, param0);
    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT_PARAM1_CV(CompliantCriticalDataEvent1_CV, PDT_ProductAndServiceUsage, int, param0);
    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT_PARAM2(CompliantCriticalDataEvent2, PDT_ProductAndServiceUsage, int, param0, double, param1);
    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT_PARAM2_CV(CompliantCriticalDataEvent2_CV, PDT_ProductAndServiceUsage, int, param0, double, param1);
    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT_PARAM3(
        CompliantCriticalDataEvent3, PDT_ProductAndServiceUsage, int, param0, double, param1, PCWSTR, param2);
    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT_PARAM3_CV(
        CompliantCriticalDataEvent3_CV, PDT_ProductAndServiceUsage, int, param0, double, param1, PCWSTR, param2);
    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT_PARAM4(
        CompliantCriticalDataEvent4, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT_PARAM4_CV(
        CompliantCriticalDataEvent4_CV, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT_PARAM5(
        CompliantCriticalDataEvent5, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT_PARAM5_CV(
        CompliantCriticalDataEvent5_CV, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT_PARAM6(
        CompliantCriticalDataEvent6, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT_PARAM6_CV(
        CompliantCriticalDataEvent6_CV, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT_PARAM7(
        CompliantCriticalDataEvent7, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT_PARAM7_CV(
        CompliantCriticalDataEvent7_CV, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT_PARAM8(
        CompliantCriticalDataEvent8,
        PDT_ProductAndServiceUsage,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4,
        HRESULT,
        param5,
        char,
        param6,
        GUID,
        param7);
    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT_PARAM8_CV(
        CompliantCriticalDataEvent8_CV,
        PDT_ProductAndServiceUsage,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4,
        HRESULT,
        param5,
        char,
        param6,
        GUID,
        param7);
    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT_UINT32(CompliantCriticalDataEventUInt32, PDT_ProductAndServiceUsage, value);
    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT_BOOL(CompliantCriticalDataEventBool, PDT_ProductAndServiceUsage, value);
    DEFINE_COMPLIANT_CRITICAL_DATA_EVENT_STRING(CompliantCriticalDataEventString, PDT_ProductAndServiceUsage, value);

    DEFINE_COMPLIANT_EVENTTAGGED_CRITICAL_DATA_EVENT_CV(CompliantEventTaggedCriticalDataEvent0_CV, PDT_ProductAndServiceUsage, MICROSOFT_EVENTTAG_MARK_PII);
    DEFINE_COMPLIANT_EVENTTAGGED_CRITICAL_DATA_EVENT_PARAM1_CV(
        CompliantEventTaggedCriticalDataEvent1_CV, PDT_ProductAndServiceUsage, MICROSOFT_EVENTTAG_MARK_PII, int, param0);
    DEFINE_COMPLIANT_EVENTTAGGED_CRITICAL_DATA_EVENT_PARAM2_CV(
        CompliantEventTaggedCriticalDataEvent2_CV, PDT_ProductAndServiceUsage, MICROSOFT_EVENTTAG_MARK_PII, int, param0, double, param1);
    DEFINE_COMPLIANT_EVENTTAGGED_CRITICAL_DATA_EVENT_PARAM3_CV(
        CompliantEventTaggedCriticalDataEvent3_CV, PDT_ProductAndServiceUsage, MICROSOFT_EVENTTAG_MARK_PII, int, param0, double, param1, PCWSTR, param2);
    DEFINE_COMPLIANT_EVENTTAGGED_CRITICAL_DATA_EVENT_PARAM4_CV(
        CompliantEventTaggedCriticalDataEvent4_CV,
        PDT_ProductAndServiceUsage,
        MICROSOFT_EVENTTAG_MARK_PII,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3);
    DEFINE_COMPLIANT_EVENTTAGGED_CRITICAL_DATA_EVENT_PARAM5_CV(
        CompliantEventTaggedCriticalDataEvent5_CV,
        PDT_ProductAndServiceUsage,
        MICROSOFT_EVENTTAG_MARK_PII,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4);
    DEFINE_COMPLIANT_EVENTTAGGED_CRITICAL_DATA_EVENT_PARAM6_CV(
        CompliantEventTaggedCriticalDataEvent6_CV,
        PDT_ProductAndServiceUsage,
        MICROSOFT_EVENTTAG_MARK_PII,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4,
        HRESULT,
        param5);
    DEFINE_COMPLIANT_EVENTTAGGED_CRITICAL_DATA_EVENT_PARAM7_CV(
        CompliantEventTaggedCriticalDataEvent7_CV,
        PDT_ProductAndServiceUsage,
        MICROSOFT_EVENTTAG_MARK_PII,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4,
        HRESULT,
        param5,
        char,
        param6);
    DEFINE_COMPLIANT_EVENTTAGGED_CRITICAL_DATA_EVENT_PARAM8_CV(
        CompliantEventTaggedCriticalDataEvent8_CV,
        PDT_ProductAndServiceUsage,
        MICROSOFT_EVENTTAG_MARK_PII,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4,
        HRESULT,
        param5,
        char,
        param6,
        GUID,
        param7);
    DEFINE_COMPLIANT_EVENTTAGGED_CRITICAL_DATA_EVENT_PARAM9_CV(
        CompliantEventTaggedCriticalDataEvent9_CV,
        PDT_ProductAndServiceUsage,
        MICROSOFT_EVENTTAG_MARK_PII,
        int,
        param0,
        double,
        param1,
        PCSTR,
        param2,
        PCWSTR,
        param3,
        bool,
        param4,
        HRESULT,
        param5,
        char,
        param6,
        GUID,
        param7,
        float,
        param8);

    DEFINE_CRITICAL_DATA_ACTIVITY(CriticalDataActivity);
    DEFINE_COMPLIANT_CRITICAL_DATA_ACTIVITY(CompliantCriticalDataActivity, PDT_ProductAndServiceUsage);
    DEFINE_CRITICAL_DATA_ACTIVITY_WITH_LEVEL(CriticalDataActivity_Level, WINEVENT_LEVEL_VERBOSE);
    DEFINE_COMPLIANT_CRITICAL_DATA_ACTIVITY_WITH_LEVEL(CompliantCriticalDataActivity_Level, PDT_ProductAndServiceUsage, WINEVENT_LEVEL_VERBOSE);

    BEGIN_CRITICAL_DATA_ACTIVITY_CLASS(CustomCriticalDataActivity)
    DEFINE_TAGGED_CRITICAL_DATA_EVENT(Event0);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_CV(Event0_CV);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_PARAM1(Event1, int, param0);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_PARAM1_CV(Event1_CV, int, param0);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_PARAM2(Event2, int, param0, double, param1);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_PARAM2_CV(Event2_CV, int, param0, double, param1);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_PARAM3(Event3, int, param0, double, param1, PCWSTR, param2);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_PARAM3_CV(Event3_CV, int, param0, double, param1, PCWSTR, param2);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_PARAM4(Event4, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_PARAM4_CV(Event4_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_PARAM5(Event5, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_PARAM5_CV(Event5_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_PARAM6(Event6, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_PARAM6_CV(
        Event6_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_PARAM7(
        Event7, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_PARAM7_CV(
        Event7_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_PARAM8(
        Event8, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_PARAM8_CV(
        Event8_CV, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_PARAM9(
        Event9, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7, float, param8);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_UINT32(EventUInt32, value);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_BOOL(EventBool, value);
    DEFINE_TAGGED_CRITICAL_DATA_EVENT_STRING(EventString, value);

    DEFINE_TAGGED_COMPLIANT_CRITICAL_DATA_EVENT(CompliantEvent0, PDT_ProductAndServiceUsage);
    DEFINE_TAGGED_COMPLIANT_CRITICAL_DATA_EVENT_PARAM1(CompliantEvent1, PDT_ProductAndServiceUsage, int, param0);
    DEFINE_TAGGED_COMPLIANT_CRITICAL_DATA_EVENT_PARAM2(CompliantEvent2, PDT_ProductAndServiceUsage, int, param0, double, param1);
    DEFINE_TAGGED_COMPLIANT_CRITICAL_DATA_EVENT_PARAM3(CompliantEvent3, PDT_ProductAndServiceUsage, int, param0, double, param1, PCWSTR, param2);
    DEFINE_TAGGED_COMPLIANT_CRITICAL_DATA_EVENT_PARAM4(
        CompliantEvent4, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3);
    DEFINE_TAGGED_COMPLIANT_CRITICAL_DATA_EVENT_PARAM5(
        CompliantEvent5, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4);
    DEFINE_TAGGED_COMPLIANT_CRITICAL_DATA_EVENT_PARAM6(
        CompliantEvent6, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5);
    DEFINE_TAGGED_COMPLIANT_CRITICAL_DATA_EVENT_PARAM7(
        CompliantEvent7, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6);
    DEFINE_TAGGED_COMPLIANT_CRITICAL_DATA_EVENT_PARAM8(
        CompliantEvent8, PDT_ProductAndServiceUsage, int, param0, double, param1, PCSTR, param2, PCWSTR, param3, bool, param4, HRESULT, param5, char, param6, GUID, param7);
    DEFINE_TAGGED_COMPLIANT_CRITICAL_DATA_EVENT_UINT32(CompliantEventUInt32, PDT_ProductAndServiceUsage, value);
    DEFINE_TAGGED_COMPLIANT_CRITICAL_DATA_EVENT_BOOL(CompliantEventBool, PDT_ProductAndServiceUsage, value);
    DEFINE_TAGGED_COMPLIANT_CRITICAL_DATA_EVENT_STRING(CompliantEventString, PDT_ProductAndServiceUsage, value);
    END_ACTIVITY_CLASS()

    DEFINE_CALLCONTEXT_ACTIVITY(CallContextActivity);
    DEFINE_CALLCONTEXT_ACTIVITY_WITH_LEVEL(CallContextActivity_Level, WINEVENT_LEVEL_VERBOSE);

    BEGIN_CALLCONTEXT_ACTIVITY_CLASS(CustomCallContextActivity)
    DEFINE_ACTIVITY_START(int param0, HRESULT param1)
    {
        TELEMETRY_WRITE_ACTIVITY_START(
            CustomCallContextActivity, TraceLoggingValue(param0, "param0"), TraceLoggingHResult(param1, "param1"));
    }
    DEFINE_ACTIVITY_STOP(double param0, GUID param1)
    {
        TELEMETRY_WRITE_ACTIVITY_STOP(CustomCallContextActivity, TraceLoggingValue(param0, "param0"), TraceLoggingValue(param1, "param1"));
    }
    END_ACTIVITY_CLASS()

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    DEFINE_TRACELOGGING_THREAD_ACTIVITY(ThreadActivity);
    DEFINE_TRACELOGGING_THREAD_ACTIVITY_WITH_KEYWORD(ThreadActivity_Keyword, MICROSOFT_KEYWORD_TELEMETRY);
    DEFINE_TRACELOGGING_THREAD_ACTIVITY_WITH_LEVEL(ThreadActivity_Level, WINEVENT_LEVEL_VERBOSE);
    DEFINE_TRACELOGGING_THREAD_ACTIVITY_WITH_KEYWORD_LEVEL(ThreadActivity_KeywordLevel, MICROSOFT_KEYWORD_TELEMETRY, WINEVENT_LEVEL_VERBOSE);
    DEFINE_TELEMETRY_THREAD_ACTIVITY(TelemetryThreadActivity);
#endif
};
