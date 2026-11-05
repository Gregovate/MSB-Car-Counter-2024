#include "traffic/Firmware.h"
#include "secrets.h"

// Extracted from the production baseline; behavior fixes are separate changes.
// Register the same five configured APs at the original setup position.
void configureWifiAccessPoints() {
    wifiMulti.addAP(secret_ssid_AP_1, secret_pass_AP_1);
    wifiMulti.addAP(secret_ssid_AP_2, secret_pass_AP_2);
    wifiMulti.addAP(secret_ssid_AP_3, secret_pass_AP_3);
    wifiMulti.addAP(secret_ssid_AP_4, secret_pass_AP_4);
    wifiMulti.addAP(secret_ssid_AP_5, secret_pass_AP_5);
}

void configureAdditionalWifiAccessPoints() {
    // The sixth entry is private, like the original five. A missing half-pair
    // is a compile error rather than silently disabling the requested AP.
#if defined(secret_ssid_AP_6) && defined(secret_pass_AP_6)
    wifiMulti.addAP(secret_ssid_AP_6,secret_pass_AP_6);
#elif defined(secret_ssid_AP_6) || defined(secret_pass_AP_6)
#error "AP 6 requires both secret_ssid_AP_6 and secret_pass_AP_6"
#endif
}

void setup_wifi() {
    static unsigned long lastAttempt=0;
    static unsigned int ap=0;
    static bool started=false;
    if (WiFi.status()==WL_CONNECTED) return;
    if (started && millis()-lastAttempt<15000UL) return;
    started=true;lastAttempt=millis();
    const char* ssids[]={secret_ssid_AP_1,secret_ssid_AP_2,secret_ssid_AP_3,secret_ssid_AP_4,secret_ssid_AP_5
#if defined(secret_ssid_AP_6) && defined(secret_pass_AP_6)
        ,secret_ssid_AP_6
#endif
    };
    const char* passwords[]={secret_pass_AP_1,secret_pass_AP_2,secret_pass_AP_3,secret_pass_AP_4,secret_pass_AP_5
#if defined(secret_ssid_AP_6) && defined(secret_pass_AP_6)
        ,secret_pass_AP_6
#endif
    };
    WiFi.begin(ssids[ap],passwords[ap]);
    ap=(ap+1)%(sizeof(ssids)/sizeof(ssids[0]));
}

void preferPrimaryIfAvailable() {
    // Keep a working AP. A foreground scan/handoff can miss a vehicle.
}

