from pathlib import Path
import tempfile,subprocess,unittest
ROOT=Path(__file__).resolve().parents[1]
class NetworkConnection(unittest.TestCase):
    def test_real_handshake_ownership(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);(p/'traffic').mkdir();(p/'freertos').mkdir()
            (p/'traffic/Firmware.h').write_text('''#pragma once
#include <cassert>
struct Client {bool online=false;int calls=0;bool connect(const char*,const char*,const char*){++calls;online=true;return true;}bool connected(){return online;}};
inline Client mqtt_client;
inline const char* THIS_MQTT_CLIENT="test";
''')
            (p/'freertos/FreeRTOS.h').write_text('#pragma once\n#define pdPASS 1\n')
            (p/'freertos/task.h').write_text('''#pragma once
inline void (*pendingTask)(void*)=nullptr;
inline bool createOk=true;
inline int xTaskCreate(void(*fn)(void*),const char*,int,void*,int,void*){if(!createOk)return 0;pendingTask=fn;return 1;}
inline void vTaskDelete(void*){}
''')
            source='#include "'+str(ROOT/'src/services/MqttConnection.cpp')+'"\n'+'''
int main(){
 assert(!mqttOnline());assert(!pollMqttConnection("user","pass"));
 assert(mqttConnectionBusy() && !mqttOnline() && mqtt_client.calls==0);
 assert(!pollMqttConnection("other","other") && mqtt_client.calls==0);
 pendingTask(nullptr);assert(!mqttConnectionBusy() && !mqttOnline());
 assert(pollMqttConnection("user","pass"));assert(mqttOnline() && mqtt_client.calls==1);
 mqtt_client.online=false;assert(!mqttOnline());createOk=false;
 assert(!pollMqttConnection("user","pass"));assert(!mqttConnectionBusy());
}
'''
            (p/'test.cpp').write_text(source)
            subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(p),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
    def test_no_foreground_connection_or_scan_wait(self):
        net=(ROOT/'src/services/NetworkService.cpp').read_text()
        for forbidden in ['wifiMulti.run(', 'scanNetworks(', 'while(wifi', 'mqtt_client.connect(']:self.assertNotIn(forbidden,net)
        self.assertIn('mqttConnectionBusy()',net)
        main=(ROOT/'src/main.cpp').read_text()
        self.assertNotIn('scanNetworks(',main)
        self.assertIn('configTzTime("CST6CDT,M3.2.0,M11.1.0"',main)
    def test_private_ap_six_is_attempted_and_half_pair_rejected(self):
        import re
        source=(ROOT/'src/services/NetworkService.cpp').read_text()
        functions='\n'.join(re.search(r'void '+name+r'\(\) \{.*?^\}',source,re.S|re.M).group(0) for name in ['configureWifiAccessPoints','configureAdditionalWifiAccessPoints','setup_wifi'])
        for count in [5,6]:
            macros='\n'.join('#define secret_ssid_AP_%d "ap%d"\n#define secret_pass_AP_%d "pw%d"'%(i,i,i,i) for i in range(1,count+1))
            harness=r'''
#include <vector>
#include <string>
#include <cassert>
#define WL_CONNECTED 1
unsigned long ticks=0;
unsigned long millis(){return ticks;}
std::vector<std::string> attempted,registered;
struct Multi {void addAP(const char* s,const char*){registered.push_back(s);}} wifiMulti;
struct Wifi {bool online=false;int status(){return online ? 1:0;}void begin(const char* s,const char*){attempted.push_back(s);}} WiFi;
'''+macros+'\n'+functions+f'''
int main(){{
 configureWifiAccessPoints();configureAdditionalWifiAccessPoints();assert(registered.size()=={count});
 setup_wifi();assert(attempted.size()==1);ticks=1000;setup_wifi();assert(attempted.size()==1);
 for(int i=1;i<{count};++i){{ticks+=15000;setup_wifi();assert(attempted.back()=="ap"+std::to_string(i+1));}}
 ticks+=15000;setup_wifi();assert(attempted.back()=="ap1");
 auto n=attempted.size();WiFi.online=true;ticks+=15000;setup_wifi();assert(attempted.size()==n);
}}
'''
            with tempfile.TemporaryDirectory() as tmp:
                p=Path(tmp);(p/'ap.cpp').write_text(harness)
                subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(p/'ap.cpp'),'-o',str(p/'ap')],check=True)
                subprocess.run([str(p/'ap')],check=True)
                if count==5:
                    (p/'partial.cpp').write_text('#define secret_ssid_AP_6 "partial"\n'+harness)
                    result=subprocess.run(['g++','-std=c++17','-c',str(p/'partial.cpp'),'-o',str(p/'partial.o')],capture_output=True,text=True)
                    self.assertNotEqual(result.returncode,0);self.assertIn('AP 6 requires both',result.stderr)
        self.assertIn('configureAdditionalWifiAccessPoints();',(ROOT/'src/main.cpp').read_text())

if __name__=='__main__':unittest.main()
