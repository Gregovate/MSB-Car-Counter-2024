#pragma once
#include <stdint.h>
namespace traffic {
struct JournalPartition {uint32_t type,subtype,address,size;bool encrypted;};
inline bool partitionsOverlap(const JournalPartition& a,const JournalPartition& b) {
    return uint64_t(a.address)<uint64_t(b.address)+b.size && uint64_t(b.address)<uint64_t(a.address)+a.size;
}
inline bool approveJournalPartition(const JournalPartition& data,const JournalPartition& ota0,const JournalPartition& ota1) {
    // ESP-IDF: APP=0, DATA=1, SPIFFS=0x82, OTA_0=0x10, OTA_1=0x11.
    return data.type==1 && data.subtype==0x82 && !data.encrypted &&
        data.size>=1024U*1024U && data.address%4096==0 && data.size%4096==0 &&
        ota0.type==0 && ota0.subtype==0x10 && ota0.size>0 &&
        ota1.type==0 && ota1.subtype==0x11 && ota1.size>0 &&
        uint64_t(data.address)+data.size<=0x100000000ULL &&
        uint64_t(ota0.address)+ota0.size<=0x100000000ULL &&
        uint64_t(ota1.address)+ota1.size<=0x100000000ULL &&
        !partitionsOverlap(data,ota0) && !partitionsOverlap(data,ota1) && !partitionsOverlap(ota0,ota1);
}
}
