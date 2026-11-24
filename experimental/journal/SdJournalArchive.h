#pragma once
#include "JournalStore.h"
#include <stdio.h>
namespace traffic {
// Caller supplies an SD filesystem with exclusive ownership for this operation.
// Internal journal is authoritative. No totals are changed by archive replay.
// Write current recording-year paths only, preserving existing archive protection.
template<class Filesystem> class SdJournalArchive {
    Filesystem& fs;
    uint32_t year;
    const char* root;
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
    SdJournalArchive(Filesystem& filesystem,uint32_t recordingYear,const char* counterRoot)
        :fs(filesystem),year(recordingYear),root(counterRoot){}
    bool isVerified(const JournalRecord& r) {
        if(!validJournalRecord(r) || r.counts.date/10000!=year ||
           (strcmp(root,"CC") && strcmp(root,"GC")))return false;
        char path[96];
        snprintf(path,sizeof(path),"/%s/%lu/TrafficJournal/%010lu.bin",root,
            static_cast<unsigned long>(year),static_cast<unsigned long>(r.sequence));
        return verified(path,r);
    }
    bool storeVerified(const JournalRecord& r) {
        if(!validJournalRecord(r) || r.counts.date/10000!=year ||
           (strcmp(root,"CC") && strcmp(root,"GC")))return false;
        char folder[64],path[96],pending[104];
        snprintf(folder,sizeof(folder),"/%s/%lu/TrafficJournal",root,static_cast<unsigned long>(year));
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
