#pragma once
#include <stdint.h>
// Only the GPIO ISR touches the raw producer. Detector/count/storage code runs
// in the foreground with original edge timing reconstructed by BeamTimeline.
using BeamSampleSink=void(*)(uint32_t,bool,bool);
void initializeBeamCapture(int aPin,int bPin);
void serviceBeamCapture(uint32_t debounceMs,BeamSampleSink sink);
uint32_t beamCaptureDropped();
uint32_t beamCaptureMaxGapMs();
void publishBeamCaptureHealth();
void resetBeamDetectorAfterLoss();
void setCountEventMillis(uint32_t eventMs);
void clearCountEventTime();
DateTime countEventTime();
bool countEventClockValid();

const char* beamCaptureHealth();
