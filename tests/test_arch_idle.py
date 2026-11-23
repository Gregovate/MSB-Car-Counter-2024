from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class IdleArches(unittest.TestCase):
    def test_actual_arch_service(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);(p/'traffic').mkdir()
            (p/'traffic/Firmware.h').write_text('''#pragma once
#include <cassert>
#define HIGH 1
#define LOW 0
unsigned long ticks=0,noCarTimer=0,patternModeMillis=0;
bool occupied=false;
bool vehicleArchFeedbackActive(){return occupied;}
unsigned long millis(){return ticks;}
enum Pattern {INITIAL_OFF,RED_ON,GREEN_ON,BOTH_ON,BOTH_OFF,RED_OFF_GREEN_ON,GREEN_OFF_RED_ON};
Pattern currentPatternState=INITIAL_OFF;
enum Detector {WAITING_FOR_CAR,FIRST_BEAM_HIGH,BOTH_BEAMS_HIGH,CAR_DETECTED};
Detector currentCarDetectState=WAITING_FOR_CAR;
bool carStuckAlarmActive=false,stableFirstBeamState=false,stableSecondBeamState=false;
const int firstBeamPin=33,secondBeamPin=32,redArchPin=25,greenArchPin=26;
int pins[40]{};int writes=0;
int digitalRead(int pin){return pins[pin];}
void digitalWrite(int pin,int value){pins[pin]=value;++writes;}
''')
            (p/'test.cpp').write_text('#include "'+str(ROOT/'src/services/ArchEffects.cpp')+'"\n'+'''
int main(){
 ticks=29999;serviceIdleArchPattern();assert(writes==0);
 ticks=30000;serviceIdleArchPattern();assert(writes==2);
 ticks+=500;serviceIdleArchPattern();assert(pins[redArchPin]==LOW && pins[greenArchPin]==HIGH);
 // Raw/unqualified occupancy and alarms suspend idle for the full stop.
 occupied=true;pins[firstBeamPin]=HIGH;stableSecondBeamState=true;carStuckAlarmActive=true;
 auto n=writes;ticks+=600000;serviceIdleArchPattern();assert(writes==n && noCarTimer==ticks);
 ticks+=500;serviceIdleArchPattern();assert(writes==n && noCarTimer==ticks);
 occupied=false;pins[firstBeamPin]=LOW;stableSecondBeamState=false;carStuckAlarmActive=false;
 ticks+=29999;serviceIdleArchPattern();assert(writes==n);
 ticks+=1;serviceIdleArchPattern();assert(writes>n);

}
''')
            subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(p),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
    def test_loop_prioritizes_detection_without_show_gate(self):
        s=(ROOT/'src/main.cpp').read_text()
        self.assertIn('serviceIdleArchPattern();',s)
        self.assertLess(s.index('detectCar();'),s.index('serviceIdleArchPattern();'))
        self.assertNotIn('if (showTime && (millis() - noCarTimer)',s)
if __name__=='__main__':unittest.main()
