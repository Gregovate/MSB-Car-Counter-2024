from pathlib import Path
import hashlib,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class JournalProvisioning(unittest.TestCase):
 def test_real_transaction_power_cuts_and_guards(self):
  fixture=bytes([0x42])*1048576;digest=hashlib.sha256(fixture).hexdigest().upper()
  source=(ROOT/'src/services/JournalProvisioning.cpp').read_text()
  # Only the pinned identity is substituted: production requires the operator's
  # actual SHA256, while fault injection uses an independently hashed fixture.
  source=source.replace('DE4C066D622782D66ECFDC2EA777677CCAC7565BB2272233DE853ED4E48584C1',digest)
  harness='#define main recovery_baseline_tests\n#include "'+str(ROOT/'tests/count_recovery_test.cpp')+'"\n#undef main\n'+source+r'''
void workerBoot(){provisionHandled=false;provisionPrefsOpen=false;provisionActive=false;nativeRestarts=0;nativeEraseCalls=0;nativeEraseBudget=-1;nativeSeedWriteBudget=-1;nativeInvalidateBaselineAfter=-1;}
void initial(){
 nativeJournalEnabled=false;nativeJournalHealthOverride="FOREIGN_DATA_PRESERVED";legacy();stringPrefs.clear();assert(restoreCountState());
 std::fill(nativeFlash.bytes.begin(),nativeFlash.bytes.end(),0x42);
 disk["/CC/2026/TrafficJournalInspection"]="";disk["/CC/2026/TrafficJournalInspection/flash-test.bin"]=std::string(1048576,0x42);
 fakeData={1,0x82,0x290000,0x160000,false};nativeRunningData=false;nativeBackupReadCount=0;nativeBackupReadMutateOn=-1;workerBoot();
}
int main(){
 initial();auto before=nativeFlash.bytes;fakeData.address=0x280000;serviceJournalProvisioning();assert(nativeEraseCalls==0 && nativeFlash.bytes==before);
 initial();nativeRunningData=true;serviceJournalProvisioning();assert(nativeEraseCalls==0);
 initial();disk["/CC/2026/TrafficJournalInspection/flash-test.bin"][8]^=1;serviceJournalProvisioning();assert(nativeEraseCalls==0);
 initial();nativeFlash.bytes[99]^=1;serviceJournalProvisioning();assert(nativeEraseCalls==0);
 initial();++totalDailyCars;serviceJournalProvisioning();assert(nativeEraseCalls==0); // live state must be independently verified on SD
 initial();nativeBackupReadMutateOn=257;serviceJournalProvisioning();assert(nativeEraseCalls==0); // corruption AFTER the first successful backup hash
 // Every sector boundary, including loss before the first erase and after the
 // last. Only the original image/partially erased image may resume from receipt.
 for(int cut=0;cut<=256;++cut){
  initial();nativeEraseBudget=cut;serviceJournalProvisioning();
  assert(nativeEraseCalls==unsigned(cut) && totalDailyCars==40 && totalShowCars==400);
  assert(disk.at("/CC/2026/TrafficJournalInspection/flash-test.bin")==std::string(1048576,0x42));
  if(cut<256){assert(nativeRestarts==0);workerBoot();serviceJournalProvisioning();assert(nativeRestarts==1);}
  traffic::JournalStore<NativeFlash> seeded(nativeFlash);assert(seeded.open(true) && seeded.snapshot() && seeded.snapshot()->daily==40);
 }
 // Every initial header/record-write interruption. A durable initial snapshot
 // exists before restart; a torn empty owned header can be completed, never erased.
 for(int cut=0;cut<=344;++cut){
  initial();nativeSeedWriteBudget=cut;serviceJournalProvisioning();
  workerBoot();nativeJournalEnabled=true;nativeJournalHealthOverride=nullptr;
  bool opened=nativeJournal.open(true);if(!opened)nativeJournalHealthOverride="FOREIGN_DATA_PRESERVED";
  reboot();assert(restoreCountState());serviceJournalProvisioning();
  traffic::JournalStore<NativeFlash> restored(nativeFlash);
  assert(restored.open(true) && restored.snapshot() && restored.snapshot()->daily==40);
  assert(totalDailyCars==40 && totalShowCars==400);
 }
 // A partly erased sector (zero-to-one changes) can safely resume too.
 initial();nativeEraseBudget=1;serviceJournalProvisioning();nativeFlash.bytes[8192]|=0x14;
 workerBoot();serviceJournalProvisioning();assert(nativeRestarts==1 && nativeEraseCalls==256);
 // A new programmed bit after interruption is NOT an interrupted erase. Even
 // a valid receipt cannot authorize deleting data that differs from the backup.
 initial();nativeEraseBudget=1;serviceJournalProvisioning();nativeFlash.bytes[17]=0;
 workerBoot();serviceJournalProvisioning();assert(nativeEraseCalls==0);
 // Valid count records survive even if their bank header is damaged/missing.
 initial();nativeEraseBudget=2;serviceJournalProvisioning();
 traffic::JournalRecord record{};record.magic=0x4d53424a;record.schema=2;record.sequence=1;record.event.kind=3;record.counts=savedCounts;
 record.checksum=traffic::journalHash(&record,offsetof(traffic::JournalRecord,checksum));record.commit=traffic::journalCommit;
 memcpy(nativeFlash.bytes.data()+4096,&record,sizeof(record));workerBoot();serviceJournalProvisioning();
 assert(nativeEraseCalls==0 && traffic::validJournalRecord(record));
 // Losing SD checkpoint currency during migration stops BEFORE another erase.
 initial();nativeInvalidateBaselineAfter=2;serviceJournalProvisioning();assert(nativeEraseCalls==2 && nativeRestarts==0);
 initial();nativeJournalHealthOverride="OK";serviceJournalProvisioning();assert(nativeEraseCalls==0 && nativeRestarts==0);
 // Execute normal boot recovery against the newly provisioned region, using
 // the actual count service + actual journal. Counts and old unassigned events
 // survive; a subsequent healthy boot must never erase again.
 initial();stringPrefs["vehicles"]="1";serviceJournalProvisioning();assert(nativeRestarts==1);
 nativeJournalEnabled=true;nativeJournalHealthOverride=nullptr;assert(nativeJournal.open(true));
 sdAvailable=false; // the original startup fault must not prevent baseline recovery
 reboot();assert(restoreCountState());assert(totalDailyCars==40 && totalShowCars==400 && pendingVehicles==1);
 assert(nativeJournal.snapshot() && nativeJournal.snapshot()->daily==40 && nativeJournal.pendingCount()==1);
 workerBoot();serviceJournalProvisioning();assert(nativeEraseCalls==0 && nativeRestarts==0);
}
'''
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);(p/'test.cpp').write_text(harness)
   subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(ROOT/'tests/provision_stubs'),'-I'+str(ROOT/'tests/count_stubs'),'-I'+str(ROOT/'include'),str(p/'test.cpp'),'-lcrypto','-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
