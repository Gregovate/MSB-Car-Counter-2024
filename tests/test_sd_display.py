from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class SdDisplay(unittest.TestCase):
 def test_actual_display_retains_warning_and_counts(self):
  header=r'''#pragma once
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#define WHITE 1
#define BLACK 0
bool sdAvailable=false,ready=true;
bool countsReady(){return ready;}
bool trafficJournalDurable(){return false;}
const char* trafficJournalHealth(){return "OK";}
const char* beamCaptureHealth(){return "OK";}
struct DateTime {int hour(){return 17;}int minute(){return 0;}int second(){return 0;}int dayOfTheWeek(){return 1;}int month(){return 11;}int day(){return 23;}int year(){return 2026;}};
struct Rtc{DateTime now(){return {};}}rtc;
const char* days[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
const char* months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
int line1=0,line2=8,line3=16,line4=24,line5=32,daysRunning=2,totalShowCars=18,totalDailyCars=19,carCounterCars=22;
float tempF=34;
struct Display{
 std::vector<std::string> lines;
 void clearDisplay(){lines.clear();}void setTextColor(int){}void setTextSize(int){}void setCursor(int,int){}
 template<class...T>void printf(const char*f,T...){lines.push_back(f);}
 void println(const char*s){lines.push_back(s);}void display(){}void fillRect(int,int,int,int,int){}
 bool contains(const char*s){for(auto&l:lines)if(l.find(s)!=std::string::npos)return true;return false;}
}display;
'''
  harness=r'''
int main(){
 updateDisplay();assert(display.contains("SD FAULT") && display.contains("COUNTS MAY BE UNSAVED"));
 updateDisplay();assert(display.contains("SD FAULT")); // survives next normal redraw
 sdAvailable=true;updateDisplay();assert(!display.contains("SD FAULT"));
 ready=false;updateDisplay();assert(display.contains("COUNT RECOVERY"));
}
'''
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);(p/'traffic').mkdir();(p/'traffic/Firmware.h').write_text(header)
   (p/'test.cpp').write_text('#include "'+str(ROOT/'src/services/DisplayService.cpp')+'"\n'+harness)
   subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(p),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
