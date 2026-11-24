#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include "traffic/CountRecord.h"

namespace traffic {
// Internal count/event journal. Commit markers and snapshots recover independently of SD.
// All offsets are relative to an existing DATA partition; no absolute flash writes.
// Two 512 KiB banks hold 3,250 full event+count snapshots. Never overwrite records.
// SD-verified reclamation/replay and detector integration are separate gates.
constexpr uint32_t journalBankBytes=512U*1024U;
constexpr uint32_t journalHeaderBytes=4096;
constexpr uint32_t journalSlots=(journalBankBytes-journalHeaderBytes)/320;
constexpr uint32_t journalCommit=0x434d4954U;
struct JournalEvent {
    uint32_t bootId,localEpoch,endUptimeMs,passageMs;
    uint32_t followMs,bBrokenMs,betweenMs,clearBeforeMs;
    uint32_t maxSampleGapMs,checkpointMs,correctionEpoch,flags;
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
struct ReclaimReceipt {uint32_t magic,victim,victimLast,keepLast,keepCount,reserved,checksum,commit;};
static_assert(sizeof(ReclaimReceipt)==32,"Fixed erase receipt");
inline uint32_t journalHash(const void* data,size_t bytes) {
    const auto* p=static_cast<const uint8_t*>(data);uint32_t h=2166136261U;
    for(size_t i=0;i<bytes;++i)h=(h^p[i])*16777619U;
    return h;
}
inline const CountRecord& zeroCountRecord(){static const CountRecord zero{};return zero;}
inline bool validJournalRecord(const JournalRecord& r) {
    return r.magic==0x4d53424aU && (r.schema==1 || r.schema==2) && r.sequence && (r.schema==2 || r.reserved==0) &&
        r.commit==journalCommit && r.checksum==journalHash(&r,offsetof(JournalRecord,checksum)) &&
        (validCountRecord(r.counts) || (r.event.kind==2 && memcmp(&r.counts,&zeroCountRecord(),sizeof(r.counts))==0));
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
// Reclamation also requires erase(offset,size), gated by verified archive records.
// A failed append or erase requires reopen; partial bank headers fail closed.
template<class Store> class JournalStore {
    Store& store;
    bool bankReady[2]{false,false};
    uint32_t firstSequence[2]{0,0},lastSequence[2]{0,0};
    uint32_t nextSlot[2]{0,0};
    uint32_t latestSequence=0;
    JournalRecord latestRecord{};
    CountRecord latestCounts{};
    uint32_t pending=0,correctionEpoch=0;
    uint32_t cursorAfter=0,cursorSlot=0,cursorGeneration=0;unsigned cursorBank=0;
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
    bool receiptValid(const ReclaimReceipt& r,unsigned victim) {
        return r.magic==0x4d534252U && r.victim==victim && r.victimLast &&
            r.keepLast>r.victimLast && r.keepCount && r.reserved==0 && r.commit==journalCommit &&
            r.checksum==journalHash(&r,offsetof(ReclaimReceipt,checksum));
    }
    bool writeReceipt(unsigned victim) {
        ReclaimReceipt r{};r.magic=0x4d534252U;r.victim=victim;r.victimLast=lastSequence[victim];
        r.keepLast=latestSequence;r.keepCount=latestCounts.sequence;
        r.checksum=journalHash(&r,offsetof(ReclaimReceipt,checksum));r.commit=journalCommit;
        unsigned keep=1-victim;
        for(uint32_t p=64;p+sizeof(r)<=journalHeaderBytes;p+=sizeof(r)) {
            ReclaimReceipt old{};uint32_t at=keep*journalBankBytes+p;
            if(!store.read(at,&old,sizeof(old)))return false;
            if(!journalErased(&old,sizeof(old)))continue; // Never overwrite torn receipts.
            if(!store.write(at,&r,offsetof(ReclaimReceipt,commit)) ||
               !store.write(at+offsetof(ReclaimReceipt,commit),&r.commit,sizeof(r.commit)))return false;
            ReclaimReceipt check{};
            return store.read(at,&check,sizeof(check)) && memcmp(&check,&r,sizeof(r))==0;
        }
        return false;
    }
    bool recoverErases() {
        // A committed receipt proves that every victim record was archived and
        // a newer count snapshot was retained before erase began. Only that
        // exact older generation may be erased again after interruption.
        for(unsigned victim=0;victim<2;++victim) {
            unsigned keep=1-victim;JournalHeader h{};
            if(!store.read(keep*journalBankBytes,&h,sizeof(h))){status=JournalHealth::ReadFailed;return false;}
            if(!validJournalHeader(h,keep))continue;
            ReclaimReceipt receipt{};
            for(uint32_t p=64;p+sizeof(receipt)<=journalHeaderBytes;p+=sizeof(receipt)) {
                ReclaimReceipt r{};
                if(!store.read(keep*journalBankBytes+p,&r,sizeof(r))){status=JournalHealth::ReadFailed;return false;}
                if(receiptValid(r,victim) && r.keepLast>receipt.keepLast)receipt=r;
            }
            if(!receipt.magic)continue;
            uint32_t keepLast=0,keepCount=0;bool newerVictim=false;
            for(uint32_t i=0;i<journalSlots;++i) {
                JournalRecord r{};
                if(!store.read(keep*journalBankBytes+journalHeaderBytes+i*sizeof(r),&r,sizeof(r))){status=JournalHealth::ReadFailed;return false;}
                if(r.commit==journalCommit) {
                    if(!validJournalRecord(r)){status=JournalHealth::Corrupt;return false;}
                    if(r.sequence>keepLast)keepLast=r.sequence;
                    if(validCountRecord(r.counts) && r.counts.sequence>keepCount)keepCount=r.counts.sequence;
                }
                if(!store.read(victim*journalBankBytes+journalHeaderBytes+i*sizeof(r),&r,sizeof(r))){status=JournalHealth::ReadFailed;return false;}
                if(validJournalRecord(r) && r.sequence>receipt.victimLast)newerVictim=true;
            }
            if(newerVictim)continue; // Receipt cannot authorize erasing a reused bank.
            if(keepLast<receipt.keepLast || keepCount<receipt.keepCount){status=JournalHealth::Corrupt;return false;}
            // Avoid an unnecessary erase if the previous erase completed.
            if(readBankErased(victim,0))continue;
            if(status==JournalHealth::ReadFailed)return false;
            if(!store.erase(victim*journalBankBytes,journalBankBytes) || !readBankErased(victim,0)) {
                status=JournalHealth::WriteFailed;return false;
            }
        }
        return true;
    }
public:
    explicit JournalStore(Store& s):store(s){}
    JournalHealth health()const{return status;}
    uint32_t sequence()const{return latestSequence;}
    // Zero means no safe retirement boundary is available. Only sequences
    // strictly below this boundary are absent from BOTH live flash banks.
    uint32_t oldestRetainedSequence()const {
        if(!ready)return 0;
        if(!firstSequence[0])return firstSequence[1];
        if(!firstSequence[1])return firstSequence[0];
        return firstSequence[0]<firstSequence[1]?firstSequence[0]:firstSequence[1];
    }
    uint32_t bankLastSequence(unsigned bank)const{return bank<2?lastSequence[bank]:0;}
    const JournalRecord* latest()const{return latestSequence?&latestRecord:nullptr;}
    const CountRecord* snapshot()const{return latestCounts.date?&latestCounts:nullptr;}
    uint32_t pendingCount()const{return pending;}
    uint32_t correctionNonce()const{return correctionEpoch;}
    void carryPending(uint32_t value){if(value>pending)pending=value;}
    // One bounded archive operation per service pass, never a whole-night replay.
    bool readAfter(uint32_t after,JournalRecord& out) {
        if(!ready)return false;
        unsigned b=2;
        for(unsigned candidate=0;candidate<2;++candidate)
            if(lastSequence[candidate]>after && (b==2 || firstSequence[candidate]<firstSequence[b]))b=candidate;
        if(b==2)return false;
        uint32_t start=cursorAfter==after && cursorBank==b && cursorGeneration==firstSequence[b]?cursorSlot:0;
        for(uint32_t i=start;i<nextSlot[b];++i) {
            JournalRecord r{};
            if(!store.read(b*journalBankBytes+journalHeaderBytes+i*sizeof(r),&r,sizeof(r))) {status=JournalHealth::ReadFailed;ready=false;return false;}
            if(r.commit!=journalCommit)continue;
            if(!validJournalRecord(r)){status=JournalHealth::Corrupt;ready=false;return false;}
            if(r.sequence>after){out=r;cursorAfter=r.sequence;cursorBank=b;cursorSlot=i+1;cursorGeneration=firstSequence[b];return true;}
        }
        return false;
    }
    uint32_t availableSlots()const{return 2*journalSlots-nextSlot[0]-nextSlot[1];}
    // Caller must verify DATA partition, both OTA slots, nonoverlap, unencrypted
    // partition, and >=1 MiB before allowing ANY access. Runtime adapter enforces it.
    bool open(bool approvedLayout) {
        ready=false;status=JournalHealth::NotOpened;latestSequence=0;latestRecord={};latestCounts={};pending=0;correctionEpoch=0;
        bankReady[0]=bankReady[1]=false;nextSlot[0]=nextSlot[1]=0;cursorAfter=cursorSlot=cursorGeneration=0;
        firstSequence[0]=firstSequence[1]=lastSequence[0]=lastSequence[1]=0;
        if(!approvedLayout){status=JournalHealth::LayoutRejected;return false;}
        if(!recoverErases())return false;
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
                if(!validJournalRecord(r) || r.sequence<=lastSequence[b]) {
                    status=JournalHealth::Corrupt;return false;
                }
                if(!firstSequence[b])firstSequence[b]=r.sequence;
                lastSequence[b]=r.sequence;
                if(r.sequence>latestSequence){latestSequence=r.sequence;latestRecord=r;}
                if(validCountRecord(r.counts) && r.counts.sequence>latestCounts.sequence)latestCounts=r.counts;
                if(r.schema==2 && r.reserved>pending)pending=r.reserved;
                if(r.schema==2 && r.event.correctionEpoch>correctionEpoch)correctionEpoch=r.event.correctionEpoch;
                if(r.event.kind==2 && r.event.beforeDaily>pending)pending=r.event.beforeDaily;
            }
        }
        if(firstSequence[0] && firstSequence[1] &&
           !(lastSequence[0]<firstSequence[1] || lastSequence[1]<firstSequence[0])) {
            status=JournalHealth::Corrupt;return false;
        }
        ready=true;status=JournalHealth::Ready;return true;
    }
    // Walk durable records in their sequence order. The archive adapter MUST
    // use the sequence as an immutable identity and verify the entire record.
    // Replaying after reboot is then idempotent; this engine never edits totals.
    template<class Archive> bool replay(Archive& archive) {
        if(!ready)return false;
        unsigned oldest=firstSequence[1] && (!firstSequence[0] || firstSequence[1]<firstSequence[0])?1:0;
        for(unsigned step=0;step<2;++step) {
            unsigned b=(oldest+step)%2;
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
    // Reclamation is allowed only when the OTHER bank retains the newest
    // durable snapshot, and every committed record in this bank has a byte-exact
    // independently readable archive. A caller-supplied sequence alone is NOT proof.
    template<class Archive> bool reclaim(unsigned b,Archive& archive) {
        if(!ready || b>1 || !bankReady[b] || !lastSequence[b] ||
           lastSequence[b]>=latestSequence || lastSequence[1-b]!=latestSequence)return false;
        for(uint32_t i=0;i<nextSlot[b];++i) {
            JournalRecord r{};
            if(!store.read(b*journalBankBytes+journalHeaderBytes+i*sizeof(r),&r,sizeof(r)))return false;
            if(journalErased(&r,sizeof(r)) || r.commit!=journalCommit)continue;
            if(!validJournalRecord(r) || !archive.isVerified(r))return false;
        }
        // Ensure the retained bank also contains the newest COUNT snapshot,
        // not merely a newer undated event that lacks a baseline.
        bool keepsCounts=false;
        for(uint32_t i=0;i<nextSlot[1-b];++i) {
            JournalRecord r{};
            if(!store.read((1-b)*journalBankBytes+journalHeaderBytes+i*sizeof(r),&r,sizeof(r)))return false;
            if(validJournalRecord(r) && validCountRecord(r.counts) && r.counts.sequence==latestCounts.sequence)keepsCounts=true;
        }
        if(!keepsCounts)return false;
        if(!writeReceipt(b)){status=JournalHealth::WriteFailed;ready=false;return false;}
        if(!store.erase(b*journalBankBytes,journalBankBytes)) {
            status=JournalHealth::WriteFailed;ready=false;return false;
        }
        if(!readBankErased(b,0)){ready=false;return false;}
        bankReady[b]=false;nextSlot[b]=firstSequence[b]=lastSequence[b]=0;
        status=JournalHealth::Ready;return true;
    }
    JournalAppend append(const JournalEvent& event,const CountRecord& counts) {
        if(!ready)return JournalAppend::NotReady;
        if((!validCountRecord(counts) && !(event.kind==2 && memcmp(&counts,&zeroCountRecord(),sizeof(counts))==0)) || latestSequence==UINT32_MAX){status=JournalHealth::Corrupt;ready=false;return JournalAppend::Failed;}
        unsigned b=nextSlot[0]<journalSlots?0:1;
        if(nextSlot[b]>=journalSlots){status=JournalHealth::Full;return JournalAppend::Full;}
        if(!bankReady[b] && !initializeBank(b)){ready=false;return JournalAppend::Failed;}
        JournalRecord r{};r.magic=0x4d53424aU;r.schema=2;r.sequence=latestSequence+1;
        r.reserved=event.kind==2 && event.beforeDaily>pending?event.beforeDaily:pending;
        r.event=event;r.counts=counts;
        r.event.correctionEpoch=event.kind==6 && event.correctionEpoch>correctionEpoch?event.correctionEpoch:correctionEpoch;
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
        if(!firstSequence[b])firstSequence[b]=r.sequence;
        lastSequence[b]=r.sequence;
        latestSequence=r.sequence;latestRecord=check;correctionEpoch=r.event.correctionEpoch;
        if(validCountRecord(counts))latestCounts=counts;
        if(event.kind==2 && event.beforeDaily>pending)pending=event.beforeDaily;
        return JournalAppend::Durable;
    }
};
}
