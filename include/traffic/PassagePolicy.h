#pragma once
#include <stdint.h>
#include <string.h>
namespace traffic {
// Pure sampled-beam policy. Durations use debounced levels and wrap-safe millis.
// A transaction stays locked until both beams clear; alarms never expire a car.
struct PassagePolicy {
    bool armed=false,active=false,started=false,finished=false,accepted=false;
    bool qualified=false,completed=false;
    bool a=false,b=false,sawB=false,overlap=false;
    uint32_t start=0,last=0,aMs=0,bMs=0,overlapMs=0,overlapRun=0,maxOverlap=0,followMs=0;
    uint32_t beamMinimum=100,overlapMinimum=500;
    const char* reason="IDLE";
    const char* order="NONE";
    void update(bool nextA,bool nextB,uint32_t now,uint32_t beamMin,uint32_t detectMin) {
        started=finished=false;
        if(!armed) {if(!nextA && !nextB) armed=true;return;}
        if(!active) {
            if(!nextA && !nextB) return;
            active=started=true;accepted=qualified=completed=false;start=last=now;
            a=nextA;b=nextB;sawB=nextB;overlap=nextA && nextB;
            aMs=bMs=overlapMs=overlapRun=maxOverlap=followMs=0;
            beamMinimum=beamMin;overlapMinimum=detectMin;
            order=nextA && !nextB ? "A_THEN_B" : nextB && !nextA ? "B_FIRST":"SIMULTANEOUS";
            reason=nextA && !nextB ? "PENDING" : nextB && !nextA ? "REVERSE_B_FIRST":"AMBIGUOUS_START";
            return;
        }
        // B-clear decides once; BOTH-clear is required only for rearming.
        if(completed) {a=nextA;b=nextB;if(!a && !b) active=false;return;}
        uint32_t elapsed=now-last;last=now;
        if(a) aMs+=elapsed;
        if(b) bMs+=elapsed;
        if(a && b) {overlapMs+=elapsed;overlapRun+=elapsed;if(overlapRun>maxOverlap) maxOverlap=overlapRun;}
        else overlapRun=0;
        if(strcmp(reason,"PENDING")==0) {
            if(!b && nextB) {
                if(sawB || !a || !nextA) reason="AMBIGUOUS_SEQUENCE";
                else {sawB=true;followMs=now-start;overlap=true;}
            }
            if(a && !nextA && !sawB) reason="A_ONLY";
            if(!a && nextA) reason="AMBIGUOUS_REBREAK";

        }
        bool bCleared=b && !nextB;
        if(strcmp(reason,"PENDING")==0 && overlap && aMs>=beamMinimum &&
            bMs>=beamMinimum && maxOverlap>=overlapMinimum) qualified=true;
        a=nextA;b=nextB;
        if((a || b) && (!bCleared || strcmp(reason,"PENDING")!=0)) return;
        completed=finished=true;
        if(!a && !b) active=false;
        if(strcmp(reason,"PENDING")!=0) return;
        if(!overlap) reason="NO_OVERLAP";
        else if(aMs<beamMinimum) reason="SHORT_BEAM_A";
        else if(bMs<beamMinimum) reason="SHORT_BEAM_B";
        else if(maxOverlap<overlapMinimum) reason="SHORT_OVERLAP";
        else {reason="ACCEPTED_FORWARD";accepted=true;}
    }
};
}
