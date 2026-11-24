#define main count_service_scenarios
#include "count_recovery_test.cpp"
#undef main
int main(){
 legacy();nativeJournalEnabled=true;assert(nativeJournal.open(true));assert(restoreCountState());
 assert(persistCountState());assert(std::string(countFault)=="OK");
 const std::string path="/CC/2026/ClosedCounts-20261103.bin";
 auto old=savedCounts;old.date=20261103;old.showResetDate=0;old.sequence=1;old.checksum=traffic::countChecksum(old);
 // Ensure current checkpoint sequence is strictly newer than the old snapshot.
 ++totalDailyCars;assert(persistCountState());
 disk[path]=std::string(reinterpret_cast<char*>(&old),sizeof(old));
 auto before=disk;
 assert(!removeObsoleteClosedCount("/CC/2026/CountState.a"));
 assert(!removeObsoleteClosedCount("/CC/2026/TrafficJournal/0000000001.bin"));
 assert(!removeObsoleteClosedCount("/CC/2025/ClosedCounts-20261103.bin"));
 assert(!removeObsoleteClosedCount("/CC/2026/ClosedCounts-20261104.bin"));
 assert(!removeObsoleteClosedCount("/CC/2026/ClosedCounts-20261103.bin/../CountState.a"));
 assert(disk==before);
 closedCounts=old;assert(!removeObsoleteClosedCount(path));closedCounts={};
 sdAvailable=false;assert(!removeObsoleteClosedCount(path));sdAvailable=true;
 rtc.lost=true;assert(!removeObsoleteClosedCount(path));rtc.lost=false;
 failRead=path;assert(!removeObsoleteClosedCount(path));failRead="";
 disk[path][50]^=1;assert(!removeObsoleteClosedCount(path));disk=before;
 disk["/CC/2026/CountState.a"]="bad";disk["/CC/2026/CountState.b"]="bad";
 assert(!removeObsoleteClosedCount(path));disk=before;
 assert(removeObsoleteClosedCount(path));assert(!disk.count(path));
 assert(totalDailyCars==41 && totalShowCars==400);
 reboot();assert(nativeJournal.open(true));assert(restoreCountState());assert(totalDailyCars==41);
}
