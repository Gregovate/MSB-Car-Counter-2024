// credentials.h
// Store sensitive information such as WiFi credentials and MQTT configuration here.

#ifndef CREDENTIALS_H
#define CREDENTIALS_H

// WiFi Access Points
const char* secret_ssid_AP_1 = "YourSSID_1";
const char* secret_pass_AP_1 = "YourPassword_1";

const char* secret_ssid_AP_2 = "YourSSID_2";
const char* secret_pass_AP_2 = "YourPassword_2";

const char* secret_ssid_AP_3 = "YourSSID_3";
const char* secret_pass_AP_3 = "YourPassword_3";

const char* secret_ssid_AP_4 = "YourSSID_4";
const char* secret_pass_AP_4 = "YourPassword_4";

const char* secret_ssid_AP_5 = "YourSSID_5";
const char* secret_pass_AP_5 = "YourPassword_5";

// MQTT Configuration
struct MQTTConfig {
    const char* server;
    int port;
    const char* username;
    const char* password;
};

MQTTConfig mqtt_configs[] = {
    {"mqtt1.example.com", 1883, "user1", "password1"},
    {"mqtt2.example.com", 1883, "user2", "password2"},
    {"mqtt3.example.com", 1883, "user3", "password3"},
    {"mqtt4.example.com", 1883, "user4", "password4"},
    {"mqtt5.example.com", 1883, "user5", "password5"}
};

const int mqtt_servers_count = sizeof(mqtt_configs) / sizeof(MQTTConfig);

#endif // CREDENTIALS_H
