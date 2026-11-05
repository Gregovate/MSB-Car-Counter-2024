#pragma once
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include "RecordingPolicy.h"
namespace traffic {
// Fixed-width record. Two alternating copies protect the last completed save.
struct LegacyCountRecord {
    uint32_t magic, sequence, date, daily, show, showResetDate;
    uint32_t hourly[24];
    uint32_t checksum;
};
static_assert(sizeof(LegacyCountRecord)==124, "Legacy layout must not change");
struct CountRecord {
    uint32_t magic, sequence, date, daily, show, showResetDate;
    uint32_t hourly[24];
    uint32_t official[24];
    uint32_t officialComplete;
    uint32_t checksum;
};
static_assert(sizeof(CountRecord)==224, "V2 layout must not change");
template<class Record> inline uint32_t countChecksum(const Record& r) {
    const uint8_t* p=reinterpret_cast<const uint8_t*>(&r);
    uint32_t hash=2166136261UL;
    for (size_t i=0;i<offsetof(Record,checksum);++i) hash=(hash^p[i])*16777619UL;
    return hash;
}
inline bool countDateValid(uint32_t d) {
    return validDate(d/10000,d/100%100,d%100);
}
inline bool validCountRecord(const CountRecord& r) {
    if (r.magic!=0x4d534232UL || !r.sequence || !countDateValid(r.date) ||
        r.daily>INT_MAX || r.show>INT_MAX ||
        (r.showResetDate && r.showResetDate!=r.date) || r.checksum!=countChecksum(r)) return false;
    for (auto count:r.hourly) if (count>INT_MAX) return false;
    for (auto count:r.official) if (count>INT_MAX) return false;
    if (r.officialComplete>1) return false;
    return true;
}
}

namespace traffic {
inline bool decodeLegacyCount(const LegacyCountRecord& old, CountRecord& out) {
    if (old.magic!=0x4d534231UL || old.checksum!=countChecksum(old)) return false;
    out={};out.magic=0x4d534232UL;out.sequence=old.sequence;out.date=old.date;
    out.daily=old.daily;out.show=old.show;out.showResetDate=old.showResetDate;
    for(int i=0;i<24;++i)out.hourly[i]=old.hourly[i];
    // Official bins cannot be reconstructed safely from all-day bins.
    out.officialComplete=0;out.checksum=countChecksum(out);
    return validCountRecord(out);
}
}
