#pragma once
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include "RecordingPolicy.h"
namespace traffic {
// Fixed-width record. Two alternating copies protect the last completed save.
struct CountRecord {
    uint32_t magic, sequence, date, daily, show, showResetDate;
    uint32_t hourly[24];
    uint32_t checksum;
};
static_assert(sizeof(CountRecord)==124, "Count record layout changed; migrate format");
inline uint32_t countChecksum(const CountRecord& r) {
    const uint8_t* p=reinterpret_cast<const uint8_t*>(&r);
    uint32_t hash=2166136261UL;
    for (size_t i=0;i<offsetof(CountRecord,checksum);++i) hash=(hash^p[i])*16777619UL;
    return hash;
}
inline bool countDateValid(uint32_t d) {
    return validDate(d/10000,d/100%100,d%100);
}
inline bool validCountRecord(const CountRecord& r) {
    if (r.magic!=0x4d534231UL || !r.sequence || !countDateValid(r.date) ||
        r.daily>INT_MAX || r.show>INT_MAX ||
        (r.showResetDate && r.showResetDate!=r.date) || r.checksum!=countChecksum(r)) return false;
    for (auto count:r.hourly) if (count>INT_MAX) return false;
    return true;
}
}
