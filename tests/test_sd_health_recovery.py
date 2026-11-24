from pathlib import Path
import re,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class SdHealthRecovery(unittest.TestCase):
 def test_actual_health_recovers_missing_and_transient_sd(self):
  source=(ROOT/'src/services/StorageService.cpp').read_text()
  body=re.search(r'void checkSdHealth\(\) \{.*?^\}',source,re.S|re.M).group()
  harness=r'''
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#define topic_base_path "test"
using String=std::string;
#define CARD_NONE 0
#define PIN_SPI_CS 5
#define FILE_WRITE "w"
#define FILE_READ "r"
#define MQTT_PUB_SD_HEALTH "health"
#define MQTT_PUB_SD_STATUS "status"
#define MQTT_DEBUG_LOG "debug"
int SPI=0,mounts=0,folders=0;bool mounted=false,mountWorks=false,writesWork=true,readsWork=true,sdAvailable=false;
std::string contents,status;
struct SDMock{int cardType(){return mounted?1:0;}bool begin(int,int,int){++mounts;mounted=mountWorks;return mounted;}uint64_t totalBytes(){return 32ULL*1024*1024*1024;}}SD;
struct File{bool valid=true;operator bool(){return valid;}size_t print(const char*s){contents=writesWork?s:"";return contents.size();}void flush(){}void close(){}size_t size(){return contents.size();}int read(){return contents.empty()?-1:contents[0];}};
File recordingOpen(const char*,const char*mode){return {mounted && (strcmp(mode,"r") || readsWork)};}
bool productionCardMatches(){return true;}
int recordingYear(){return 2026;}
void ensureSeasonFolderExists(){++folders;}void ensureSeasonFolderExists(int){++folders;}
void recordingRemove(const char*){contents="";}
void publishMQTT(const char*t,const char*s,bool=false){if(std::string(t).find("status")!=std::string::npos)status=s;}
'''+body+r'''
int main(){
 checkSdHealth();assert(!sdAvailable && mounts==1 && folders==0);
 mountWorks=true;checkSdHealth();assert(sdAvailable && mounts==2 && folders==1 && status=="OK_32768MB");
 writesWork=false;checkSdHealth();assert(!sdAvailable && mounts==2);
 writesWork=true;readsWork=false;checkSdHealth();assert(!sdAvailable);
 readsWork=true;checkSdHealth();assert(sdAvailable && mounts==2); // existing mount survives
}
'''
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);(p/'test.cpp').write_text(harness)
   subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
