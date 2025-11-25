#pragma once

struct HeartbeatFields {
  String boot;
  String now;
  int enter;
  int show;
  int exit;
  int inpark;
  int rssi;
};

// publish retained heartbeat json
inline void publishHeartbeat(const char* topic, const HeartbeatFields& hb) {
  String payload = "{";
  payload += "\"boot\":\"" + hb.boot + "\",";
  payload += "\"now\":\"" + hb.now + "\",";
  if (hb.enter >= 0)  payload += "\"enter\":"  + String(hb.enter)  + ",";
  if (hb.show >= 0)   payload += "\"show\":"   + String(hb.show)   + ",";
  if (hb.exit >= 0)   payload += "\"exit\":"   + String(hb.exit)   + ",";
  if (hb.inpark >= 0) payload += "\"inpark\":" + String(hb.inpark) + ",";
  payload += "\"rssi\":" + String(hb.rssi);
  payload += "}";

  publishMQTT(topic, payload, true);
}
