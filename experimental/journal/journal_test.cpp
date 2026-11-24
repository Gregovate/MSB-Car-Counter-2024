#include "JournalStore.h"
#include "PartitionGuard.h"
#include <vector>
#include <cassert>
#include <iostream>
#include <algorithm>
#include <map>
using namespace traffic;
struct Flash {
    std::vector<uint8_t> bytes=std::vector<uint8_t>(2*journalBankBytes,0xff);
    int budget=-1;bool failRead=false;unsigned writes=0;
    bool read(uint32_t off,void* dst,size_t n){if(failRead || uint64_t(off)+n>bytes.size())return false;memcpy(dst,bytes.data()+off,n);return true;}
    bool erase(uint32_t off,size_t n){if(uint64_t(off)+n>bytes.size())return false;std::fill(bytes.begin()+off,bytes.begin()+off+n,0xff);return true;}
    bool write(uint32_t off,const void* src,size_t n){
        ++writes;if(uint64_t(off)+n>bytes.size())return false;
        const auto* p=static_cast<const uint8_t*>(src);
        for(size_t i=0;i<n;++i){if(budget==0)return false;if(budget>0)--budget;assert((bytes[off+i]&p[i])==p[i]);bytes[off+i]&=p[i];}
        return true;
    }
};
struct Archive {
 std::map<uint32_t,JournalRecord> files;int budget=-1;unsigned writes=0;
 bool isVerified(const JournalRecord& r){auto it=files.find(r.sequence);return it!=files.end() && memcmp(&it->second,&r,sizeof(r))==0;}
 bool storeVerified(const JournalRecord& r){
  auto it=files.find(r.sequence);
  if(it!=files.end())return memcmp(&it->second,&r,sizeof(r))==0;
  if(budget==0)return false;
  if(budget>0)--budget;
  files[r.sequence]=r;++writes;return memcmp(&files[r.sequence],&r,sizeof(r))==0;
 }
};
CountRecord counts(unsigned n){CountRecord r{};r.magic=0x4d534232;r.sequence=n+1;r.date=20261123;r.daily=n;r.show=n;r.hourly[17]=n;r.official[17]=n;r.officialComplete=1;r.checksum=countChecksum(r);return r;}
int main(){
 JournalPartition data{1,0x82,0x290000,0x160000,false},a{0,0x10,0x10000,0x140000,false},b{0,0x11,0x150000,0x140000,false};
 assert(approveJournalPartition(data,a,b));auto bad=data;bad.type=0;assert(!approveJournalPartition(bad,a,b));bad=data;bad.address=a.address;assert(!approveJournalPartition(bad,a,b));bad=data;bad.size=128*1024;assert(!approveJournalPartition(bad,a,b));bad=data;bad.encrypted=true;assert(!approveJournalPartition(bad,a,b));b.subtype=0;assert(!approveJournalPartition(data,a,b));
 Flash seed;JournalStore<Flash> first(seed);assert(first.open(true));JournalEvent e{};e.bootId=42;e.localEpoch=1795453200;e.kind=1;e.passageMs=582000;
 assert(first.append(e,counts(1))==JournalAppend::Durable);
 // Every interruption byte of a complete record payload plus commit word.
 for(int cut=0;cut<=312;++cut){
  Flash f=seed;f.budget=cut;JournalStore<Flash> j(f);assert(j.open(true));auto result=j.append(e,counts(2));
  f.budget=-1;JournalStore<Flash> boot(f);assert(boot.open(true));
  if(result==JournalAppend::Durable){assert(boot.sequence()==2 && boot.latest()->counts.daily==2);}else assert(boot.sequence()==1);
  assert(boot.append(e,counts(3))==JournalAppend::Durable);JournalStore<Flash> again(f);assert(again.open(true));assert(again.latest()->counts.daily==3);
 }
 Flash f;JournalStore<Flash> j(f);assert(j.open(true));
 for(unsigned i=1;i<=1300;++i){e.endUptimeMs=i*2000;e.betweenMs=2000;assert(j.append(e,counts(i))==JournalAppend::Durable);}
 JournalStore<Flash> reboot(f);assert(reboot.open(true));assert(reboot.sequence()==1300 && reboot.latest()->counts.daily==1300 && reboot.latest()->event.betweenMs==2000);
 Archive archive;archive.budget=600;assert(!reboot.replay(archive));assert(archive.files.size()==600);
 archive.budget=-1;assert(reboot.replay(archive));assert(archive.files.size()==1300 && archive.writes==1300);
 assert(reboot.replay(archive));assert(archive.writes==1300); // no duplicate on repeated reconnect
 archive.files[700].event.passageMs^=1;assert(!reboot.replay(archive)); // never overwrite a conflicting identity
 for(unsigned i=1301;i<=2*journalSlots;++i)assert(reboot.append(e,counts(i))==JournalAppend::Durable);
 auto before=f.bytes;assert(reboot.append(e,counts(9999))==JournalAppend::Full);assert(before==f.bytes);
 // A full bank cannot be erased until each record is independently verified.
 Archive complete;assert(reboot.replay(complete));
 assert(!reboot.reclaim(1,complete)); // never erase the newest snapshot
 complete.files[500].event.flags^=1;before=f.bytes;
 assert(!reboot.reclaim(0,complete) && before==f.bytes);
 complete.files[500].event.flags^=1;assert(reboot.reclaim(0,complete));
 assert(reboot.append(e,counts(3251))==JournalAppend::Durable);
 JournalStore<Flash> rotated(f);assert(rotated.open(true));assert(rotated.sequence()==3251);
 assert(rotated.latest()->counts.daily==3251);
 Archive rebuilt;assert(rotated.replay(rebuilt));assert(rebuilt.files.size()==journalSlots+1);
 // Unknown filesystem bytes, even deep in a blank-looking bank, are protected.
 Flash foreign;foreign.bytes[700000]=0;JournalStore<Flash> protect(foreign);before=foreign.bytes;assert(!protect.open(true));assert(before==foreign.bytes && foreign.writes==0);
 Flash layout;JournalStore<Flash> reject(layout);assert(!reject.open(false) && layout.writes==0);
 Flash corrupt=seed;corrupt.bytes[journalHeaderBytes+64]^=1;JournalStore<Flash> c(corrupt);assert(!c.open(true) && c.health()==JournalHealth::Corrupt);
 Flash unreadable=seed;unreadable.failRead=true;JournalStore<Flash> r(unreadable);assert(!r.open(true) && r.health()==JournalHealth::ReadFailed);
 std::cout<<"PASS: 1300 durable records; 313 power-cut positions; cold reboot; torn-slot recovery; both-bank capacity "<<2*journalSlots<<"; no overwrite; unknown-data protection; corruption/read failure; OTA layout guards\n";
}
