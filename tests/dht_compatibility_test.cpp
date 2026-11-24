#include "traffic/DhtCompatibility.h"
#include <cassert>
#include <cmath>
#include <initializer_list>
int main() {
    bool changed=false;
    assert(fabs(traffic::compatibleDht22F(-5860.66f,changed)-26.42f)<.01f && changed);
    assert(fabs(traffic::compatibleDht22F(-5866.06f,changed)-31.82f)<.01f && changed);
    // Exhaust both legal encodings over the full sensor temperature range.
    for(int c=-400;c<=800;++c) {
        float expected=c*.18f+32;
        assert(fabs(traffic::compatibleDht22F(expected,changed)-expected)<.01f && !changed);
        if(c<0) {
            float decoded=32-(32768+c)*.18f;
            assert(fabs(traffic::compatibleDht22F(decoded,changed)-expected)<.01f && changed);
        }
    }
    for(float v:{-999.f,-100.f,150.f,-6000.f,-5866.24f}) {
        assert(traffic::compatibleDht22F(v,changed)==v && !changed);
    }
    assert(std::isnan(traffic::compatibleDht22F(NAN,changed)) && !changed);
}
