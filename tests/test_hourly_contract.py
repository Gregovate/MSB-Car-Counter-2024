from pathlib import Path
import re,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
def function(source,name):
    return re.search(r'void '+name+r'\([^\n]*\) \{.*?^\}',source,re.S|re.M).group(0)
class HourlyContract(unittest.TestCase):
    def test_actual_snapshot(self):
        compiler=shutil.which('g++')
        if not compiler:self.skipTest('g++ required')
        source=(ROOT/'src/services/MqttTransport.cpp').read_text()
        topics=(ROOT/'include/traffic/MqttTopics.h').read_text()
        definitions='\n'.join(x for x in topics.splitlines() if x.startswith('#define MQTT_'))
        harness=r'''
#include <string>
#include <map>
#include <cassert>
#include <cstdio>
#include <climits>
struct String:std::string {using std::string::string;String(int n):std::string(std::to_string(n)){};};
struct Client {bool online=true,allocation=true;unsigned size=256;bool connected(){return online;}unsigned getBufferSize(){return size;}bool setBufferSize(unsigned n){if(!allocation)return false;size=n;return true;}} mqtt_client;
bool restored=true;bool countsReady(){return restored;}
int hourlyCount[24]={};
std::map<std::string,std::string> retained;
unsigned errors=0;
void publishDebugLog(const char*){++errors;}
void publishMQTT(const char* topic,const String& value,bool retain){assert(retain);assert(std::string(topic).find("//")==std::string::npos);assert(std::string(topic).size()+value.size()+7<=mqtt_client.size);retained[topic]=value;}
'''+definitions+'\n'+function(source,'publishHourlySnapshot')+r'''
int main(){
 for(int i=0;i<24;++i)hourlyCount[i]=i*13;
 publishHourlySnapshot();assert(retained.size()==25);
 std::string expected="[";
 for(int i=0;i<24;++i){if(i)expected+=",";expected+=std::to_string(i*13);char t[96];snprintf(t,sizeof(t),"%s%02d",MQTT_PUB_CARS_HOURLY,i);assert(retained[t]==std::to_string(i*13));}expected+="]";
 assert(retained[MQTT_PUB_HOURLY_JSON]==expected);
 retained.clear();mqtt_client.online=false;publishHourlySnapshot();assert(retained.empty());
 mqtt_client.online=true;restored=false;publishHourlySnapshot();assert(retained.empty());
 restored=true;mqtt_client.size=256;mqtt_client.allocation=false;publishHourlySnapshot();assert(retained.empty()&&errors==1);
 mqtt_client.allocation=true;for(auto& n:hourlyCount)n=INT_MAX;
 publishHourlySnapshot();assert(retained.size()==25);
 expected="[";for(int i=0;i<24;++i){if(i)expected+=",";expected+=std::to_string(INT_MAX);}expected+="]";
 assert(retained[MQTT_PUB_HOURLY_JSON]==expected);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            cpp=Path(tmp)/'hourly.cpp';cpp.write_text(harness);exe=str(Path(tmp)/'hourly')
            subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror',str(cpp),'-o',exe],check=True)
            subprocess.run([exe],check=True)
    def test_main_loop_services_idle_recovery_before_consumers(self):
        loop=function((ROOT/'src/main.cpp').read_text(),'loop')
        start=loop.index('updateRecordingCalendar();')
        service=loop.index('serviceCountState();')
        self.assertLess(start,service)
        for call in ['KeepMqttAlive();','timeTriggeredEvents();','detectCar();']:
            self.assertLess(service,loop.index(call))
        mqtt=(ROOT/'src/services/MqttService.cpp').read_text()
        self.assertIn('publishHourlySnapshot();',function(mqtt,'KeepMqttAlive'))
        for p in (ROOT/'src').rglob('*.cpp'):
            self.assertNotIn('"%s/%02d", MQTT_PUB_CARS_HOURLY',p.read_text())
if __name__=='__main__':unittest.main()
