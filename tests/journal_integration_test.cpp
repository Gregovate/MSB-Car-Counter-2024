#define main count_service_baseline_scenarios
#include "count_recovery_test.cpp"
#undef main
int main() {
    legacy();stringPrefs.clear();nativeJournalEnabled=true;
    assert(nativeJournal.open(true));assert(restoreCountState());
    assert(nativeJournal.snapshot() && nativeJournal.snapshot()->daily==40);
    // SD disappears: every vehicle's full dated state stays durable in flash.
    sdAvailable=false;
    for(unsigned i=0;i<1300;++i){++totalDailyCars;++totalShowCars;++hourlyCount[16];persistCountState();}
    assert(nativeJournal.snapshot()->daily==1340);
    reboot();assert(nativeJournal.open(true));assert(restoreCountState());
    assert(totalDailyCars==1340 && totalShowCars==1700 && hourlyCount[16]==1340);
    // Show reset and vehicles after it must not resurrect pre-reset totals.
    assert(resetCountsForShow());
    ++totalDailyCars;++hourlyCount[16];persistCountState();
    reboot();assert(nativeJournal.open(true));assert(restoreCountState());
    assert(totalDailyCars==1 && showResetDate==20261104 && !resetCountsForShow());
    // Invalid clock produces undated records, never changes the dated baseline.
    rtc.lost=true;recordPendingVehicle();recordPendingVehicle();
    uint32_t previous=nativeJournal.snapshot()->daily;
    reboot();assert(nativeJournal.open(true));assert(!restoreCountState());assert(pendingVehicles==2);
    assert(nativeJournal.snapshot()->daily==previous);
    // SD comes back stale: flash wins; copyback must really update SD.
    rtc.lost=false;sdAvailable=true;
    assert(restoreCountState());assert(totalDailyCars==1 && pendingVehicles==2);
    traffic::CountRecord a{},b{};bool va=readCountRecord(0,a),vb=readCountRecord(1,b);assert(va || vb);
    assert((traffic::validCountRecord(a) && a.daily==1) || (traffic::validCountRecord(b) && b.daily==1));
    // Correction stays applied even if NVS acknowledgement is lost; replay
    // identity is in the same record as the resulting count.
    assert(correctCountState(true,20,rtc.now().unixtime()));
    uint32_t nonce=journalCorrectionNonce();++totalDailyCars;++hourlyCount[16];persistCountState();
    reboot();assert(nativeJournal.open(true));assert(restoreCountState());
    assert(totalDailyCars==21 && journalCorrectionNonce()==nonce);
    assert(!correctCountState(true,20,nonce) && totalDailyCars==21);
    // Midnight while SD is absent preserves show total and journals a new day.
    sdAvailable=false;rtc.current=DateTime(2026,11,5,0,0,1);ticks+=5001;serviceCountState();
    assert(totalDailyCars==0 && totalShowCars==1700 && hourlyCount[16]==0);
    reboot();assert(nativeJournal.open(true));assert(restoreCountState());
    assert(totalDailyCars==0 && totalShowCars==1700 && countDate==20261105);
    sdAvailable=true;persistCountState();
    // Same lineage sequence with different contents is a conflict, not a guess.
    traffic::CountRecord conflict=*nativeJournal.snapshot();++conflict.daily;conflict.checksum=traffic::countChecksum(conflict);
    disk["/CC/2026/CountState.a"]=std::string(reinterpret_cast<char*>(&conflict),sizeof(conflict));
    disk.erase("/CC/2026/CountState.b");reboot();assert(nativeJournal.open(true));
    assert(!restoreCountState());assert(std::string(countFault)=="RESTORE_CONFLICT_KEEP_RECORDS");
    std::cout<<"PASS: real count service with 1300 offline events, flash-only reboot, reset replay, undated preservation, stale-SD copyback and equal-sequence conflict\n";
}
