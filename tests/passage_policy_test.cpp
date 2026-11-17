#include "traffic/PassagePolicy.h"
#include <cassert>
#include <cstring>
#include <iostream>
using traffic::PassagePolicy;
void update(PassagePolicy& p,bool a,bool b,uint32_t t){p.update(a,b,t,100,500);}
void forward(PassagePolicy& p,uint32_t t) {
 update(p,false,false,t);update(p,true,false,t+100);
 update(p,true,true,t+300);update(p,false,true,t+1000);update(p,false,false,t+1200);
 assert(p.finished && p.accepted && p.aMs==900 && p.bMs==900 && p.maxOverlap==700);
}
int main(){
 PassagePolicy p;forward(p,0);forward(p,6000); // six-second arrivals, no cooldown
 for(int i=0;i<2;++i){uint32_t t=12000+i*6000;
  update(p,false,true,t);update(p,true,true,t+200);update(p,true,false,t+1000);
  update(p,false,false,t+1300);assert(p.finished && !p.accepted);
  assert(!strcmp(p.reason,"REVERSE_B_FIRST"));
 }
 forward(p,24000);
 // One and two non-overlapping walkers never create one combined vehicle.
 for(int i=0;i<2;++i){uint32_t t=30000+i*2000;
  update(p,true,false,t);update(p,false,false,t+200);assert(!p.accepted);
  update(p,false,true,t+500);update(p,false,false,t+700);assert(!p.accepted);
 }
 update(p,true,false,35000);update(p,true,true,35200);update(p,false,true,35400);
 update(p,false,false,35600);assert(!p.accepted && !strcmp(p.reason,"SHORT_OVERLAP"));
 // Long A alone must not satisfy the overlap requirement.
 update(p,true,false,40000);update(p,true,true,50000);update(p,false,true,50100);
 update(p,false,false,51000);assert(!p.accepted && !strcmp(p.reason,"SHORT_OVERLAP"));
 // Hold longer than alarm timeout: still pending, then one final acceptance.
 update(p,true,false,60000);update(p,true,true,60200);update(p,true,true,180000);
 assert(p.active && !p.finished);update(p,false,true,180100);update(p,false,false,180300);
 assert(p.finished && p.accepted);update(p,false,false,180400);assert(!p.finished);
 update(p,true,true,190000);update(p,false,false,191000);assert(!p.accepted);
 // Startup with a blocked beam cannot count a partial event.
 PassagePolicy boot;update(boot,true,false,0);update(boot,true,true,1000);
 update(boot,false,false,2000);assert(!boot.finished);forward(boot,3000);
 // Millis rollover and threshold boundary.
 PassagePolicy wrap;forward(wrap,0xfffffd00U);
 PassagePolicy edge;update(edge,false,false,0);update(edge,true,false,100);
 update(edge,true,true,200);update(edge,false,true,700);update(edge,false,false,800);assert(edge.accepted);
 std::cout<<"PASS: forward/reverse pairs, walkers, overlap, six-second arrivals, long stops, rearming and millis wrap\n";
}
