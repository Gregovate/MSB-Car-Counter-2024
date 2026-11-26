#define TRAFFIC_OPERATING_TEST
#define main count_recovery_scenarios
#include "count_recovery_test.cpp"
#undef main
#include "../src/services/OperatingSettings.cpp"
#include "../src/services/EnvironmentService.cpp"
#include "../src/services/ShowSummary.cpp"
void configure(bool) {
    settings=traffic::OperatingSettings{};
    settings.issuedDate=20261127;settings.endDate=20261231;settings.downDate=20261224;
}
#include "../src/services/TemperatureSensor.cpp"
int main() {
    rtc.current=DateTime(2026,11,27,17,0);configure(false);
    ticks=0;messages.clear();publishEnvironmentState();
    assert(messages[messages.size()-2].second=="{\"tempF\": null, \"humidity\": null}");
    readTempandRH();assert(dht.calls==0);
    ticks=59999;readTempandRH();assert(dht.calls==0);
    ticks=60000;dht.temperature=0;readTempandRH();
    assert(dht.calls==1 && tempF==0 && environmentSamples.count[17]==1);
    assert(messages[messages.size()-2].second.find("0.0")!=std::string::npos);
    ticks=61000;readTempandRH();assert(dht.calls==1); // No duplicate samples.
    ticks=120000;dht.temperature=60;readTempandRH();
    float average=-1;assert(showTemperatureAverage(average) && average==30);
    ticks=180000;dht.temperature=NAN;readTempandRH();
    assert(dhtFailures==1 && dhtConsecutiveFailures==1 && environmentSamples.count[17]==2);
    assert(std::string(dhtRejection)=="TEMPERATURE_READ_FAILED");
    assert(messages.back().second.find("\"lastReadTempF\":null")!=std::string::npos);
    assert(messages[messages.size()-2].second.find("null")!=std::string::npos);
    // Failed sensor must still close the hour. Missing hour never becomes 0 F.
    rtc.current=DateTime(2026,11,27,18,0);ticks=240000;readTempandRH();
    assert(environmentSamples.hour==18 && environmentSamples.count[18]==0);
    assert(hourlyTemp[17]==30 && isnan(hourlyTemp[18]));
    bool closed=false;for(auto& m:messages)
        if(m.first=="msb/traffic/TestCounter/Env/temp/hourly/17" && m.second=="30.0")closed=true;
    assert(closed);
    bool retainedHour=false,dateMarker=false,unknownCleared=false;
    for(size_t i=0;i<messages.size();++i) {
        if(messages[i].first=="msb/traffic/TestCounter/Env/temp/hourly/17" && messages[i].second=="30.0" && messageRetained[i])retainedHour=true;
        if(messages[i].first=="msb/traffic/TestCounter/Env/temp/hourlyDate/17" && messages[i].second=="20261127" && messageRetained[i])dateMarker=true;
        if(messages[i].first=="msb/traffic/TestCounter/Env/temp/hourly/18" && messages[i].second.empty() && messageRetained[i])unknownCleared=true;
    }
    assert(retainedHour && dateMarker && unknownCleared);
    // Recovery publishes immediately, weighted average is 40 rather than 45.
    ticks=300000;dht.temperature=60;dht.rh=50;readTempandRH();
    assert(dhtConsecutiveFailures==0 && dhtFailures==2);
    assert(showTemperatureAverage(average) && average==40);
    // Pair validation: bad humidity cannot add an otherwise valid temperature.
    ticks=360000;dht.rh=101;readTempandRH();assert(environmentSamples.count[18]==1);
    assert(std::string(dhtRejection)=="HUMIDITY_RANGE");
    assert(messages.back().second.find("\"lastReadHumidity\":101")!=std::string::npos);
    assert(messages.back().second.find("\"lastReadTempF\":60")!=std::string::npos);
    assert(messages.back().second.find("\"pin\":4")!=std::string::npos);
    // Show cutoff excludes later samples from show average, not hourly average.
    rtc.current=DateTime(2026,11,27,22,0);ticks=420000;dht.rh=50;dht.temperature=100;readTempandRH();
    assert(showTemperatureAverage(average) && average==40 && environmentSamples.count[22]==1);
    // Day rollover clears sums and missing slots even with the same hour number.
    rtc.current=DateTime(2026,11,28,22,0);serviceEnvironmentHour();
    assert(environmentSamples.date==20261128 && environmentSamples.count[22]==0);
    assert(!showTemperatureAverage(average) && isnan(hourlyTemp[17]));
    // Clock rollback must not mix samples into a later bucket.
    assert(!environmentSamples.add(20261127,22,40,50,true));
    assert(!environmentSamples.add(20261128,21,40,50,true));
    assert(traffic::validEnvironment(32,50) && traffic::validEnvironment(31.8f,50));
    assert(!traffic::validEnvironment(INFINITY,50));
    assert(!traffic::validEnvironment(-998,50));
    assert(!traffic::validEnvironment(-999,50));
    // Actual sensor path must accept the observed negative-encoding packet.
    ticks=480000;dht.temperature=-5860.66f;dht.rh=66.8f;readTempandRH();
    assert(fabs(tempF-26.42f)<.01f && dhtCurrentValid && dhtEncodingCorrected);
    publishEnvironmentState();
    assert(messages.back().second.find("TWOS_COMPLEMENT_COMPAT")!=std::string::npos);
    assert(lastRawTemperature==dht.temperature);
    // No valid samples after reboot: CSV field must be blank, never invented 0.
    legacy();rtc.current=DateTime(2026,11,29,22);configure(false);assert(restoreCountState());officialCountsComplete=true;
    environmentSamples={};assert(writeOfficialSummaries());
    assert(disk["/CC/2026/ShowSummary.csv"].find(",,0\n")!=std::string::npos);
    // Canonical measurement/health layout is independent of legacy topic config.
    bool canonical=false,health=false,legacyMeasurement=false,legacyHour=false;
    for(const auto& m:messages) {
        if(m.first=="msb/traffic/TestCounter/Env/temp") canonical=true;
        if(m.first=="msb/traffic/TestCounter/System/dhtHealth") health=true;
        if(m.first=="msb/traffic/TestCounter/Env/tempHumidity") legacyMeasurement=true;
        if(m.first=="msb/traffic/TestCounter/Env/tempHumidity/hourly/17") legacyHour=true;
    }
    assert(canonical && health);
#ifdef TRAFFIC_TEST_LEGACY_ENV
    assert(legacyMeasurement && legacyHour);
#else
    assert(!legacyMeasurement && !legacyHour);
#endif
    std::cout<<"PASS: real DHT/averaging services, warmup, null reporting, failure/recovery, zero F, weighted means, boundaries and missing summaries\n";
}
