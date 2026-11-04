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

void setup_wifi() {
    Serial.println("Connecting to WiFi");
    display.println("Connecting to WiFi..");
    display.display();

    WiFi.mode(WIFI_STA);

    // ----- Step 1: Scan once to see PRIMARY strength -----
    int n = WiFi.scanNetworks(false, true);
    int primaryRSSI = -127;
    bool primarySeen = false;

    for (int i = 0; i < n; i++) {
        if (WiFi.SSID(i) == secret_ssid_AP_1) {
            primaryRSSI = WiFi.RSSI(i);
            primarySeen = true;
            break;
        }
    }

    // ----- Step 2: If PRIMARY is seen and decent, connect to it directly -----
    if (primarySeen && primaryRSSI > -75) {
        WiFi.begin(secret_ssid_AP_1, secret_pass_AP_1);

        unsigned long startAttempt = millis();
        const unsigned long primaryTimeout = 8000;
        while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt) < primaryTimeout) {
            delay(200);
        }
    }

    // ----- Step 3: If not connected, fall back to WiFiMulti (PRIMARY+BACKUP) -----
    if (WiFi.status() != WL_CONNECTED) {
        while (wifiMulti.run(connectTimeOutPerAP) != WL_CONNECTED) {
            Serial.print(".");
            delay(200);
        }
    }

    // ----- Display / log final connection -----
    Serial.println("Connected to the WiFi network");
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.display();

    display.setCursor(0, line1);
    display.print("SSID: ");
    display.println(WiFi.SSID());
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    IPAddress ip = WiFi.localIP();
    Serial.print("IP: ");
    Serial.println(ip);
    display.setCursor(0, line2);
    display.print("IP: ");
    display.println(ip);

    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
    display.setCursor(0, line3);
    display.print("signal: ");
    display.print(rssi);
    display.println(" dBm");

    display.display();
    delay(1000);
}

void preferPrimaryIfAvailable() {
if (WiFi.status() != WL_CONNECTED) return;
    if (WiFi.SSID() == secret_ssid_AP_1) return;   // already on PRIMARY (MSB-CARCOUNTER)

    // Scan for PRIMARY strength without wiping WiFi state
    int n = WiFi.scanNetworks(false, true);
        for (int i = 0; i < n; i++) {
            if (WiFi.SSID(i) == secret_ssid_AP_1 && WiFi.RSSI(i) > -75) {
                // Primary is detected AND strong → switch back
                WiFi.disconnect(true);     // clear current connection
                delay(200);
                WiFi.begin(secret_ssid_AP_1, secret_pass_AP_1);
                return;
            }
        }
}

void MQTTreconnect() {
    static unsigned long lastReconnectAttempt = 0; // Tracks the last reconnect attempt time
    const unsigned long reconnectInterval = 5000; // Time between reconnect attempts (5 seconds)

    // If the client is already connected, do nothing
    if (mqtt_client.connected()) {
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
    String clientId = THIS_MQTT_CLIENT;

    Serial.printf("Trying MQTT server: %s:%d\n", mqtt_Server, mqtt_Port);

    if (mqtt_client.connect(clientId.c_str(), mqtt_UserName, mqtt_Password)) {

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

        // Log subscriptions
        Serial.println("Subscribed to MQTT topics.");
        publishDebugLog("MQTT connected and topics subscribed.");

        // Flush queued messages now that we're back online
        publishQueuedMessages();

        return;

    } else {
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

/* non-blocking WiFi and MQTT Connectivity Checks 
    First check if WiFi is connected */
    if (wifiMulti.run() == WL_CONNECTED) {
        /* If MQTT is not connected then Attempt MQTT Connection */
        if (!mqtt_client.connected()) {
            Serial.print("hour = ");
            Serial.println(currentHr12);
            Serial.println("Attempting MQTT Connection");
            MQTTreconnect();
            start_MqttMillis = millis();
        } else {
                //keep MQTT client connected when WiFi is connected
                mqtt_client.loop();
        }
    } else {
        // If WiFi if lost, then attemp non blocking WiFi Connection
        if ((millis() - start_WiFiMillis) > wifi_connectioncheckMillis) {
            setup_wifi();
            start_WiFiMillis = millis();
        }
    }    
}
