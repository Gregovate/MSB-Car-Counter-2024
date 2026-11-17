// Executes the real shared service against host-only RTC/SD/NVS doubles.
#include "../src/services/RecordingCalendar.cpp"
#include <cassert>
#include <iostream>
int main(int argc,char** argv) {
    std::string mode=argc>1?argv[1]:"normal";
    if(mode=="black-friday") {
        for(int y=2026;y<=2099;++y)std::cout<<y<<" "<<traffic::blackFridayDay(y)<<"\n";
        return 0;
    }
    if(mode=="backward") {prefs["year"]=2027;rtc.current=DateTime(2025,12,31);}
    if(mode=="offline") {prefs["year"]=2026;rtc.lost=true;}
    if(mode=="sd-recovery") {
        // Persisted HA override with missing NVS; valid first copy, damaged second.
        disk["/recording-calendar.a"]="2026 11 28 "+std::to_string(0x4d534243UL^(2026UL<<16)^(11UL<<8)^28UL)+"\n";
        disk["/recording-calendar.b"]="torn";
    }
    initializeRecordingCalendar();
    if(mode=="backward") {
        assert(recordingYear()==2027);
        assert(!recordingPathWritable("/CC/2026/EnterLog.csv"));
        assert(recordingPathWritable("/CC/2027/EnterLog.csv"));return 0;
    }
    loadRecordingCalendarFromSD();
    assert(recordingYear()==2026);
    if(mode=="sd-recovery") {assert(recordingShowStart().day()==28);return 0;}
    if(mode=="offline") {
        assert(recordingDaysRunning()==0);assert(recordingPathWritable("/GC/2026/log.csv"));
        assert(rtcTimeLossObserved);rtc.lost=false;publishRecordingCalendar();
        bool warned=false;for(auto&m:messages)if(m.second=="TIME_RECOVERED_CHECK_RTC_BACKUP")warned=true;
        assert(warned);return 0;
    }
    assert(recordingShowStart().day()==27);
    assert(recordingDaysRunning()==0);
    assert(!acceptShowStartDate("2025-11-28"));
    assert(!acceptShowStartDate("2026-02-30"));
    assert(!acceptShowStartDate("2027-11-26"));
    assert(!acceptShowStartDate("2026-11-27x"));
    assert(acceptShowStartDate("2026-11-28"));
    assert(prefs["start"]==20261128);
    assert(disk["/recording-calendar.a"]==disk["/recording-calendar.b"]);
    assert(totalDailyCars==7 && totalShowCars==42);
    if(mode=="torn-write") {
        std::string previous=disk["/recording-calendar.b"];
        failWrite="/recording-calendar.a";
        acceptShowStartDate("2026-11-27");
        assert(disk["/recording-calendar.b"]==previous);
        publishRecordingCalendar();
        bool warning=false;for(auto &m:messages)if(m.second=="CALENDAR_SD_VERIFY_FAILED")warning=true;
        assert(warning);return 0;
    }
    rtc.current=DateTime(2026,11,28);assert(recordingDaysRunning()==1);
    rtc.current=DateTime(2026,12,23);int before=recordingDaysRunning();
    rtc.current=DateTime(2026,12,24);assert(recordingDaysRunning()==before);assert(!recordingShowSeasonActive());
    rtc.current=DateTime(2026,12,25);assert(recordingDaysRunning()==before+1);
    // A web write at Jan 1 must be blocked even before loop path rotation.
    rtc.current=DateTime(2027,1,1);
    assert(!recordingPathWritable("/CC/2026/EnterLog.csv"));
    ticks+=1000;updateRecordingCalendar();
    assert(recordingYear()==2027 && seasonFolder=="/CC/2027");
    assert(recordingShowStart().day()==26 && recordingDaysRunning()==0);
    assert(prefs["year"]==2027);
    assert(totalDailyCars==7 && totalShowCars==42);
    rtc.current=DateTime(2026,12,31);ticks+=1000;updateRecordingCalendar();
    assert(recordingYear()==2027);
    rtc.current=DateTime(2030,1,1);ticks+=1000;updateRecordingCalendar();
    assert(recordingYear()==2027); // Reject unconfirmed multi-year jump.
    noteNetworkClock(2030);ticks+=1000;updateRecordingCalendar();assert(recordingYear()==2030);
    for(const char* path:{"/CC/2025/x","/gc/2025/x","/GC","/CC/2025","/CC/2025/","/CC/2030/../2025/x","/data/../CC/2025/x","//CC/2025/x","/CC/2030/./x","/CC/2030./x","/CC/2030 /x","/CC/2030/%2e%2e/x","/CC/","/CC/2"}) {
        assert(!recordingOpen(path,FILE_WRITE));
        assert(!recordingRemove(path));
        assert(!recordingRename("/data/test",path));
    }
    assert(recordingPathWritable("/CC/2030/log.csv"));
    assert(recordingPathWritable("/GC/2030/log.csv"));
    assert(recordingPathWritable("/data/index.html"));
    assert(!recordingWebWritable("/recording-calendar.a"));
    disk["/CC/2025/ShowSummaryBuckets.csv"]="official metrics";
    assert(recordingOpen("/CC/2025/ShowSummaryBuckets.csv",FILE_READ));
    assert(!recordingOpen("/CC/2025/ShowSummaryBuckets.csv",FILE_WRITE));
    assert(!recordingRename("/CC/2025/ShowSummaryBuckets.csv","/CC/2030/moved.csv"));
    assert(disk["/CC/2025/ShowSummaryBuckets.csv"]=="official metrics");
}
