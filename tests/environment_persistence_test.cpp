#define TRAFFIC_OPERATING_TEST
#define main count_recovery_scenarios
#include "count_recovery_test.cpp"
#undef main
#include "../src/services/OperatingSettings.cpp"
#include "../src/services/EnvironmentService.cpp"
#include "../src/services/ShowSummary.cpp"
void coldEnvironment(){environmentSamples={};environmentLoaded=false;environmentPrefsReady=false;environmentClockBlocked=false;environmentSequence=0;environmentSlot=-1;environmentPersistence="NOT_LOADED";for(auto& v:hourlyTemp)v=NAN;}
int main(){
 legacy();stringPrefs.clear();rtc.current=DateTime(2026,11,27,17);assert(restoreCountState());
 settings=traffic::OperatingSettings{};settings.issuedDate=20261127;settings.endDate=20261231;settings.downDate=20261224;
 officialCountsComplete=true;
 tempF=40;humidity=50;averageHourlyTemp();assert(std::string(environmentPersistence)=="OK");
 rtc.current=DateTime(2026,11,27,17,1);tempF=60;averageHourlyTemp();
 float average=0;assert(showTemperatureAverage(average)&&average==50);
 coldEnvironment();assert(showTemperatureAverage(average)&&average==50 && environmentSamples.showCount==2);
 assert(environmentSamples.count[17]==2 && hourlyTemp[17]==50);
 rtc.current=DateTime(2026,11,27,22);assert(writeOfficialSummaries());
 assert(disk["/CC/2026/ShowSummary.csv"].find(",50.0,,")!=std::string::npos);
 // Lose both NVS slots: an already saved report average survives rewrite.
 stringPrefs.clear();coldEnvironment();assert(!showTemperatureAverage(average));
 assert(writeOfficialSummaries());assert(disk["/CC/2026/ShowSummary.csv"].find(",50.0,,")!=std::string::npos);
 // Recovery from a valid previous export fills a blank only for the same date.
 disk["/CC/2026/ShowSummary.csv.previous"]=disk["/CC/2026/ShowSummary.csv"];
 disk["/CC/2026/ShowSummary.csv"].replace(disk["/CC/2026/ShowSummary.csv"].find(",50.0,,"),7,",,,");
 disk["/CC/2026/ShowSummaryBuckets.csv"].replace(disk["/CC/2026/ShowSummaryBuckets.csv"].find(",50.0,,"),7,",,,");
 assert(writeOfficialSummaries());assert(disk["/CC/2026/ShowSummary.csv"].find(",50.0,,")!=std::string::npos);
 // Torn newest slot selects the older complete snapshot, never corrupt sums.
 stringPrefs.clear();coldEnvironment();rtc.current=DateTime(2026,11,28,17);
 tempF=0;averageHourlyTemp();tempF=20;averageHourlyTemp();
 stringPrefs["env:b"].resize(10);coldEnvironment();assert(showTemperatureAverage(average)&&average==0);
 // A new date cannot reuse yesterday's show average.
 rtc.current=DateTime(2026,11,29,17);coldEnvironment();assert(!showTemperatureAverage(average));
 // Future saved date/clock rollback preserves the checkpoint and blocks mixing.
 rtc.current=DateTime(2026,11,27,17);coldEnvironment();auto before=stringPrefs;
 assert(!showTemperatureAverage(average));tempF=50;averageHourlyTemp();assert(stringPrefs==before);
 assert(std::string(environmentPersistence)=="CLOCK_BEHIND_SAVED_SAMPLES");
 rtc.current=DateTime(2026,11,28,17);assert(showTemperatureAverage(average)&&average==0);
 // Gate enclosure data is backup-only and never writes Car's official namespace.
 THIS_MQTT_CLIENT="espGateCounter";rtc.current=DateTime(2026,11,29,17);coldEnvironment();
 tempF=30;averageHourlyTemp();assert(stringPrefs==before);
 assert(std::string(environmentPersistence)=="BACKUP_SENSOR_RAM_ONLY");
}
