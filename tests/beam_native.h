#pragma once
#include "traffic/BeamTimeline.h"
#include <vector>
traffic::BeamTimeline nativeTimeline;
struct NativeEdge {uint32_t ms;bool a,b;};
std::vector<NativeEdge> nativeCaptured;
uint32_t nativeLastEventMs=0;
void captureForLater(uint32_t ms,bool a,bool b){nativeCaptured.push_back({ms,a,b});}
using BeamSampleSink=void(*)(uint32_t,bool,bool);
void setCountEventMillis(uint32_t ms){nativeLastEventMs=ms;}
void setTrafficVehicleEvent(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t=0){}
void serviceBeamCapture(uint32_t debounceMs,BeamSampleSink sink) {
 for(const auto&e:nativeCaptured)nativeTimeline.edge(e.ms,e.a,e.b,debounceMs,sink);
 nativeCaptured.clear();
 nativeTimeline.edge(millis(),digitalRead(BEAM_TEST_A),digitalRead(BEAM_TEST_B),debounceMs,sink);
 nativeTimeline.finish(millis(),debounceMs,sink);
}
