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
inline bool parseOperatingSettings(const char* text, OperatingSettings& result, int year) {
    int values[12]{};
    const char* p=text;
    for (int i=0;i<12;++i) {
        bool negative=*p=='-'; if (negative) ++p;
        if (*p<'0' || *p>'9') return false;
        int n=0;
        while (*p>='0' && *p<='9') {
            if (n>214748363) return false;
            n=n*10+(*p++-'0');
        }
        values[i]=negative ? -n:n;
        if (i<11) { if (*p++!=',') return false; }
        else if (*p) return false;
    }
    OperatingSettings s;
    s.schema=values[0];s.start=values[1];s.end=values[2];
    s.resetOffset=values[3];s.grace=values[4];s.extension=values[5];
    s.issuedDate=values[6];s.alarmMs=values[7];s.detectMs=values[8];
    s.endDate=values[9];s.downDate=values[10];
    // Final field is the recording year, independent of the show-start date.
    if (values[11]!=year || s.schema!=1 || s.start<0 || s.end<=s.start ||
        s.resetOffset< -5 || s.resetOffset>60 || s.start+s.resetOffset<0 ||
        s.start+s.resetOffset>=s.end || s.grace<0 || s.grace>60 ||
        s.extension<0 || s.extension>30 || s.end+s.grace+30>=1440 ||
        s.alarmMs<5000 || s.alarmMs>300000 || s.detectMs<0 || s.detectMs>3000)
        return false;
    for (int date : {s.issuedDate,s.endDate,s.downDate}) {
        if (date/10000!=year || !validDate(year,date/100%100,date%100)) return false;
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
