#pragma once
#include <stdint.h>
#include <stddef.h>
namespace traffic {
// Interrupted NOR erasure can only change original zero bits into ones. A new
// journal write cannot satisfy this condition at the inspected image header.
// Resume also requires a durable receipt, exact backup SHA and no journal records.
inline bool compatibleInterruptedErase(const uint8_t* observed,const uint8_t* original,size_t bytes) {
    for(size_t i=0;i<bytes;++i)if((observed[i]&original[i])!=original[i])return false;
    return true;
}
inline bool allowInspectedProvision(bool layout,bool backup,bool sourceMatches,bool receipt,
                                    bool eraseCompatible,bool hasJournal,bool baseline) {
    return layout && backup && baseline && !hasJournal && eraseCompatible &&
        (sourceMatches || receipt);
}
}
