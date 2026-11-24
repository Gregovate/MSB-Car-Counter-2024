#define TRAFFIC_OPERATING_TEST
#define main count_recovery_scenarios
#include "count_recovery_test.cpp"
#undef main
#include "../src/services/OperatingSettings.cpp"
#include "../src/services/EnvironmentService.cpp"
#include "../src/services/ShowSummary.cpp"
void cold(){environmentSamples={};for(auto& v:rollingEnvironment)v={};environmentLoaded=false;environmentPrefsReady=false;environmentClockBlocked=false;environmentSequence=0;environmentSlot=-1;}
bool received(const char* topic,const char* value){for(size_t i=0;i<messages.size();++i)if(messages[i].first==topic && messages[i].second==value && messageRetained[i])return true;return false;}
int main(){
 legacy();stringPrefs.clear();settings=traffic::OperatingSettings{};settings.issuedDate=20261127;settings.endDate=20261231;settings.downDate=20261224;
 rtc.current=DateTime(2026,11,27,23,59);tempF=26;humidity=60;averageHourlyTemp();
 // Reboot across midnight without an hourly close: seed from the .28 sample checkpoint.
 cold();rtc.current=DateTime(2026,11,28,0,1);messages.clear();messageRetained.clear();publishEnvironmentHourlyState();
 assert(received("msb/traffic/TestCounter/Env/temp/hourly/23","26.0"));
 assert(received("msb/traffic/TestCounter/Env/temp/hourlyDate/23","20261127"));
 assert(!environmentSamples.showCount); // Daily official averages remain date-specific.
 // Complete a new 00 bucket while keeping yesterday's 23 bucket.
 tempF=24;averageHourlyTemp();rtc.current=DateTime(2026,11,28,1,0);serviceEnvironmentHour();messages.clear();messageRetained.clear();publishEnvironmentHourlyState();
 assert(received("msb/traffic/TestCounter/Env/temp/hourly/00","24.0"));
 assert(received("msb/traffic/TestCounter/Env/temp/hourlyDate/00","20261128"));
 assert(received("msb/traffic/TestCounter/Env/temp/hourly/23","26.0"));
 // Fresh process can replay both dated rolling buckets, not just today's samples.
 cold();messages.clear();messageRetained.clear();publishEnvironmentHourlyState();
 assert(received("msb/traffic/TestCounter/Env/temp/hourly/23","26.0"));
 assert(received("msb/traffic/TestCounter/Env/temp/hourly/00","24.0"));
 // After 24 hours the slot is removed, never presented as a new 0 F measurement.
 rtc.current=DateTime(2026,11,29,0,1);messages.clear();messageRetained.clear();publishEnvironmentHourlyState();
 assert(received("msb/traffic/TestCounter/Env/temp/hourly/23",""));
 assert(received("msb/traffic/TestCounter/Env/temp/hourlyDate/23",""));
 assert(received("msb/traffic/TestCounter/Env/temp/hourly/00","24.0"));
}
