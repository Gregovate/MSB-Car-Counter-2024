from pathlib import Path
import re,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class DetectorIntegration(unittest.TestCase):
    def test_real_detector_and_debug_control(self):
        # Compile the actual detection, debounce, debug and arch services. Count
        # persistence is covered by the existing real count-service suite.
        source=(ROOT/'src/services/VehicleDetector.cpp').read_text()
        source=re.sub(r'void countTheCar\(\) \{.*?^\}', 'void countTheCar(){++totalDailyCars;noCarTimer=millis();}',source,count=1,flags=re.S|re.M)
        header=r'''#pragma once
#include <string>
#include <cstring>
#include <cstdio>
#include <vector>
#include <cassert>
#include <iostream>
#define LOW 0
#define HIGH 1
struct String:std::string {
 using std::string::string;
 String(const std::string&s):std::string(s){}
 String(unsigned int n):std::string(std::to_string(n)){}
 String(int n):std::string(std::to_string(n)){}
 String(unsigned long n):std::string(std::to_string(n)){}
};
unsigned long ticks=100;
unsigned long millis(){return ticks;}
int totalDailyCars=26;
unsigned long noCarTimer=0,start_MqttMillis=0,patternModeMillis=0;
unsigned long firstBeamTripTime=0,abFollow_ms=0,timeToPassMS=0,timeBetweenCars_ms=0,lastCarDetectedMillis=0;
unsigned long lastFirstBeamChangeTime=0,lastSecondBeamChangeTime=0,debounceDelay=50,minActivationDuration=100,waitDuration=500,carCounterTimeout=60000;
bool stableFirstBeamState=false,stableSecondBeamState=false,carStuckAlarmActive=false;
int firstBeamState=0,secondBeamState=0;
enum Detector {WAITING_FOR_CAR,FIRST_BEAM_HIGH,BOTH_BEAMS_HIGH,CAR_DETECTED};
Detector currentCarDetectState=WAITING_FOR_CAR;
enum Pattern {INITIAL_OFF,RED_ON,GREEN_ON,BOTH_ON,BOTH_OFF,RED_OFF_GREEN_ON,GREEN_OFF_RED_ON};
Pattern currentPatternState=INITIAL_OFF;
const int firstBeamPin=33,secondBeamPin=32,redArchPin=25,greenArchPin=26;
int pins[40]{};int writes=0;
int digitalRead(int p){return pins[p];}
void digitalWrite(int p,int n){pins[p]=n;++writes;}
struct SerialMock {template<class...T>void printf(const char*,T...){}template<class T>void print(T){}template<class T>void println(T){}} Serial;
struct Record {String topic,payload;bool retained;};
std::vector<Record> messages;
void publishMQTT(const char*t,const String&s,bool r=false){messages.push_back({t,s,r});}
bool online=true;
bool mqttOnline(){return online;}
struct Client {int getBufferSize(){return 256;}bool setBufferSize(int){return true;}} mqtt_client;
String bootTimestamp="2026-11-17 12:00:00";
String getRtcTimestamp(){return "2026-11-17 12:01:00";}
void stopIdleArchPattern();
#define topic_base_path "msb/traffic/CarCounter"
#define MQTT_PUB_BEAM_A_STATE "beamA"
#define MQTT_PUB_BEAM_B_STATE "beamB"
#define MQTT_PUB_ALARM "alarm"
#define MQTT_PUB_BEAM_AB_MS "ab"
#define MQTT_PUB_BEAM_B_BROKEN_MS "bms"
#define MQTT_PUB_TTP "ttp"
#define MQTT_PUB_BETWEENCARS_MS "between"
#define MQTT_COUNTER_LOG "log"
'''
        harness=r'''
void sample(int a,int b,unsigned long t){ticks=t;pins[firstBeamPin]=a;pins[secondBeamPin]=b;detectCar();ticks=t+60;detectCar();}
void reverse(unsigned long t){sample(0,1,t);sample(1,1,t+200);sample(1,0,t+1200);sample(0,0,t+1500);}
void forward(unsigned long t){sample(1,0,t);sample(1,1,t+200);sample(0,1,t+1000);sample(0,0,t+1300);}
unsigned events(){unsigned n=0;for(auto&m:messages)if(m.topic=="msb/traffic/CarCounter/Sensors/passageEvent" && m.payload.size() && m.payload.find("SESSION")==std::string::npos) {++n;assert(!m.retained);}return n;}
int main(){
 sample(0,0,100);reverse(1000);reverse(7000);assert(totalDailyCars==26 && events()==0 && noCarTimer==0 && writes==0);
 // OFF is default. Exact ON/OFF commands only, independent of count settings.
 assert(!handlePassageDebugCommand("other","ON"));
 assert(handlePassageDebugCommand("msb/traffic/CarCounter/Config/passageDebug","ON"));assert(passageDebug);
 reverse(14000);assert(totalDailyCars==26 && events()==1 && writes==0);
 auto last=messages.back().payload;assert(last.find("REVERSE_B_FIRST")!=std::string::npos);
 assert(last.find("\"before\":26,\"after\":26")!=std::string::npos);
 // Short walking-length overlap: no counts or arch writes.
 sample(1,0,20000);sample(1,1,20200);sample(0,1,20400);sample(0,0,20600);
 assert(totalDailyCars==26 && writes==0);
 forward(26000);assert(totalDailyCars==27 && noCarTimer>26000 && writes==2);
 forward(32000);assert(totalDailyCars==28); // six seconds apart
 // Both held through stuck timeout: alarm is diagnostic, not a discard/reset.
 sample(1,0,40000);sample(1,1,40200);sample(1,1,102000);
 assert(totalDailyCars==28 && carStuckAlarmActive);
 sample(0,1,103000);sample(0,0,104000);assert(totalDailyCars==29 && !carStuckAlarmActive);
 sample(0,0,105000);assert(totalDailyCars==29);
 unsigned before=events();handlePassageDebugCommand("msb/traffic/CarCounter/Config/passageDebug","OFF");
 forward(110000);assert(totalDailyCars==30 && events()==before);
 handlePassageDebugCommand("msb/traffic/CarCounter/Config/passageDebug","ON");resetPassageDebugConnection();assert(!passageDebug);
 assert(messages.back().payload=="OFF");
 std::cout<<"PASS: actual debounce/direction/count callback, reverse pair, walkers, six-second cars, stopped car, debug ON/OFF/reconnect and arch ownership\n";
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);(p/'traffic').mkdir();(p/'traffic/Firmware.h').write_text(header)
            (p/'test.cpp').write_text(source+'\n#include "'+str(ROOT/'src/services/ArchEffects.cpp')+'"\n'+harness)
            subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(p),'-I'+str(ROOT/'include'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
