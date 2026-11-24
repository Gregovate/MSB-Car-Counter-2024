#pragma once
#include "EnvironmentPolicy.h"
#include <stdint.h>
#include <stddef.h>
namespace traffic {
struct EnvironmentCheckpoint {
    uint32_t magic=0,sequence=0;
    EnvironmentSamples samples{};
    uint32_t checksum=0;
};
inline uint32_t environmentChecksum(const EnvironmentCheckpoint& s) {
    const auto* p=reinterpret_cast<const uint8_t*>(&s);uint32_t h=2166136261U;
    for(size_t i=0;i<offsetof(EnvironmentCheckpoint,checksum);++i)h=(h^p[i])*16777619U;
    return h;
}
inline bool validEnvironmentCheckpoint(const EnvironmentCheckpoint& s) {
    const auto& v=s.samples;
    if(s.magic!=0x454e5631U || !s.sequence || s.checksum!=environmentChecksum(s) ||
       !validDate(v.date/10000,v.date/100%100,v.date%100) || v.hour<0 || v.hour>23)return false;
    uint64_t total=0;
    for(unsigned i=0;i<24;++i){
        if(!isfinite(v.sum[i]) || v.count[i]>1000000 ||
           v.sum[i]<-40.0*v.count[i] || v.sum[i]>120.0*v.count[i])return false;
        total+=v.count[i];
    }
    return v.showCount<=total && isfinite(v.showSum) &&
        v.showSum>=-40.0*v.showCount && v.showSum<=120.0*v.showCount;
}
}
