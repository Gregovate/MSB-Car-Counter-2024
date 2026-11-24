#include "traffic/SdJournalArchive.h"
#include <map>
#include <string>
#include <vector>
#include <cassert>
#include <iostream>
using namespace traffic;
struct Filesystem {
 std::map<std::string,std::vector<uint8_t>> files;bool failRead=false,failRename=false;int writeBytes=320;unsigned writes=0;
 struct File {
  Filesystem* fs;std::string path;bool ok;
  operator bool()const{return ok;}
  size_t size()const{return fs->files[path].size();}
  size_t read(uint8_t* p,size_t n){if(fs->failRead)return 0;n=std::min(n,size());memcpy(p,fs->files[path].data(),n);return n;}
  size_t write(const uint8_t* p,size_t n){n=std::min(n,size_t(fs->writeBytes));fs->files[path].assign(p,p+n);++fs->writes;return n;}
  void close(){}void flush(){}
 };
 File open(const char* p,const char* mode){bool present=files.count(p);if(!strcmp(mode,"w")){files[p]={};present=true;}return {this,p,present};}
 bool exists(const char* p){return files.count(p);}
 bool mkdir(const char* p){files[p]={};return true;}
 bool rename(const char* p,const char* q){if(failRename || files.count(q) || !files.count(p))return false;files[q]=files[p];files.erase(p);return true;}
};
JournalRecord record(){JournalRecord r{};r.magic=0x4d53424a;r.schema=1;r.sequence=9;r.event.bootId=42;r.counts.magic=0x4d534232;r.counts.sequence=11;r.counts.date=20261123;r.counts.daily=19;r.counts.checksum=countChecksum(r.counts);r.checksum=journalHash(&r,offsetof(JournalRecord,checksum));r.commit=journalCommit;return r;}
int main(){
 auto r=record();Filesystem fs;SdJournalArchive<Filesystem> archive(fs,2026,"CC");
 for(int cut=0;cut<320;++cut){fs.writeBytes=cut;assert(!archive.storeVerified(r));assert(!fs.exists("/CC/2026/TrafficJournal/0000000009.bin"));}
 fs.writeBytes=320;fs.failRead=true;assert(!archive.storeVerified(r));fs.failRead=false;
 fs.failRename=true;assert(!archive.storeVerified(r));fs.failRename=false;
 assert(archive.storeVerified(r));auto n=fs.writes;
 assert(archive.storeVerified(r) && fs.writes==n); // repeat must never duplicate
 fs.files["/CC/2026/TrafficJournal/0000000009.bin"][64]^=1;
 auto before=fs.files;assert(!archive.storeVerified(r));assert(fs.files==before);
 SdJournalArchive<Filesystem> wrongYear(fs,2027,"CC");assert(!wrongYear.storeVerified(r));
 SdJournalArchive<Filesystem> wrongRoot(fs,2026,"../data");assert(!wrongRoot.storeVerified(r));
 std::cout<<"PASS: 320 short-write positions, readback/rename failure, retry, duplicate prevention, conflict preservation and recording-year guard\n";
}
