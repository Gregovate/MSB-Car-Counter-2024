"""Exercise actual timing publishers against a retained-command broker model."""
from pathlib import Path
import re, shutil, subprocess, tempfile, unittest
ROOT=Path(__file__).resolve().parents[1]
def function(source,name):
    return re.search(r'void '+name+r'\([^\n]*\) \{.*?^\}',source,re.S|re.M).group(0)
class ConfigurationOwnership(unittest.TestCase):
    def test_actual_publishers_preserve_commands(self):
        compiler=shutil.which('g++')
        if not compiler:self.skipTest('g++ required')
        mqtt=(ROOT/'src/services/MqttService.cpp').read_text(encoding='utf-8')
        transport=(ROOT/'src/services/MqttTransport.cpp').read_text(encoding='utf-8')
        topics=(ROOT/'include/traffic/MqttTopics.h').read_text(encoding='utf-8')
        defines='\n'.join(line for line in topics.splitlines() if line.startswith('#define MQTT_'))
        harness=r'''
#include <string>
#include <map>
#include <cassert>
class String {
 std::string s;
 public:
 String(const char* v):s(v){} String(unsigned long v):s(std::to_string(v)){}
 int indexOf(const char* v)const {auto p=s.find(v);return p==std::string::npos?-1:int(p);}
 void replace(const char* from,const char* to){size_t p=0;while((p=s.find(from,p))!=std::string::npos){s.replace(p,std::string(from).size(),to);p+=std::string(to).size();}}
 const char* c_str()const{return s.c_str();}
};
struct Client {bool online=true;bool connected(){return online;}} mqtt_client;
bool mqttOnline(){return mqtt_client.connected();}

std::map<std::string,std::string> retained;
unsigned publishes=0;
void publishMQTT(const char* t,const String& v,bool retain){assert(retain);retained[t]=v.c_str();++publishes;}
unsigned long carCounterTimeout=60000,waitDuration=950,gateCounterTimeout=60000,carDetectMS=1000;
'''+defines+'\n'+function(transport,'publishConfigState')+'\n'+function(mqtt,'publishTimingConfig')
        car='MQTT_SUB_TOPIC5' in function(mqtt,'publishTimingConfig')
        name='CarCounter' if car else 'GateCounter'
        timeout='carCounterTimeout' if car else 'gateCounterTimeout'
        detector='beamWaitDuration' if car else 'carDetectMS'
        variable='waitDuration' if car else 'carDetectMS'
        harness+=f'''
int main(){{
 const std::string base="msb/traffic/{name}/";
 const auto command=base+"Config/{timeout}";
 const auto detect=base+"Config/{detector}";
 retained[command]="45000";retained[detect]="500";
 publishTimingConfig();
 assert(retained[command]=="45000" && retained[detect]=="500");
 assert(retained[base+"State/{timeout}"]=="60000");
 assert(retained[base+"State/{detector}"]=="{950 if car else 1000}");
 mqtt_client.online=false;unsigned before=publishes;publishTimingConfig();assert(publishes==before);
 {timeout}=45000;{variable}=500;
 mqtt_client.online=true;publishTimingConfig();
 assert(retained[base+"State/{timeout}"]=="45000");
 assert(retained[base+"State/{detector}"]=="500");
 assert(retained[command]=="45000" && retained[detect]=="500");
 before=publishes;publishConfigState("invalid/topic",String(12UL));assert(publishes==before);
}}
'''
        with tempfile.TemporaryDirectory() as tmp:
            cpp=Path(tmp)/'config.cpp';cpp.write_text(harness)
            exe=str(Path(tmp)/'config')
            subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror',str(cpp),'-o',exe],check=True)
            subprocess.run([exe],check=True)
        self.assertIn('publishTimingConfig();',function(mqtt,'KeepMqttAlive'))
if __name__=='__main__':unittest.main()
