#pragma once
#include <stdint.h>
namespace traffic {
// Replay raw levels using their capture times, not foreground drain times.
// A change must remain continuous for debounceMs before becoming stable.
class BeamTimeline {
    bool rawA=false,rawB=false,a=false,b=false,started=false;
    uint32_t changedA=0,changedB=0;
public:
    template<class Sink> void advance(uint32_t now,uint32_t debounceMs,Sink sink) {
        for(unsigned n=0;n<2;++n) {
            bool dueA=rawA!=a && uint32_t(now-changedA)>debounceMs;
            bool dueB=rawB!=b && uint32_t(now-changedB)>debounceMs;
            if(!dueA && !dueB)break;
            uint32_t ta=changedA+debounceMs+1,tb=changedB+debounceMs+1;
            uint32_t at=dueA && (!dueB || int32_t(ta-tb)<=0)?ta:tb;
            if(dueA && ta==at)a=rawA;
            if(dueB && tb==at)b=rawB;
            sink(at,a,b);
        }
    }
    template<class Sink> void edge(uint32_t now,bool nextA,bool nextB,uint32_t debounceMs,Sink sink) {
        if(!started){
            // Boot/overflow has no proven preceding clear interval. Expose the
            // actual occupancy so the detector waits for both-clear to arm.
            started=true;rawA=a=nextA;rawB=b=nextB;changedA=changedB=now;sink(now,a,b);return;
        }
        advance(now,debounceMs,sink);
        if(rawA!=nextA){rawA=nextA;changedA=now;}
        if(rawB!=nextB){rawB=nextB;changedB=now;}
    }
    template<class Sink> void finish(uint32_t now,uint32_t debounceMs,Sink sink) {
        advance(now,debounceMs,sink);sink(now,a,b);
    }
};
}
