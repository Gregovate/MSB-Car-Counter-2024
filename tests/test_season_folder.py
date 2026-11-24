from pathlib import Path
import re,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class SeasonFolderContract(unittest.TestCase):
 def test_shared_type_and_gate_path_adapters(self):
  self.assertRegex((ROOT/'include/traffic/RuntimeState.h').read_text(),r'extern\s+SeasonFolder\s+seasonFolder\s*;')
  state=(ROOT/'src/services/RuntimeState.cpp').read_text()
  definition=re.search(r'SeasonFolder seasonFolder\s*=\s*"[^"]*";',state).group(0)
  source=(ROOT/'src/services/StorageService.cpp').read_text()
  gate='String sPath(' in source
  funcs=''
  if gate:
   for name in ['ensureSeasonFolderExists','sPath','initSeasonalPaths','applyRecordingCalendar']:
    start=re.search(r'^(?:void|String) '+name+r'\([^\n]*\)\s*\{',source,re.M).start()
    brace=source.index('{',start);depth=1;end=brace+1
    while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
    funcs+=source[start:end]+'\n'
  harness=r'''
#include <string>
#include <cstdio>
#include <cstdint>
#include <cassert>
#include <type_traits>
#include "traffic/SeasonFolder.h"
struct String:std::string{using std::string::string;String()=default;String(const std::string&s):std::string(s){}String(int n):std::string(std::to_string(n)){} };
DEFINITION
static_assert(std::is_array<SeasonFolder>::value && std::extent<SeasonFolder>::value==32,"Fixed shared path ABI");
int year=2026;int recordingYear(){return year;}
struct DateTime {uint16_t year(){return 2026;}uint8_t month(){return 11;}uint8_t day(){return 27;}};
struct Rtc{DateTime now(){return {};}}rtc;
DateTime recordingShowStart(){return {};}
int determineSeasonYear(DateTime){return recordingYear();}
int showStartYear=0,showStartMonth=0,showStartDay=0;bool showStartDateValid=false;String showStartDate;
String fileName1,fileName2,fileName3,fileName4,fileName5,fileName6,fileName7,fileNameShowStart;
#define CARD_NONE 0
struct Sd{int card=0;int cardType(){return card;}bool exists(const String&){return false;}}SD;
bool recordingMkdir(const String&){return true;}void noteFreshCountFolder(){}
struct SerialMock{template<class...T>void printf(const char*,T...){}}Serial;
const char* MQTT_PUB_SEASON_YEAR="year";const char* MQTT_PUB_SEASON_FOLDER="folder";
void publishMQTT(const char*,const String&,bool){}void publishDebugEvent(const char*,const String&,bool){}
void checkAndCreateFile(const String&,const char* =nullptr){}void createAndInitializeHourlyFile(const String&){}
FUNCTIONS
int main(){
TESTS
}
'''.replace('DEFINITION',definition).replace('FUNCTIONS',funcs).replace('TESTS',('''applyRecordingCalendar();assert(std::string(seasonFolder)=="/GC/2026" && showStartDate=="2026-11-27");
 SD.card=1;year=2030;initSeasonalPaths();assert(std::string(seasonFolder)=="/GC/2030");
 assert(fileName6=="/GC/2030/ExitLog.csv");''' if gate else 'assert(std::string(seasonFolder)=="/CC/0000");'))
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);(p/'test.cpp').write_text(harness)
   subprocess.run(['g++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
