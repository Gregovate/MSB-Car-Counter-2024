#pragma once
#include "RuntimeState.h"

// Service interface; definitions are compiled once in src/services.
void KeepMqttAlive();
void publishTimingConfig();
void callback(char* topic, byte* payload, unsigned int length);
