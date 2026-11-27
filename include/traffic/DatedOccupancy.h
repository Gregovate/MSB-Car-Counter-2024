#pragma once
#include "CountRecord.h"
#include <stdio.h>

namespace traffic {
constexpr char datedEntryTopic[]="msb/traffic/CarCounter/Cars/officialEntriesDated";
constexpr char liveEntryTopic[]="msb/traffic/CarCounter/Cars/liveEntriesDated";
struct EntranceReading {
    uint32_t date=0,passages=0,stamp=0;
    bool fresh(uint32_t today,uint32_t now) const {
        // Allow five seconds of inter-device clock skew and four missed 30 s heartbeats.
        return date && date==today && int64_t(now)-stamp>=-5 && int64_t(now)-stamp<=120;
    }
    void encode(char (&out)[48]) const {
        snprintf(out,sizeof(out),"%lu,%lu,%lu",(unsigned long)date,(unsigned long)passages,(unsigned long)stamp);
    }
    bool accept(const char* text,uint32_t today,uint32_t now) {
        EntranceReading next;
        if(!readField(text,',',next.date) || !readField(text,',',next.passages) ||
           !readField(text,0,next.stamp) || next.passages>INT_MAX ||
           !countDateValid(next.date) || !next.fresh(today,now) ||
           (date==next.date && stamp>next.stamp))return false;
        *this=next;return true;
    }
private:
    static bool readField(const char*& p,char delimiter,uint32_t& v) {
        if(*p<'0' || *p>'9')return false;
        while(*p>='0' && *p<='9'){
            uint32_t digit=*p++-'0';
            if(v>(UINT32_MAX-digit)/10)return false;
            v=v*10+digit;
        }
        if(*p!=delimiter)return false;
        if(delimiter)++p;
        return true;
    }
};
}
