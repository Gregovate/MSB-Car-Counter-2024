#include "traffic/Firmware.h"
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
// Only the connection handshake runs on this task. The normal loop owns all
// publishing, subscriptions and callbacks after it completes. While busy,
// callers must not touch PubSubClient, whose API is not thread-safe.
std::atomic<int> connectionPhase{-2}; // idle, busy(-1), failed(0), connected(1), ready(2)
const char* connectUser=nullptr;
const char* connectPassword=nullptr;
void mqttHandshake(void*) {
    bool ok=mqtt_client.connect(THIS_MQTT_CLIENT,connectUser,connectPassword);
    connectionPhase.store(ok ? 1:0,std::memory_order_release);
    vTaskDelete(nullptr);
}
}

bool mqttConnectionBusy() { return connectionPhase.load(std::memory_order_acquire)==-1; }
bool mqttOnline() {
    return connectionPhase.load(std::memory_order_acquire)==2 && mqtt_client.connected();
}
bool pollMqttConnection(const char* user,const char* password) {
    int phase=connectionPhase.load(std::memory_order_acquire);
    if (phase==-1) return false;
    if (phase==1) { connectionPhase.store(2);return true; }
    if (phase==0) { connectionPhase.store(-2);return false; }
    connectUser=user;connectPassword=password;
    connectionPhase.store(-1);
    if (xTaskCreate(mqttHandshake,"mqtt-connect",4096,nullptr,1,nullptr)!=pdPASS)
        connectionPhase.store(0);
    return false;
}
