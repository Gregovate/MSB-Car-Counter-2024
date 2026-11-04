#pragma once
#include "RuntimeState.h"

// Service interface; definitions are compiled once in src/services.
void setup_wifi();
void preferPrimaryIfAvailable();
void MQTTreconnect();
void checkWiFiConnection();
void configureWifiAccessPoints();
