from pathlib import Path
import re,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class JournalCleanup(unittest.TestCase):
 def test_actual_archive_removal_requires_retired_verified_dated_record(self):
  source=(ROOT/'src/services/TrafficJournal.cpp').read_text()
  method=re.search(r'bool removeRetiredJournalArchive\(.*?^}',source,re.S|re.M).group(0)
  harness=r'''
#define main count_service_scenarios
#include "COUNT_TEST"
#undef main
auto& journal=nativeJournal;
bool opened=true,durable=true;const char* health="OK";const char* archiveHealth="OK";
METHOD
int main(){
 legacy();nativeJournalEnabled=true;assert(nativeJournal.open(true));assert(restoreCountState());
 auto old=*journal.latest();
 const String path="/CC/2026/TrafficJournal/0000000001.bin";
 auto bytes=[](const traffic::JournalRecord& r){return std::string(reinterpret_cast<const char*>(&r),sizeof(r));};
 disk[path]=bytes(old);
 rtc.current=DateTime(2026,11,5,12);
 auto c=*journal.snapshot();c.date=20261105;c.showResetDate=0;
 for(unsigned i=1;i<=traffic::journalSlots;++i){++c.sequence;c.checksum=traffic::countChecksum(c);traffic::JournalEvent e{};e.kind=3;assert(journal.append(e,c)==traffic::JournalAppend::Durable);}
 assert(!removeRetiredJournalArchive(path)); // Still required for reclaim verification.
 struct Verified{bool isVerified(const traffic::JournalRecord&){return true;}} verified;
 assert(journal.reclaim(0,verified));assert(journal.oldestRetainedSequence()==traffic::journalSlots+1);
 auto flashBefore=nativeFlash.bytes;
 sdAvailable=false;assert(!removeRetiredJournalArchive(path));sdAvailable=true;
 rtc.lost=true;assert(!removeRetiredJournalArchive(path));rtc.lost=false;
 archiveHealth="SD_WRITE_VERIFY_FAILED_RETRY";assert(!removeRetiredJournalArchive(path));archiveHealth="OK";
 failRead=path;assert(!removeRetiredJournalArchive(path));failRead="";
 disk[path][60]^=1;assert(!removeRetiredJournalArchive(path));disk[path]=bytes(old);
 auto changed=old;changed.event.kind=2;changed.checksum=traffic::journalHash(&changed,offsetof(traffic::JournalRecord,checksum));disk[path]=bytes(changed);
 assert(!removeRetiredJournalArchive(path));
 changed=old;changed.counts.date=20261105;changed.counts.checksum=traffic::countChecksum(changed.counts);changed.checksum=traffic::journalHash(&changed,offsetof(traffic::JournalRecord,checksum));disk[path]=bytes(changed);
 assert(!removeRetiredJournalArchive(path));disk[path]=bytes(old);
 assert(!removeRetiredJournalArchive("/CC/2026/TrafficJournal/../0000000001.bin"));
 assert(!removeRetiredJournalArchive("/CC/2025/TrafficJournal/0000000001.bin"));
 assert(removeRetiredJournalArchive(path));assert(!disk.count(path));assert(nativeFlash.bytes==flashBefore);
 assert(journal.snapshot()->date==20261105 && totalDailyCars==40 && totalShowCars==400);
}
'''.replace('COUNT_TEST',str(ROOT/'tests/count_recovery_test.cpp')).replace('METHOD',method)
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);(p/'test.cpp').write_text(harness)
   subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(ROOT/'tests/count_stubs'),'-I'+str(ROOT/'include'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
