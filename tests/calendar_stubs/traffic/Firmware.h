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
    void trim() { while(!empty() && isspace(back())) pop_back(); }
    void toLowerCase() { std::transform(begin(),end(),begin(),[](unsigned char c){return tolower(c);}); }
    int indexOf(const char* p) const {auto n=find(p);return n==npos ? -1 : int(n);}
    bool startsWith(const char* p) const { return rfind(p,0)==0; }
};
struct DateTime {
    int y,m,d,h,mi,s;
    DateTime(int yy=2026,int mm=11,int dd=4,int hh=0,int mn=0,int ss=0):y(yy),m(mm),d(dd),h(hh),mi(mn),s(ss){}
    int year()const{return y;} int month()const{return m;} int day()const{return d;}
    bool isValid() const{return traffic::validDate(y,m,d);}
    uint32_t unixtime() const { tm t={};t.tm_year=y-1900;t.tm_mon=m-1;t.tm_mday=d;t.tm_hour=h;t.tm_min=mi;t.tm_sec=s;return timegm(&t); }
};
inline struct Rtc { DateTime current;bool lost=false;bool lostPower(){return lost;}DateTime now(){return current;} } rtc;
inline bool rtcReady=true;
inline unsigned long ticks=10000;
inline unsigned long millis(){return ticks;}
inline void SetLocalTime(){}
inline struct SerialMock { template<class... T>void printf(const char*,T...){} } Serial;
#define FILE_READ "r"
#define FILE_WRITE "w"
inline std::map<std::string,std::string> disk;
inline std::string failWrite;
struct File {
    std::string path;bool valid=false;
    operator bool() const {return valid;}
    size_t print(const char* s) { if(path==failWrite)return 0; disk[path]+=s;return strlen(s); }
    void flush(){}void close(){}
    String readStringUntil(char c){return disk[path].substr(0,disk[path].find(c));}
};
inline struct Sd {
    File open(const String&p,const char* mode){
        if(strcmp(mode,"r")==0)return {p,disk.count(p)>0};
        disk[p]="";return {p,true};
    }
    bool mkdir(const String&p){disk[p]="";return true;}
    bool remove(const String&p){return disk.erase(p)>0;}
    bool rename(const String&a,const String&b){if(!disk.count(a))return false;disk[b]=disk[a];disk.erase(a);return true;}
} SD;
inline int daysRunning=0,totalDailyCars=7,totalShowCars=42;
inline String seasonFolder;
inline std::vector<std::pair<String,String>> messages;
inline void publishMQTT(const char* t,const String& s,bool=false){messages.emplace_back(t,s);}
#define topic_base_path "msb/traffic/TestCounter"
#define MQTT_DEBUG_LOG "debug"
#define MQTT_PUB_SEASON_YEAR "year"
#define MQTT_PUB_SEASON_FOLDER "folder"
#define MQTT_PUB_DAYSRUNNING "days"
int recordingYear();
DateTime recordingShowStart();
int recordingDaysRunning();
void publishRecordingCalendar();
inline void applyRecordingCalendar(){seasonFolder=String("/CC/")+String(recordingYear());}

bool recordingPathWritable(const String &, bool mkdirOnly=false);
