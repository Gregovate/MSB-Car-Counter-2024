#pragma once
#include <stdint.h>
#include <stddef.h>
#include <initializer_list>
#include "RecordingPolicy.h"
namespace traffic {
// One complete HA transaction. No partially applied start/end/extension state.
struct OperatingSettings {
    int schema=1, start=1020, end=1260, resetOffset=-1, grace=10;
    int extension=0, issuedDate=0, alarmMs=60000, detectMs=500;
    int endDate=0, downDate=0;
};
// Optional diagnostics never change the accepted settings or validation policy.
struct OperatingParseDiagnostic {
    const char* reason="OK";
    int field=-1; // Zero-based CSV field; -1 means the whole transaction.
    int issuedDate=0; // Zero means this field has not been decoded.
};
inline bool parseOperatingSettings(const char* text, OperatingSettings& result, int year,
                                   OperatingParseDiagnostic* diagnostic=nullptr) {
    if (diagnostic) *diagnostic=OperatingParseDiagnostic{};
    auto reject=[&](const char* reason,int field) {
        if (diagnostic) { diagnostic->reason=reason;diagnostic->field=field; }
        return false;
    };
    if (!text) return reject("MISSING_PAYLOAD",-1);
    int values[12]{};
    const char* p=text;
    for (int i=0;i<12;++i) {
        bool negative=*p=='-'; if (negative) ++p;
        if (*p<'0' || *p>'9') return reject("EXPECTED_INTEGER",i);
        int n=0;
        while (*p>='0' && *p<='9') {
            if (n>214748363) return reject("INTEGER_OVERFLOW",i);
            n=n*10+(*p++-'0');
        }
        values[i]=negative ? -n:n;
        if (diagnostic && i==6) diagnostic->issuedDate=values[i];
        if (i<11) { if (*p++!=',') return reject("EXPECTED_COMMA",i); }
        else if (*p) return reject("TRAILING_DATA",i);
    }
    OperatingSettings s;
    s.schema=values[0];s.start=values[1];s.end=values[2];
    s.resetOffset=values[3];s.grace=values[4];s.extension=values[5];
    s.issuedDate=values[6];s.alarmMs=values[7];s.detectMs=values[8];
    s.endDate=values[9];s.downDate=values[10];
    // Final field is the recording year, independent of the show-start date.
    if (values[11]!=year) return reject("RECORDING_YEAR_MISMATCH",11);
    if (s.schema!=1) return reject("UNSUPPORTED_SCHEMA",0);
    if (s.start<0 || s.end<=s.start) return reject("INVALID_SHOW_WINDOW",1);
    if (s.resetOffset< -5 || s.resetOffset>60 ||
        int64_t(s.start)+s.resetOffset<0 || int64_t(s.start)+s.resetOffset>=s.end)
        return reject("INVALID_RESET_OFFSET",3);
    if (s.grace<0 || s.grace>60) return reject("INVALID_GRACE",4);
    if (s.extension<0 || s.extension>30) return reject("INVALID_EXTENSION",5);
    if (int64_t(s.end)+s.grace+30>=1440) return reject("END_EXCEEDS_DAY",2);
    if (s.alarmMs<5000 || s.alarmMs>300000) return reject("INVALID_ALARM_TIMEOUT",7);
    if (s.detectMs<0 || s.detectMs>3000) return reject("INVALID_DETECT_TIME",8);
    const int dates[]={s.issuedDate,s.endDate,s.downDate};
    const int fields[]={6,9,10};
    for (int i=0;i<3;++i) {
        int date=dates[i];
        if (date/10000!=year || !validDate(year,date/100%100,date%100))
            return reject("INVALID_DATE",fields[i]);
    }
    result=s;return true;
}
inline int extensionForDate(const OperatingSettings& s, int date) {
    return s.issuedDate==date ? s.extension:0;
}
inline int officialStart(const OperatingSettings& s, bool gate) {
    (void)gate;
    // A positive reset offset starts official counting after opening. A lead
    // reset never admits pre-opening volunteers into the official series.
    return s.start+(s.resetOffset>0 ? s.resetOffset:0);
}
inline int officialEnd(const OperatingSettings& s, int date) {
    return s.end+s.grace+extensionForDate(s,date);
}
inline bool insideOfficialWindow(const OperatingSettings& s,bool gate,int date,int minute) {
    return date<=s.endDate && date!=s.downDate &&
        minute>=officialStart(s,gate) && minute<officialEnd(s,date);
}
}
