#define TRAFFIC_OPERATING_TEST
#define main count_recovery_scenarios
#include "count_recovery_test.cpp"
#undef main
#include "../src/services/OperatingSettings.cpp"
#include "../src/services/ShowSummary.cpp"

void configure(bool gate,int extension=0) {
    THIS_MQTT_CLIENT=gate ? "espGateCounter":"espCarCounter";
    initializeOperatingSettings();
    settings=traffic::OperatingSettings{};
    settings.resetOffset=gate ? 10:-1;settings.grace=gate ? 25:10;
    settings.issuedDate=20261127;settings.endDate=20261231;settings.downDate=20261224;
    settings.extension=extension;settingsHealth="OK";
}
void vehicle() {
    serviceCountState();++totalDailyCars;++hourlyCount[rtc.now().hour()];
    recordOfficialVehicle();assert(persistCountState());
}
int main() {
    traffic::OperatingSettings parsed;
    assert(traffic::parseOperatingSettings("1,1020,1260,10,25,30,20261127,60000,100,20261231,20261224,2026",parsed,2026));
    for(const char* invalid:{"", "1,1020,1260,10,25,30,20261127,60000,100,20261231,20261224,2025",
        "1,1020,1260,10,25,31,20261127,60000,100,20261231,20261224,2026",
        "1,1020,1260,10,25,0,20261127,60000x,100,20261231,20261224,2026",
        "1,1020,1260,10,25,0,20261127,60000,100,20261231,20261224,2026,7"})
        assert(!traffic::parseOperatingSettings(invalid,parsed,2026));
    // Reproduce a next-day HA transaction arriving before RTC local midnight.
    legacy();rtc.current=DateTime(2026,11,16,23,59,58);configure(false);
    settings.issuedDate=20261116;
    const char* settingsTopic="msb/traffic/TestCounter/Config/operatingSettings";
    const char* nextDay="1,1020,1260,-1,10,0,20261117,60000,500,20261231,20261224,2026";
    auto beforeDisk=disk;int beforeDaily=totalDailyCars,beforeShow=totalShowCars;
    messages.clear();messageRetained.clear();
    handleOperatingCommand(settingsTopic,nextDay);
    assert(settingsHealth=="REJECTED_INVALID_SETTINGS");
    assert(lastSettingsRejection.find("FUTURE_ISSUED_DATE")!=std::string::npos);
    assert(lastSettingsRejection.find("\"receivedIssuedDate\":20261117")!=std::string::npos);
    assert(lastSettingsRejection.find("\"acceptedIssuedDate\":20261116")!=std::string::npos);
    assert(lastSettingsRejection.find("2026-11-16 23:59:58")!=std::string::npos);
    assert(disk==beforeDisk && totalDailyCars==beforeDaily && totalShowCars==beforeShow);
    bool retainedEvidence=false;
    for(size_t i=0;i<messages.size();++i)
        if(messages[i].first=="msb/traffic/TestCounter/System/settingsLastRejection")
            retainedEvidence=messageRetained[i] && messages[i].second==lastSettingsRejection;
    assert(retainedEvidence);
    String evidence=lastSettingsRejection;
    rtc.current=DateTime(2026,11,17,0,5);handleOperatingCommand(settingsTopic,nextDay);
    assert(settingsHealth=="OK" && settings.issuedDate==20261117);
    assert(lastSettingsRejection==evidence); // Valid retries cannot erase the evidence.
    assert(totalDailyCars==beforeDaily && totalShowCars==beforeShow);
    beforeDisk=disk;
    handleOperatingCommand(settingsTopic,"1,1020,1260,-1,10,0,20261116,60000,500,20261231,20261224,2026");
    assert(lastSettingsRejection.find("STALE_ISSUED_DATE")!=std::string::npos);
    assert(settings.issuedDate==20261117 && disk==beforeDisk);
    handleOperatingCommand(settingsTopic,"1,1020,1260,-1,10,0,20261117,60000,500,20261231,20261224,2025");
    assert(lastSettingsRejection.find("RECORDING_YEAR_MISMATCH")!=std::string::npos);
    assert(settings.issuedDate==20261117 && disk==beforeDisk);
    traffic::OperatingParseDiagnostic detail;
    assert(!traffic::parseOperatingSettings("1,x",parsed,2026,&detail));
    assert(std::string(detail.reason)=="EXPECTED_INTEGER" && detail.field==1);
    assert(!traffic::parseOperatingSettings(nullptr,parsed,2026,&detail));
    assert(std::string(detail.reason)=="MISSING_PAYLOAD");
    assert(!traffic::parseOperatingSettings("1,1020,1260,-1,10,0,20261117,60000,3001,20261231,20261224,2026",parsed,2026,&detail));
    assert(std::string(detail.reason)=="INVALID_DETECT_TIME" && detail.field==8);
    assert(!traffic::parseOperatingSettings("1,1020,1260,-1,10,0,20260230,60000,500,20261231,20261224,2026",parsed,2026,&detail));
    assert(std::string(detail.reason)=="INVALID_DATE" && detail.field==6);
    for(bool gate:{false,true}) for(int extension:{0,15,30}) {
        legacy();rtc.current=DateTime(2026,11,27,4);noteFreshCountFolder();assert(restoreCountState());
        configure(gate,extension);
        assert(settings.start+settings.resetOffset==(gate ? 1030:1019));
        assert(effectiveShowEnd()==(gate ? 1285:1270)+extension);
        rtc.current=DateTime(2026,11,27,16,50);vehicle(); // volunteer enters; raw only
        assert(totalShowCars==0);
        rtc.current=DateTime(2026,11,27,17,gate ? 9:0);vehicle();
        assert(totalShowCars==(gate ? 0:1));
        rtc.current=DateTime(2026,11,27,17,10);vehicle();
        rtc.current=DateTime(2026,11,27,21,0);vehicle();
        int stop=effectiveShowEnd();rtc.current=DateTime(2026,11,27,stop/60,stop%60);vehicle();
        unsigned show=totalShowCars; // exact cutoff excluded
        rtc.current=DateTime(2026,11,27,23,0);vehicle();assert(totalShowCars==int(show));
        assert(hourlyCount[23]==1); // volunteers remain in raw traffic
        assert(writeOfficialSummaries());
        auto csv=disk["/CC/2026/ShowSummaryBuckets.csv"];
        assert(csv.find("10PM,ExtensionMin")!=std::string::npos);
        // No 21:00 vehicle is misreported in the 20:00-20:59 bucket.
        assert(csv.find(gate ? "2026-11-27,0,1,0,0,0," : "2026-11-27,0,2,0,0,0,")!=std::string::npos);
        assert(csv.find(extension ? ",1,"+std::to_string(extension)+"\n" : ",,\n")!=std::string::npos);
        assert(writeOfficialSummaries());assert(disk["/CC/2026/ShowSummaryBuckets.csv"]==csv);
        reboot();assert(restoreCountState());assert(totalShowCars==int(show));
        assert(officialHourly[21]==1);assert(writeOfficialSummaries());
        assert(disk["/CC/2026/ShowSummaryBuckets.csv"]==csv);
        assert(disk["/CC/2025/ShowSummaryBuckets.csv"]=="OFFICIAL UNCHANGED");
    }
    legacy();rtc.current=DateTime(2026,11,27,4);noteFreshCountFolder();assert(restoreCountState());configure(true);
    rtc.current=DateTime(2026,11,27,17,10);ticks+=10001;serviceShowSchedule();
    for(int i=0;i<7;++i) vehicle();
    reboot();assert(restoreCountState());
    ticks+=10001;serviceShowSchedule();assert(totalDailyCars==7 && officialHourly[17]==7);
    // HA changes reset offset to +12: saved settings are restored on offline boot.
    rtc.current=DateTime(2026,11,27,18);
    const char* command="msb/traffic/TestCounter/Config/operatingSettings";
    assert(handleOperatingCommand(command,"1,1020,1260,12,25,15,20261127,45000,200,20261231,20261224,2026"));
    assert(settings.resetOffset==12 && totalDailyCars==7);
    initializeOperatingSettings();assert(settings.resetOffset==12 && settings.extension==15);
    failWrite="/operating-settings.b";
    handleOperatingCommand(command,"1,1020,1260,14,25,30,20261127,45000,200,20261231,20261224,2026");
    assert(settings.resetOffset==12);failWrite="";
    rtc.current=DateTime(2026,11,28,18);assert(effectiveExtension()==0);
    // Legacy summary rows are preserved exactly, with appended blank cells.
    rtc.current=DateTime(2026,11,27,22);configure(false);officialCountsComplete=true;
    const std::string oldHeader="Date,DaysRunning,6PM,7PM,8PM,9PM,DailyTotal,ShowTotal,DailyAvgTemp";
    disk["/CC/2026/ShowSummaryBuckets.csv"]=oldHeader+"\n2026-11-26,0,1,2,3,4,10,10,35.0\n";
    assert(writeOfficialSummaries());assert(disk["/CC/2026/ShowSummaryBuckets.csv"].find("2026-11-26,0,1,2,3,4,10,10,35.0,,\n")!=std::string::npos);
    auto previous=disk["/CC/2026/ShowSummaryBuckets.csv"];
    failWrite="/CC/2026/ShowSummaryBuckets.csv.pending";assert(!writeOfficialSummaries());
    assert(disk["/CC/2026/ShowSummaryBuckets.csv"]==previous);failWrite="";
    // Fresh manual correction is applied once, never replayed after another vehicle.
    handleOperatingCommand("msb/traffic/TestCounter/Config/countCorrection","20261127220000,daily,20");
    assert(totalDailyCars==20);vehicle();
    handleOperatingCommand("msb/traffic/TestCounter/Config/countCorrection","20261127220000,daily,20");
    assert(totalDailyCars==21);
    std::cout<<"PASS: actual operating service, HA offsets, SD fallback, seven-car reboot, normal/15/30 extensions, volunteers, summary retries, replay protection\n";
}
