#pragma once
#include <math.h>
#include "RecordingPolicy.h"
namespace traffic {
inline bool validEnvironment(float temperature,float rh) {
    return isfinite(temperature) && isfinite(rh) && temperature>=-40 &&
           temperature<=120 && rh>=0 && rh<=100;
}
// Each reading belongs to a full local date/hour, never merely an hour-of-day.
struct EnvironmentSamples {
    int date=0,hour=-1;
    double sum[24]{};
    unsigned count[24]{};
    double showSum=0;
    unsigned showCount=0;
    bool advance(int nextDate,int nextHour) {
        if (!validDate(nextDate/10000,nextDate/100%100,nextDate%100) ||
            nextHour<0 || nextHour>23 || nextDate<date ||
            (nextDate==date && nextHour<hour)) return false;
        if(nextDate!=date) {
            for(int i=0;i<24;++i) {sum[i]=0;count[i]=0;}
            showSum=0;showCount=0;
        }
        date=nextDate;hour=nextHour;return true;
    }
    bool add(int day,int h,float temperature,float rh,bool official) {
        if(!validEnvironment(temperature,rh) || !advance(day,h)) return false;
        sum[h]+=temperature;++count[h];
        if(official) {showSum+=temperature;++showCount;}
        return true;
    }
};
}
