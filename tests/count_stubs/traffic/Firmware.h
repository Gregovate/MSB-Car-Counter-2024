#pragma once
// Host-only test doubles. Never included in a PlatformIO firmware build.
#include <string>
#include <map>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include <algorithm>
#include "traffic/RecordingPolicy.h"
struct String : std::string {
    using std::string::string;
    String() = default;
    String(const std::string &s):std::string(s){}
    String(int n):std::string(std::to_string(n)){}
    void trim() { while(!empty() && isspace(back())) pop_back(); while(!empty() && isspace(front())) erase(0,1); }
    void toLowerCase() { std::transform(begin(),end(),begin(),[](unsigned char c){return tolower(c);}); }
    int indexOf(char c,int start=0) const {auto n=find(c,start);return n==npos ? -1 : int(n);}
    int indexOf(const char* s) const {auto n=find(s);return n==npos ? -1 : int(n);}
    String substring(int start,int end=-1) const {return substr(start,end<0 ? npos : end-start);}
    bool startsWith(const String& p) const {return rfind(p,0)==0;}
    bool startsWith(const char* p) const { return rfind(p,0)==0; }
};
struct DateTime {
    int y,m,d,h,mi,s;
    DateTime(int yy=2026,int mm=11,int dd=4,int hh=0,int mn=0,int ss=0):y(yy),m(mm),d(dd),h(hh),mi(mn),s(ss){}
    int minute()const{return mi;} int second()const{return s;} int hour()const{return h;} int year()const{return y;} int month()const{return m;} int day()const{return d;}
    bool isValid() const{return traffic::validDate(y,m,d);}
    uint32_t unixtime() const { tm t={};t.tm_year=y-1900;t.tm_mon=m-1;t.tm_mday=d;t.tm_hour=h;t.tm_min=mi;t.tm_sec=s;return timegm(&t); }
};
inline struct Rtc { DateTime current;bool lost=false;bool lostPower(){return lost;}DateTime now(){return current;} } rtc;
inline bool rtcReady=true;
inline unsigned long ticks=10000;
inline unsigned long millis(){return ticks;}
inline void SetLocalTime(){}
inline struct SerialMock { template<class T>void print(T){} template<class T>void println(T){} template<class... T>void printf(const char*,T...){} } Serial;
#define F(s) s
#define FILE_APPEND "a"
#define FILE_READ "r"
#define FILE_WRITE "w"
inline std::map<std::string,std::string> disk;
inline std::string failWrite, failRead, failRename;
struct File {
    std::string path; bool valid=false; size_t pos=0;
    operator bool() const {return valid;}
    size_t position() const {return pos;}
    size_t size() const {return valid ? disk[path].size() : 0;}
    size_t write(const uint8_t* p,size_t n) {
        size_t written=path==failWrite ? n/2 : n;
        disk[path].append(reinterpret_cast<const char*>(p),written);return written;
    }
    size_t read(uint8_t* p,size_t n) {
        if(path==failRead)return 0;
        n=std::min(n,size()-pos);memcpy(p,disk[path].data()+pos,n);pos+=n;return n;
    }
    size_t print(const char* s){return write(reinterpret_cast<const uint8_t*>(s),strlen(s));}
    size_t print(const String& s){return print(s.c_str());}
    size_t print(int n){return print(std::to_string(n).c_str());}
    template<class T>void println(T value){print(value);print("\n");}
    void flush(){}void close(){}
    bool available()const{return valid && pos<size();}
    String readString(){String s=disk[path].substr(pos);pos=size();return s;}
    String readStringUntil(char c){auto end=disk[path].find(c,pos);auto s=disk[path].substr(pos,end==std::string::npos ? end : end-pos);pos=end==std::string::npos ? size() : end+1;return s;}
};
inline bool sdAvailable=true;
#define CARD_NONE 0
inline struct Sd {
    int cardType(){return sdAvailable ? 1 : CARD_NONE;}
    File open(const String&p,const char* mode){
        if(strcmp(mode,"r")==0)return {p,disk.count(p)>0 && p!=failRead};
        if(strcmp(mode,"a"))disk[p]="";
        return {p,true};
    }
    bool exists(const String&p){return disk.count(p)>0;}
} SD;
inline int daysRunning=0,totalDailyCars=7,totalShowCars=42;
inline String seasonFolder="/CC/2026";
inline std::vector<std::pair<String,String>> messages;
inline void publishMQTT(const char* t,const String& s,bool=false){messages.emplace_back(t,s);}
#define topic_base_path "msb/traffic/TestCounter"
#define MQTT_DEBUG_LOG "debug"
#define MQTT_PUB_SEASON_YEAR "year"
#define MQTT_PUB_SEASON_FOLDER "folder"
#define MQTT_PUB_DAYSRUNNING "days"

#include "traffic/CountPersistence.h"
inline int recordingYear(){return rtc.now().year();}
inline int recordingDaysRunning(){return 0;}
inline bool flagDailyShowStartReset=false,flagDailyShowSummarySaved=false,flagDailySummarySaved=false,flagDaysRunningReset=false;
inline unsigned int dayOfMonth=0,lastDayOfMonth=0,hourlyCount[24]{};
inline String fileName1="/EnterTotal.txt",fileName2="/ShowTotal.txt",fileName3="/DayOfMonth.txt",fileName5="/HourlyData.csv";
inline File recordingOpen(const String&p,const char* mode){
    if(strcmp(mode,FILE_READ) && !traffic::writablePath(p.c_str(),recordingYear(),false))return {};
    return SD.open(p,mode);
}
inline void saveDailyTotal(){} inline void saveShowTotal(){} inline void saveDayOfMonth(){}

inline File myFile;
inline String fileName6="/EventLog.csv";
inline unsigned long noCarTimer=0,start_MqttMillis=0;
inline int timeToPassMS=100,abFollow_ms=50,timeBetweenCars_ms=1000;
inline int inParkCars=0,carCounterCars=100,beamAState=0,beamBState=0;
inline bool showTime=false;
inline void saveHourlyCounts(){}
#define MQTT_PUB_CARS_HOURLY "hour/"
#define MQTT_PUB_ENTER_CARS "enter"
#define MQTT_PUB_EXIT_CARS "exit"
#define MQTT_PUB_SHOWTOTAL "show"
#define MQTT_PUB_INPARK_CARS "park"
#define MQTT_PUB_TIME "time"
#define MQTT_PUB_BEAM_A_STATE "beamA"
#define MQTT_PUB_BEAM_B_STATE "beamB"
#define MQTT_PUB_TTP "ttp"

inline bool recordingRemove(const String&p){return disk.erase(p)>0;}
inline bool recordingRename(const String&a,const String&b){
    if(a==failRename || !disk.count(a))return false;
    disk[b]=disk[a];disk.erase(a);return true;
}
