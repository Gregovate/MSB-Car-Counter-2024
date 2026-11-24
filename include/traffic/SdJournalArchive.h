#pragma once
#include "JournalStore.h"
#include <stdio.h>
#include <initializer_list>
namespace traffic {
// Caller supplies an SD filesystem with exclusive ownership for this operation.
// Internal journal is authoritative. No totals are changed by archive replay.
// Write current recording-year paths only, preserving existing archive protection.
template<class Filesystem> class SdJournalArchive {
    Filesystem& fs;
    uint32_t year;
    const char* root;
    uint32_t floor;
    const char* suffix(const JournalRecord& r) const {return floor && r.sequence<floor ? "-TEST" : "";}
    bool verifiedPriorYear(const JournalRecord& r) {
        const uint32_t oldYear=r.counts.date/10000;
        if(!r.counts.date || oldYear>=year)return false;
        char path[96];
        for(const char* tail:{"","-TEST"}) {
            snprintf(path,sizeof(path),"/%s/%lu%s/TrafficJournal/%010lu.bin",root,
                (unsigned long)oldYear,tail,(unsigned long)r.sequence);
            if(verified(path,r))return true;
        }
        return false;
    }
    bool verified(const char* path,const JournalRecord& r) {
        auto f=fs.open(path,"r");
        if(!f)return false;
        JournalRecord check{};
        bool ok=f.size()==sizeof(check) &&
            f.read(reinterpret_cast<uint8_t*>(&check),sizeof(check))==sizeof(check) &&
            memcmp(&check,&r,sizeof(check))==0;
        f.close();return ok;
    }
public:
    SdJournalArchive(Filesystem& filesystem,uint32_t recordingYear,const char* counterRoot,uint32_t productionFloor=0)
        :fs(filesystem),year(recordingYear),root(counterRoot),floor(productionFloor){}
    bool isVerified(const JournalRecord& r) {
        if(!validJournalRecord(r) || (strcmp(root,"CC") && strcmp(root,"GC")))return false;
        if(r.counts.date && r.counts.date/10000!=year)return verifiedPriorYear(r);
        char path[96];
        snprintf(path,sizeof(path),"/%s/%lu%s/TrafficJournal/%010lu.bin",root,
            static_cast<unsigned long>(year),suffix(r),static_cast<unsigned long>(r.sequence));
        return verified(path,r);
    }
    bool storeVerified(const JournalRecord& r) {
        if(!validJournalRecord(r) || (strcmp(root,"CC") && strcmp(root,"GC")))return false;
        if(r.counts.date && r.counts.date/10000!=year)return verifiedPriorYear(r);
        char folder[64],path[96],pending[104];
        snprintf(folder,sizeof(folder),"/%s/%lu%s/TrafficJournal",root,static_cast<unsigned long>(year),suffix(r));
        snprintf(path,sizeof(path),"%s/%010lu.bin",folder,static_cast<unsigned long>(r.sequence));
        snprintf(pending,sizeof(pending),"%s.pending",path);
        // The season directory must already exist. Never fabricate historical trees.
        if(!fs.exists(folder) && !fs.mkdir(folder))return false;
        if(fs.exists(path))return verified(path,r); // identical replay: success, no append
        // Only an internal-journal-derived temporary file is replaced. The
        // committed destination is immutable; a conflict stops recovery.
        auto f=fs.open(pending,"w");if(!f)return false;
        size_t n=f.write(reinterpret_cast<const uint8_t*>(&r),sizeof(r));f.flush();f.close();
        if(n!=sizeof(r) || !verified(pending,r))return false;
        if(fs.exists(path))return verified(path,r);
        if(!fs.rename(pending,path))return false;
        return verified(path,r);
    }
};
}
