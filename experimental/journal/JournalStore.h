#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include "traffic/CountRecord.h"

namespace traffic {
// Experimental storage engine. NOT connected to deployed counting yet.
// All offsets are relative to an existing DATA partition; no absolute flash writes.
// Two 512 KiB banks hold 3,250 full event+count snapshots. Never overwrite a bank.
// SD-verified reclamation/replay and detector integration are separate gates.
constexpr uint32_t journalBankBytes=512U*1024U;
constexpr uint32_t journalHeaderBytes=4096;
constexpr uint32_t journalSlots=(journalBankBytes-journalHeaderBytes)/320;
constexpr uint32_t journalCommit=0x434d4954U;
struct JournalEvent {
    uint32_t bootId,localEpoch,endUptimeMs,passageMs;
    uint32_t followMs,bBrokenMs,betweenMs,clearBeforeMs;
    uint32_t maxSampleGapMs,checkpointMs,mqttMs,flags;
    uint32_t kind,direction,beforeDaily,beforeShow;
};
static_assert(sizeof(JournalEvent)==64,"Fixed event schema");
struct JournalRecord {
    uint32_t magic,sequence,schema,reserved;
    JournalEvent event;
    CountRecord counts;
    uint32_t checksum,commit,padding[2];
};
static_assert(sizeof(JournalRecord)==320,"Fixed recovery record schema");
struct JournalHeader {
    uint32_t magic,schema,bankBytes,recordBytes,bankIndex,reserved,checksum,commit;
};
inline uint32_t journalHash(const void* data,size_t bytes) {
    const auto* p=static_cast<const uint8_t*>(data);uint32_t h=2166136261U;
    for(size_t i=0;i<bytes;++i)h=(h^p[i])*16777619U;
    return h;
}
inline bool validJournalRecord(const JournalRecord& r) {
    return r.magic==0x4d53424aU && r.schema==1 && r.sequence && r.reserved==0 &&
        r.commit==journalCommit && r.checksum==journalHash(&r,offsetof(JournalRecord,checksum)) &&
        validCountRecord(r.counts);
}
inline bool validJournalHeader(const JournalHeader& h,unsigned bank) {
    return h.magic==0x4d534248U && h.schema==1 && h.bankBytes==journalBankBytes &&
        h.recordBytes==sizeof(JournalRecord) && h.bankIndex==bank && h.reserved==0 &&
        h.checksum==journalHash(&h,offsetof(JournalHeader,checksum)) && h.commit==journalCommit;
}
inline bool journalErased(const void* data,size_t bytes) {
    const auto* p=static_cast<const uint8_t*>(data);
    for(size_t i=0;i<bytes;++i)if(p[i]!=0xff)return false;
    return true;
}
enum class JournalHealth {NotOpened,Ready,ForeignData,ReadFailed,WriteFailed,Corrupt,Full,LayoutRejected};
enum class JournalAppend {Durable,Failed,Full,NotReady};
// Store implements read(offset,buffer,size) and write(offset,buffer,size).
// No erase method is available to this engine. A failed append requires reopen.
template<class Store> class JournalStore {
    Store& store;
    bool bankReady[2]{false,false};
    uint32_t nextSlot[2]{0,0};
    uint32_t latestSequence=0;
    JournalRecord latestRecord{};
    bool ready=false;
    JournalHealth status=JournalHealth::NotOpened;
    bool readBankErased(unsigned b,uint32_t start) {
        uint8_t buffer[256];
        for(uint32_t p=start;p<journalBankBytes;p+=sizeof(buffer)) {
            if(!store.read(b*journalBankBytes+p,buffer,sizeof(buffer))) {status=JournalHealth::ReadFailed;return false;}
            if(!journalErased(buffer,sizeof(buffer))) {status=JournalHealth::ForeignData;return false;}
        }
        return true;
    }
    bool initializeBank(unsigned b) {
        if(!readBankErased(b,0))return false;
        JournalHeader h{};h.magic=0x4d534248U;h.schema=1;h.bankBytes=journalBankBytes;
        h.recordBytes=sizeof(JournalRecord);h.bankIndex=b;
        h.checksum=journalHash(&h,offsetof(JournalHeader,checksum));h.commit=journalCommit;
        uint32_t off=b*journalBankBytes;
        if(!store.write(off,&h,offsetof(JournalHeader,commit)) ||
           !store.write(off+offsetof(JournalHeader,commit),&h.commit,sizeof(h.commit))) {
            status=JournalHealth::WriteFailed;return false;
        }
        JournalHeader check{};
        if(!store.read(off,&check,sizeof(check)) || !validJournalHeader(check,b)) {
            status=JournalHealth::WriteFailed;return false;
        }
        bankReady[b]=true;return true;
    }
public:
    explicit JournalStore(Store& s):store(s){}
    JournalHealth health()const{return status;}
    uint32_t sequence()const{return latestSequence;}
    const JournalRecord* latest()const{return latestSequence?&latestRecord:nullptr;}
    uint32_t availableSlots()const{return 2*journalSlots-nextSlot[0]-nextSlot[1];}
    // Caller must verify DATA partition, both OTA slots, nonoverlap, unencrypted
    // partition, and >=1 MiB before allowing ANY access. Runtime adapter enforces it.
    bool open(bool approvedLayout) {
        ready=false;status=JournalHealth::NotOpened;latestSequence=0;latestRecord={};
        bankReady[0]=bankReady[1]=false;nextSlot[0]=nextSlot[1]=0;
        if(!approvedLayout){status=JournalHealth::LayoutRejected;return false;}
        // Read/validate both banks before initializing either. Unknown bytes are
        // preserved. Partial headers fail closed rather than formatting evidence.
        for(unsigned b=0;b<2;++b) {
            JournalHeader h{};
            if(!store.read(b*journalBankBytes,&h,sizeof(h))){status=JournalHealth::ReadFailed;return false;}
            if(journalErased(&h,sizeof(h))) {
                if(!readBankErased(b,0))return false;
                continue;
            }
            if(!validJournalHeader(h,b)){status=JournalHealth::ForeignData;return false;}
            bankReady[b]=true;
            for(uint32_t i=0;i<journalSlots;++i) {
                JournalRecord r{};
                if(!store.read(b*journalBankBytes+journalHeaderBytes+i*sizeof(r),&r,sizeof(r))) {
                    status=JournalHealth::ReadFailed;return false;
                }
                if(journalErased(&r,sizeof(r)))continue;
                nextSlot[b]=i+1; // Torn slots are consumed forever, never overwritten.
                if(r.commit!=journalCommit)continue; // Interrupted append, not durable.
                if(!validJournalRecord(r) || r.sequence<=latestSequence) {
                    status=JournalHealth::Corrupt;return false;
                }
                latestSequence=r.sequence;latestRecord=r;
            }
        }
        ready=true;status=JournalHealth::Ready;return true;
    }
    // Walk durable records in their sequence order. The archive adapter MUST
    // use the sequence as an immutable identity and verify the entire record.
    // Replaying after reboot is then idempotent; this engine never edits totals.
    template<class Archive> bool replay(Archive& archive) {
        if(!ready)return false;
        for(unsigned b=0;b<2;++b) {
            if(!bankReady[b])continue;
            for(uint32_t i=0;i<nextSlot[b];++i) {
                JournalRecord r{};
                if(!store.read(b*journalBankBytes+journalHeaderBytes+i*sizeof(r),&r,sizeof(r))) {
                    status=JournalHealth::ReadFailed;ready=false;return false;
                }
                if(journalErased(&r,sizeof(r)) || r.commit!=journalCommit)continue;
                if(!validJournalRecord(r)){status=JournalHealth::Corrupt;ready=false;return false;}
                // A conflict, short write, or failed readback stops replay.
                // Neither an SD failure nor repeat replay consumes flash records.
                if(!archive.storeVerified(r))return false;
            }
        }
        return true;
    }
    JournalAppend append(const JournalEvent& event,const CountRecord& counts) {
        if(!ready)return JournalAppend::NotReady;
        if(!validCountRecord(counts) || latestSequence==UINT32_MAX){status=JournalHealth::Corrupt;ready=false;return JournalAppend::Failed;}
        unsigned b=nextSlot[0]<journalSlots?0:1;
        if(nextSlot[b]>=journalSlots){status=JournalHealth::Full;return JournalAppend::Full;}
        if(!bankReady[b] && !initializeBank(b)){ready=false;return JournalAppend::Failed;}
        JournalRecord r{};r.magic=0x4d53424aU;r.schema=1;r.sequence=latestSequence+1;
        r.event=event;r.counts=counts;
        r.checksum=journalHash(&r,offsetof(JournalRecord,checksum));r.commit=journalCommit;
        uint32_t off=b*journalBankBytes+journalHeaderBytes+nextSlot[b]*sizeof(r);
        ++nextSlot[b];
        // Commit marker is programmed LAST. A reboot never accepts an incomplete
        // record as a count. After any error reopen to resolve uncertain commit.
        if(!store.write(off,&r,offsetof(JournalRecord,commit)) ||
           !store.write(off+offsetof(JournalRecord,commit),&r.commit,sizeof(r.commit))) {
            status=JournalHealth::WriteFailed;ready=false;return JournalAppend::Failed;
        }
        JournalRecord check{};
        if(!store.read(off,&check,sizeof(check)) || !validJournalRecord(check) ||
           memcmp(&check,&r,offsetof(JournalRecord,commit)+sizeof(r.commit))) {
            status=JournalHealth::WriteFailed;ready=false;return JournalAppend::Failed;
        }
        latestSequence=r.sequence;latestRecord=check;return JournalAppend::Durable;
    }
};
}
