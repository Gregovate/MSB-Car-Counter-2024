#include "traffic/Firmware.h"
// Include the real service in one test translation unit to simulate cold boots
// while preserving only the mock SD card between runs.
#include "../src/services/CountPersistence.cpp"
#include <cassert>
#include <iostream>

void reboot() {
    savedCounts={}; countDate=showResetDate=0;freshCountYear=0; countSlot=-1;
    countReady=false; countFault="NOT_RESTORED";
    totalDailyCars=totalShowCars=0;
    for(auto& n:hourlyCount)n=0;
    flagDailyShowStartReset=false;
}
std::string row(const char* date, int hour=16,int count=40) {
    std::string s=date;for(int i=0;i<24;++i)s+=","+std::to_string(i==hour ? count : 0);return s+"\n";
}
void legacy() {
    disk.clear();failWrite=failRead="";
    rtc.current=DateTime(2026,11,4,16,0,0);rtc.lost=false;sdAvailable=true;
    seasonFolder="/CC/2026";
    fileName1="/EnterTotal.txt";fileName2="/ShowTotal.txt";fileName3="/DayOfMonth.txt";fileName5="/HourlyData.csv";
    disk["/CC/2026/EnterTotal.txt"]="40";
    disk["/CC/2026/ShowTotal.txt"]="400";
    disk["/CC/2026/DayOfMonth.txt"]="4";
    disk["/CC/2026/HourlyData.csv"]="Date,Hours\n"+row("2026-11-04");
    disk["/CC/2025/ShowSummaryBuckets.csv"]="OFFICIAL UNCHANGED";
    reboot();
}
int main() {
    legacy();assert(restoreCountState());assert(totalDailyCars==40);
    // Per-event save survives immediate reboot, with no MQTT/timer cycle.
    ++totalDailyCars;++hourlyCount[16];assert(persistCountState());
    reboot();assert(restoreCountState());assert(totalDailyCars==41 && hourlyCount[16]==41 && totalShowCars==400);
    // Web text files may be stale; the checkpoint remains authoritative.
    disk["/CC/2026/EnterTotal.txt"]="0";
    reboot();assert(restoreCountState());assert(totalDailyCars==41);
    // Torn next save keeps the last completed copy; retry preserves RAM state.
    ++totalDailyCars;++hourlyCount[16];
    failWrite=countPath(countSlot==0 ? "CountState.b":"CountState.a");
    assert(!persistCountState());assert(totalDailyCars==42);
    failWrite="";assert(persistCountState());
    reboot();assert(restoreCountState());assert(totalDailyCars==42);
    // Tear a later save and reboot immediately: last completed state recovers.
    ++totalDailyCars;failWrite=countPath(countSlot==0 ? "CountState.b":"CountState.a");assert(!persistCountState());
    failWrite="";reboot();assert(restoreCountState());assert(totalDailyCars==42);
    // Reset once, count seven, reboot during reset minute: seven must survive.
    assert(resetCountsForShow());
    totalDailyCars=7;hourlyCount[16]+=7;assert(persistCountState());
    reboot();assert(restoreCountState());assert(totalDailyCars==7);assert(!resetCountsForShow());assert(totalDailyCars==7);
    // Missed midnight: new full date resets daily/hours but keeps show total.
    rtc.current=DateTime(2026,11,5);reboot();assert(restoreCountState());
    assert(totalDailyCars==0 && totalShowCars==400 && hourlyCount[16]==0);
    // Same day number next month is NOT the same date.
    totalDailyCars=9;hourlyCount[8]=9;assert(persistCountState());
    rtc.current=DateTime(2026,12,5);reboot();assert(restoreCountState());assert(totalDailyCars==0 && hourlyCount[8]==0);
    assert(disk["/CC/2025/ShowSummaryBuckets.csv"]=="OFFICIAL UNCHANGED");
    // Both damaged copies must never fall back to stale text or overwrite it.
    disk["/CC/2026/CountState.a"]="broken";disk["/CC/2026/CountState.b"]="broken";
    auto before=disk;reboot();assert(!restoreCountState());assert(!persistCountState());assert(disk==before);
    assert(!countTopicPublishable("msb/traffic/TestCounter/Cars/EnterTotal"));
    assert(countTopicPublishable("msb/traffic/TestCounter/System/countPersistence"));
    // Read errors/malformed values remain unknown, not valid zeros.
    legacy();failRead="/CC/2026/EnterTotal.txt";before=disk;assert(!restoreCountState());assert(disk==before);
    legacy();disk["/CC/2026/EnterTotal.txt"]="garbage";assert(!restoreCountState());
    legacy();disk["/CC/2026/EnterTotal.txt"]="2147483648";assert(!restoreCountState());
    legacy();disk["/CC/2026/HourlyData.csv"]="2026-11-04,1,2\n";assert(!restoreCountState());
    legacy();rtc.lost=true;before=disk;assert(!restoreCountState());assert(disk==before);
    // Gate uses absolute filenames. Same recovery engine and guarantees.
    legacy();seasonFolder="/GC/2026";
    fileName1="/GC/2026/DAILYTOT.txt";fileName2="/GC/2026/SHOWTOT.txt";
    fileName3="/GC/2026/DAYOFMONTH.txt";fileName5="/GC/2026/GateHourlyData.csv";
    disk[fileName1]="40";disk[fileName2]="400";disk[fileName3]="4";disk[fileName5]=row("2026-11-04");
    assert(restoreCountState());assert(totalDailyCars==40);assert(disk.count("/GC/2026/CountState.a"));
    // New-year cold boot initializes only a folder created during this boot.
    disk.clear();reboot();rtc.current=DateTime(2027,1,1);seasonFolder="/GC/2027";
    noteFreshCountFolder();assert(restoreCountState());assert(totalDailyCars==0 && totalShowCars==0);
    assert(disk.count("/GC/2027/CountState.a"));
    // Interrupted hourly replacement keeps all prior rows, including a power
    // cut between the original->previous and pending->original renames.
    legacy();String hourly="/CC/2026/HourlyData.csv";
    std::string old=disk[hourly];
    failWrite=hourly+".pending";
    assert(!writeHourlyExport(hourly,"new complete rows"));assert(disk[hourly]==old);
    failWrite="";failRename=hourly+".pending";
    assert(!writeHourlyExport(hourly,"new complete rows"));assert(disk[hourly]==old);
    failRename="";
    disk[hourly+".previous"]=old;disk.erase(hourly);
    assert(recoverHourlyExport(hourly));assert(disk[hourly]==old);
    assert(writeHourlyExport(hourly,"new complete rows"));
    assert(disk[hourly]=="new complete rows" && disk[hourly+".previous"]==old);
    std::cout<<"PASS: count recovery, torn saves, per-event persistence, reset replay, full dates, read failures, archive protection, Gate paths\n";
    return 0;
}
