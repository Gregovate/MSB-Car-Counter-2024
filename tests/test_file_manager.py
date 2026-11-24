from pathlib import Path
import subprocess,tempfile,unittest,shutil
ROOT=Path(__file__).resolve().parents[1]
class FileManager(unittest.TestCase):
 def test_actual_routes_work_without_sd_and_default_to_active_season(self):
  header=r'''#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cassert>
#include <cstdio>
#include <ctime>
#include <cstdint>
#define PROGMEM
#define HTTP_GET 0
struct String:std::string {using std::string::string;String()=default;String(const std::string&s):std::string(s){}String(size_t v):std::string(std::to_string(v)){} };
struct AsyncWebServerResponse {int status;std::string type,body;void addHeader(const char*,const char*){} };
struct AsyncWebServerRequest {
 AsyncWebServerResponse result{};
 AsyncWebServerResponse* beginResponse_P(int n,const char*t,const char*b){result={n,t,b};return &result;}
 AsyncWebServerResponse* beginResponse(int n,const char*t,const char*b){return beginResponse_P(n,t,b);}
 void send(AsyncWebServerResponse*){}
 void send(int n,const char*t,const String&b){result={n,t,b};}
};
struct Server{std::map<std::string,std::function<void(AsyncWebServerRequest*)>> routes;
 template<class F>void on(const char*p,int,F f){routes[p]=f;}}server;
String currentDirectory="/";char seasonFolder[32]="/GC/2026";
const char* THIS_MQTT_CLIENT="espGateCounter";const char* FWVersion="test";
bool sdAvailable=false,ready=true;int totalDailyCars=19,totalShowCars=21;
bool countsReady(){return ready;}const char* trafficJournalHealth(){return "OK";}
const char* trafficJournalArchiveHealth(){return sdAvailable?"OK":"SD_OFFLINE_FLASH_ONLY";}
uint32_t trafficJournalFreeRecords(){return 3249;}const char* beamCaptureHealth(){return "OK";}
uint32_t beamCaptureDropped(){return 0;}
struct File{int kind=0,cursor=0;operator bool()const{return kind!=0;}bool isDirectory(){return kind==1||kind==2;}
 File openNextFile(){++cursor;return {cursor==1?2:cursor==2?3:0,0};}
 const char* name(){return kind==2?"TrafficJournal":"EnterLog.csv";}
 size_t size(){return 99;}time_t getLastWrite(){return 0;}void close(){} };
File recordingOpen(const String&){return {1,0};}
void listSDFiles(AsyncWebServerRequest*);
'''
  harness=r'''
int main(){setupFileManagerRoutes();AsyncWebServerRequest req;
 server.routes.at("/")(&req);assert(currentDirectory=="/GC/2026" && req.result.status==200);
 assert(req.result.body.find("Storage and recovery")!=std::string::npos);
 server.routes.at("/storageStatus")(&req);assert(req.result.status==200);
 assert(req.result.body.find("\"sdAvailable\":false")!=std::string::npos);
 assert(req.result.body.find("\"journalFreeRecords\":3249")!=std::string::npos);
 assert(totalDailyCars==19 && totalShowCars==21);
 server.routes.at("/listFiles")(&req);assert(req.result.status==503);
 sdAvailable=true;server.routes.at("/listFiles")(&req);
 assert(req.result.status==200 && req.result.body.find("TrafficJournal/")!=std::string::npos);
 assert(req.result.body.find("EnterLog.csv")!=std::string::npos);
 currentDirectory="/";server.routes.at("/")(&req);assert(currentDirectory==seasonFolder);
 ready=false;server.routes.at("/storageStatus")(&req);
 assert(req.result.body.find("\"countsReady\":false")!=std::string::npos);
}
'''
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);(p/'traffic').mkdir();(p/'traffic/Firmware.h').write_text(header)
   (p/'test.cpp').write_text('#include "'+str(ROOT/'src/services/FileManagerService.cpp')+'"\n'+harness)
   subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(p),'-I'+str(ROOT/'include'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
 def test_embedded_page_matches_shared_html_and_script_runs(self):
  html=(ROOT/'data/index.html').read_text();header=(ROOT/'include/traffic/FileManagerPage.h').read_text()
  self.assertEqual(header.split('R"CCPAGE(',1)[1].rsplit(')CCPAGE";',1)[0],html)
  script=html.split('<script>',1)[1].split('</script>',1)[0]
  if not shutil.which('node'):self.skipTest('Node required for UI execution check')
  harness=r'''
const assert=require('assert');const elements={};
function element(){return {textContent:'',hidden:false,value:'',appendChild(){},replaceChildren(...v){this.children=v;},style:{setProperty(){}}};}
global.document={documentElement:element(),getElementById(id){return elements[id]||(elements[id]=element());},createElement:element};
global.setInterval=()=>0;
global.fetch=async(url)=>({ok:true,text:async()=>url==='/storageStatus'?JSON.stringify({device:'espGateCounter',firmware:'test',seasonFolder:'/GC/2026',sdAvailable:false,countsReady:true,journalHealth:'OK',journalArchive:'SD_OFFLINE_FLASH_ONLY',journalFreeRecords:1200,beamCaptureHealth:'OK',beamCaptureDropped:0,daily:3,show:21}):'SD OFFLINE'});
'''
  checks=r'''
setImmediate(()=>{assert.equal(elements.title.textContent,'Gate Counter File Manager');assert.equal(elements.directory.value,'/GC/2026');assert.equal(elements.health.children.length,9);assert.equal(elements.files.textContent,'SD OFFLINE');assert(elements.health.children[0].className.includes('bad'));});
'''
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp)/'page.js';p.write_text(harness+script+checks)
   subprocess.run(['node',str(p)],check=True)
