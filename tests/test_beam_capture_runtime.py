from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class BeamCaptureRuntime(unittest.TestCase):
 def test_actual_isr_queue_and_overflow(self):
  header=r'''#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <ctime>
#define DRAM_ATTR
#define IRAM_ATTR
#define ESP_OK 0
#define ESP_INTR_FLAG_IRAM 1024
#define GPIO_INTR_ANYEDGE 3
using gpio_num_t=int;
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL_ISR(p) ((void)(p))
#define portEXIT_CRITICAL_ISR(p) ((void)(p))
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
int isrFlags=0;
int gpio_install_isr_service(int flags){isrFlags=flags;return 0;}
int gpio_set_intr_type(int,int){return 0;}
int gpio_isr_handler_add(int,void(*)(void*),void*){return 0;}
struct {uint32_t in=0;struct{uint32_t val=0;}in1;}GPIO;
uint64_t physicalUs=100000;
int64_t esp_timer_get_time(){return physicalUs;}
unsigned long millis(){return uint32_t(physicalUs/1000);}
struct DateTime {
 uint32_t epoch;explicit DateTime(uint32_t e=1795453200):epoch(e){}
 uint32_t unixtime(){return epoch;}bool isValid(){return true;}int year(){return 2026;}
};
struct {DateTime now(){return DateTime(1795453200+physicalUs/1000000);}bool lostPower(){return false;}}rtc;
bool rtcReady=true;
int recordingYear(){return 2026;}
struct String:std::string{using std::string::string;String(const std::string&s):std::string(s){}String(uint32_t n):std::string(std::to_string(n)){}};
void publishMQTT(const char*,const String&,bool){}
#define topic_base_path "counter"
unsigned resets=0;
void resetBeamDetectorAfterLoss(){++resets;}
#include "traffic/BeamCapture.h"
'''
  harness=r'''
// Consumer oracle: count injected B-clear transitions after both were high.
// Real per-device qualification/rearm is covered by the detector integration tests.
bool sawBoth=false,previousB=false;unsigned cars=0;uint32_t lastAccepted=0,gap=0;
void consume(uint32_t ms,bool a,bool b){setCountEventMillis(ms);if(a && b)sawBoth=true;if(previousB && !b && sawBoth){gap=ms-lastAccepted;lastAccepted=ms;++cars;sawBoth=false;}previousB=b;}
void raw(uint64_t ms,unsigned levels){physicalUs=ms*1000;GPIO.in1.val=((levels&1)?2:0)|((levels&2)?1:0);captureEdge(nullptr);}
int main(){
 initializeBeamCapture(33,32);assert(isrFlags==ESP_INTR_FLAG_IRAM);serviceBeamCapture(50,consume);
 for(unsigned i=0;i<100;++i){uint64_t t=1000+i*1300;raw(t,1);raw(t+200,3);raw(t+900,2);raw(t+1100,0);}
 physicalUs=200000000;serviceBeamCapture(50,consume);
 assert(cars==100 && gap==1300 && lastAccepted==1000+99*1300+1100+51);
 assert(beamCaptureDropped()==0 && !strcmp(beamCaptureHealth(),"OK"));
 // ISR keeps collecting during the artificial foreground stall; wrap in the
 // low 32-bit microsecond timestamp does not move captured events into the future.
 physicalUs=4100000000ULL;serviceBeamCapture(50,consume);
 raw(4294900,1);raw(4295100,3);raw(4295800,2);raw(4296000,0);
 physicalUs=4297000000ULL;serviceBeamCapture(50,consume);assert(cars==101);
 // Excess electrical transitions exhaust the bounded queue visibly.
 for(unsigned i=0;i<3000;++i)raw(4300000+i,i&1);
 serviceBeamCapture(50,consume);assert(beamCaptureDropped()>0 && resets==1);
 assert(!strcmp(beamCaptureHealth(),"OVERFLOW_REVIEW_COUNTS"));
}
'''
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp)
   for d in ['traffic','driver','soc']:(p/d).mkdir()
   (p/'traffic/Firmware.h').write_text(header)
   for f in ['driver/gpio.h','soc/gpio_struct.h','esp_intr_alloc.h','esp_timer.h']:(p/f).write_text('')
   (p/'test.cpp').write_text('#include "'+str(ROOT/'src/services/BeamCapture.cpp')+'"\n'+harness)
   subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(p),'-I'+str(ROOT/'include'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