void MQTTreconnect() {
    if (WiFi.status()!=WL_CONNECTED || mqttConnectionBusy()) return;
    mqtt_client.setBufferSize(768);
    mqtt_client.setSocketTimeout(1);

    static unsigned long lastReconnectAttempt = 0; // Tracks the last reconnect attempt time
    const unsigned long reconnectInterval = 5000; // Time between reconnect attempts (5 seconds)

    // If the client is already connected, do nothing
    if (mqttOnline()) {
        return;
    }

    // Check if enough time has passed since the last attempt
    if (millis() - lastReconnectAttempt < reconnectInterval) {
        return;
    }

    lastReconnectAttempt = millis(); // Update the last attempt time
    Serial.println("Attempting MQTT connection...");

    // Your existing single-server config
    mqtt_client.setServer(mqtt_Server, mqtt_Port);
    mqtt_client.setCallback(callback);  // required to receive messages

    // Create a unique client ID
    // Client identity is supplied by the shared handshake worker.

    Serial.printf("Trying MQTT server: %s:%d\n", mqtt_Server, mqtt_Port);

    if (pollMqttConnection(mqtt_UserName, mqtt_Password)) {

        Serial.printf("Connected to MQTT server: %s\n", mqtt_Server);

        // Display connection status
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0,line5);
        display.println("MQTT Connect");
        display.display();

        Serial.println("connected!");
        Serial.println("Waiting for Car");

        // Once connected, publish retained online status (JSON for HA)
        String helloPayload =
            String("{\"device\":\"espCarCounter\",\"status\":\"online\",\"fw\":\"") +
            FWVersion + "\",\"boot\":\"" + bootTimestamp +
            "\",\"msg\":\"Car Counter ONLINE\"}";

        publishMQTT(MQTT_PUB_HELLO, helloPayload, true);

        // GAL 25-11-22: publish temp/RH as JSON (match HA templates)
        char jsonPayload[100];
        snprintf(jsonPayload, sizeof(jsonPayload),
                "{\"tempF\": %.1f, \"humidity\": %.1f}", tempF, humidity);
        publishMQTT(MQTT_PUB_TEMP, String(jsonPayload), true);
        publishMQTT(MQTT_PUB_ENTER_CARS, String(totalDailyCars), true);
        publishMQTT(MQTT_PUB_SHOWTOTAL, String(totalShowCars), true);

        // WiFi health immediately on connect (retained)
        publishMQTT(MQTT_PUB_RSSI, String(WiFi.RSSI()), true);
        publishMQTT(MQTT_PUB_SSID, String(WiFi.SSID()), true);
        publishMQTT(MQTT_PUB_IP,   WiFi.localIP().toString(), true);

        // GAL 25-11-22: retained online snapshot for remote debugging
        publishDebugEvent(
            "online",
            "ssid=" + WiFi.SSID() +
            " rssi=" + String(WiFi.RSSI()) +
            " ip=" + WiFi.localIP().toString(),
            true
        );

        // GAL 25-11-18: announce firmware version
        // GAL 25-11-22.4: firmware publish retained for dashboards
        publishMQTT(MQTT_PUB_FW_VERSION, String(FWVersion), true); // ok non-retained

        // Subscribe to necessary topics
        mqtt_client.subscribe(MQTT_PUB_HELLO);
        mqtt_client.subscribe(MQTT_SUB_TOPIC1); // Reset Daily Count
        mqtt_client.subscribe(MQTT_SUB_TOPIC2); // Reset Show Count
        mqtt_client.subscribe(MQTT_SUB_TOPIC3); // Reset Day of Month
        mqtt_client.subscribe(MQTT_SUB_TOPIC4); // Reset Days Running
        mqtt_client.subscribe(MQTT_SUB_TOPIC5); // Update Car Counter Timeout
        mqtt_client.subscribe(MQTT_SUB_TOPIC6); // Update Sensor Wait Duration
        mqtt_client.subscribe(MQTT_SUB_SHOWSTART); // Set Show Start Time
        mqtt_client.subscribe(MQTT_SUB_SHOWSTARTTIME); // Set Show Start Time Detailed
        mqtt_client.subscribe(MQTT_SUB_SHOWENDTIME); // Set Show End Time Detailed

        mqtt_client.subscribe((String(topic_base_path)+"/Config/operatingSettings").c_str());
                mqtt_client.subscribe((String(topic_base_path)+"/Config/countCorrection").c_str());
                mqtt_client.subscribe((String(topic_base_path)+"/Config/showStartDate").c_str());
                KeepMqttAlive();

                // Log subscriptions
        Serial.println("Subscribed to MQTT topics.");
        publishDebugLog("MQTT connected and topics subscribed.");

        // Flush queued messages now that we're back online
        while (!publishQueue.empty()) publishQueue.pop();
        KeepMqttAlive();
        publishRecordingCalendar();

        return;

    } else {
        if (mqttConnectionBusy()) return;
        Serial.printf("Failed to connect to MQTT server: %s (rc=%d)\n",
                      mqtt_Server, mqtt_client.state());

        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0, line6);
        display.println("MQTT Error");
        display.display();
    }
}

void checkWiFiConnection() {
    if (WiFi.status()!=WL_CONNECTED) { setup_wifi();return; }
    if (!mqttOnline()) MQTTreconnect();
    else mqtt_client.loop();
}

