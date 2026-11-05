#pragma once
#include "RuntimeState.h"

// Common MQTT transport/diagnostic interface.
String encodeQueuedMessage(const char *topic, const String &msg, bool retainFlag);
void publishMQTT(const char *topic, const String &message, bool retainFlag);
void publishMQTT(const char *topic, const String &message);
void publishQueuedMessages(size_t maxToFlush = MAX_FLUSH_PER_CALL);
void publishDebugLog(const String &message);
void publishDebugEvent(const char* event, const String& details, bool retainFlag = false);
void publishConfigState(const char* commandTopic, const String& value);
void publishHourlySnapshot();
