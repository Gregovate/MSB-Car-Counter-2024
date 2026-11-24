from pathlib import Path
import re,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class JournalInspection(unittest.TestCase):
 def test_actual_backup_worker_preserves_source_and_verifies_sd(self):
  source=(ROOT/'src/services/JournalInspection.cpp').read_text()
  def body(name):return re.search(r'(?:void|uint32_t) '+name+r'\([^\n]*\) \{.*?^\}',source,re.S|re.M).group()
  harness=r'''
#include <cassert>
#include <cstring>
#include <cstdint>
#include <atomic>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <algorithm>
constexpr uint32_t inspectionBytes=1048576,blockBytes=4096;
enum InspectionStage {Waiting,Copying,Verifying,Complete,Failed};
std::atomic<const char*> inspectionFailure{"NONE"};
std::atomic<unsigned> inspectionStage{Waiting};std::atomic<uint32_t> progress{0};
std::vector<uint8_t> flash(inspectionBytes,0x42);
int sourceFail=-1;bool shortWrite=false,corruptRead=false;
struct Partition {bool read(uint32_t offset,void*out,size_t n){if(int(offset)==sourceFail)return false;memcpy(out,flash.data()+offset,n);return true;}}inspectionPartition;
bool inspectionLayout=true,attempted=false;uint32_t backupHash=2166136261U;
uint8_t sourceBlock[blockBytes],verifyBlock[blockBytes];
std::map<std::string,std::vector<uint8_t>> disk;
std::set<std::string> folders;
struct File {
 std::string path;size_t pos=0;bool valid=false;
 operator bool(){return valid;}
 size_t write(const uint8_t*p,size_t n){if(shortWrite)--n;disk[path].insert(disk[path].end(),p,p+n);return n;}
 size_t read(uint8_t*p,size_t n){n=std::min(n,disk[path].size()-pos);memcpy(p,disk[path].data()+pos,n);pos+=n;if(corruptRead && n)p[0]^=1;return n;}
 size_t size(){return disk[path].size();}void flush(){}void close(){valid=false;}
}inspectionFile;
std::string inspectionFolder="/CC/2026/TrafficJournalInspection",backupPath=inspectionFolder+"/flash-test.bin",pendingPath=backupPath+".pending";
struct {bool exists(const std::string&p){return disk.count(p)||folders.count(p);}}SD;
struct {template<class...T>void printf(const char*,T...){}void println(const char*){}}Serial;
bool sdAvailable=true,ready=true;const char*health="FOREIGN_DATA_PRESERVED";
bool countsReady(){return ready;}const char*trafficJournalHealth(){return health;}
#define FILE_WRITE "w"
#define FILE_READ "r"
bool recordingMkdir(const std::string&p){folders.insert(p);return true;}
File recordingOpen(const std::string&p,const char*mode){if(!strcmp(mode,"w")){disk[p].clear();return {p,0,true};}return {p,0,disk.count(p)>0};}
bool recordingRename(const std::string&from,const std::string&to){if(!disk.count(from)||disk.count(to))return false;disk[to]=disk[from];disk.erase(from);return true;}
'''+body('extendHash')+'\n'+body('failInspection')+'\n'+body('serviceJournalInspection')+r'''
void reset(){inspectionFile.close();disk.clear();folders.clear();progress=0;inspectionStage=Waiting;attempted=false;sourceFail=-1;shortWrite=corruptRead=false;backupHash=2166136261U;}
void run(){for(unsigned i=0;i<1000 && inspectionStage!=Complete && inspectionStage!=Failed;++i)serviceJournalInspection();}
int main(){
 auto original=flash;
 sdAvailable=false;serviceJournalInspection();assert(!attempted && disk.empty());sdAvailable=true;
 ready=false;serviceJournalInspection();assert(!attempted && disk.empty());ready=true;
 health="OK";serviceJournalInspection();assert(!attempted && disk.empty());health="FOREIGN_DATA_PRESERVED";
 run();assert(inspectionStage==Complete && disk.at(backupPath)==flash && flash==original);
 assert(backupHash==extendHash(2166136261U,flash.data(),flash.size()));
 reset();shortWrite=true;run();assert(inspectionStage==Failed && !disk.count(backupPath) && flash==original);
 reset();sourceFail=4096;run();assert(inspectionStage==Failed && !disk.count(backupPath) && flash==original);
 reset();corruptRead=true;run();assert(inspectionStage==Failed && !disk.count(backupPath) && flash==original);
}
'''
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);(p/'test.cpp').write_text(harness)
   subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
 def test_no_internal_write_or_erase_route(self):
  source=(ROOT/'src/services/JournalInspection.cpp').read_text()
  self.assertNotRegex(source,r'inspectionPartition\.(write|erase)\s*\(')
  self.assertNotIn('HTTP_POST',source)
