#pragma once
#include <math.h>
namespace traffic {
// Some DHT22 variants send negative tenths-C in two's complement, while
// Adafruit decodes sign/magnitude. Recover only the disjoint impossible-value
// band mapping to -40..-0.1 C; leave ordinary readings and other faults alone.
// The library has already checked the packet checksum. This cannot prove a
// sensor's physical accuracy; expose the applied encoding in diagnostics.
inline float compatibleDht22F(float decodedF,bool& corrected) {
    corrected=false;
    if(!isfinite(decodedF)) return decodedF;
    const float magnitude=(32.0f-decodedF)/0.18f;
    if(magnitude<32368.0f-0.02f || magnitude>32767.0f+0.02f) return decodedF;
    const long tenths=lroundf(magnitude);
    if(fabsf(magnitude-static_cast<float>(tenths))>0.02f) return decodedF;
    corrected=true;
    return static_cast<float>(tenths-32768)*0.18f+32.0f;
}
}
