#include "traffic/Firmware.h"
#include "traffic/BeamTimeline.h"
#include <driver/gpio.h>
#include <soc/gpio_struct.h>
#include <esp_intr_alloc.h>
#include <esp_timer.h>

namespace {
// DRAM-only ISR state, power-of-two queue. No allocation, strings, flash tables,
// logging, I2C, SD, MQTT or count callbacks are reachable from this ISR.
struct Edge {uint32_t us,levels;};
constexpr uint32_t capacity=2048;
DRAM_ATTR Edge edges[capacity];
DRAM_ATTR volatile uint32_t head=0,tail=0,dropped=0;
DRAM_ATTR uint32_t maskA=0,maskB=0;
DRAM_ATTR bool highA=false,highB=false;
DRAM_ATTR portMUX_TYPE captureMux=portMUX_INITIALIZER_UNLOCKED;
traffic::BeamTimeline timeline;
bool initialized=false,captureOk=false,draining=false,eventActive=false;
uint32_t seenDropped=0,eventMillis=0,lastService=0,maxGap=0;
void IRAM_ATTR captureEdge(void*) {
    uint32_t lo=GPIO.in,hi=GPIO.in1.val;
    uint32_t levels=((highA?hi:lo)&maskA?1U:0U)|((highB?hi:lo)&maskB?2U:0U);
    uint32_t us=static_cast<uint32_t>(esp_timer_get_time());
    portENTER_CRITICAL_ISR(&captureMux);
    uint32_t next=(head+1)&(capacity-1);
    if(next==tail){if(dropped!=UINT32_MAX)++dropped;}
    else {edges[head].us=us;edges[head].levels=levels;head=next;}
    portEXIT_CRITICAL_ISR(&captureMux);
}
}
void initializeBeamCapture(int aPin,int bPin) {
    if(initialized)return;
    initialized=true;lastService=millis();
    if(aPin<0 || bPin<0 || aPin>39 || bPin>39 || aPin==bPin)return;
    highA=aPin>=32;highB=bPin>=32;maskA=1UL<<(aPin%32);maskB=1UL<<(bPin%32);
    // Refuse an existing ISR service: its interrupt allocation flags are unknown.
    // ESP_INTR_FLAG_IRAM permits this handler while flash cache is disabled.
    if(gpio_install_isr_service(ESP_INTR_FLAG_IRAM)!=ESP_OK)return;
    if(gpio_set_intr_type(static_cast<gpio_num_t>(aPin),GPIO_INTR_ANYEDGE)!=ESP_OK ||
       gpio_set_intr_type(static_cast<gpio_num_t>(bPin),GPIO_INTR_ANYEDGE)!=ESP_OK ||
       gpio_isr_handler_add(static_cast<gpio_num_t>(aPin),captureEdge,nullptr)!=ESP_OK ||
       gpio_isr_handler_add(static_cast<gpio_num_t>(bPin),captureEdge,nullptr)!=ESP_OK)return;
    captureOk=true;captureEdge(nullptr); // Seed actual boot occupancy; never invent an edge.
}
void serviceBeamCapture(uint32_t debounceMs,BeamSampleSink sink) {
    if(draining || !captureOk)return;
    draining=true;
    uint32_t now=millis();if(now-lastService>maxGap)maxGap=now-lastService;
    if(now-lastService>=4000000UL) {
        // A low-32-bit microsecond timestamp cannot disambiguate longer stalls.
        // Discard the uncertain batch with a sticky visible capture-loss fault.
        portENTER_CRITICAL(&captureMux);if(dropped!=UINT32_MAX)++dropped;portEXIT_CRITICAL(&captureMux);
    }
    lastService=now;
    for(;;) {
        Edge edge{};bool available;uint64_t boundaryUs;
        portENTER_CRITICAL(&captureMux);
        if(dropped!=seenDropped) {
            seenDropped=dropped;tail=head;
            timeline=traffic::BeamTimeline{};resetBeamDetectorAfterLoss();
            uint32_t lo=GPIO.in,hi=GPIO.in1.val;
            edges[head].levels=((highA?hi:lo)&maskA?1U:0U)|((highB?hi:lo)&maskB?2U:0U);
            edges[head].us=static_cast<uint32_t>(esp_timer_get_time());head=(head+1)&(capacity-1);
        }
        available=tail!=head;
        if(available){edge=edges[tail];tail=(tail+1)&(capacity-1);}
        boundaryUs=esp_timer_get_time();
        portEXIT_CRITICAL(&captureMux);
        if(!available) {
            timeline.finish(static_cast<uint32_t>(boundaryUs/1000U),debounceMs,sink);break;
        }
        // Queued intervals must be < 71 minutes (32-bit microsecond wrap).
        // Capacity/overflow health is explicit; nominal flash writes are far shorter.
        uint32_t at=static_cast<uint32_t>((boundaryUs-uint32_t(uint32_t(boundaryUs)-edge.us))/1000U);
        timeline.edge(at,edge.levels&1,edge.levels&2,debounceMs,sink);
    }
    clearCountEventTime();draining=false;
}
uint32_t beamCaptureDropped(){return dropped;}
uint32_t beamCaptureMaxGapMs(){return maxGap;}
const char* beamCaptureHealth(){return !captureOk?"ISR_START_FAILED":dropped?"OVERFLOW_REVIEW_COUNTS":"OK";}
void publishBeamCaptureHealth() {
    String base=String(topic_base_path)+"/System/";
    publishMQTT((base+"beamCaptureHealth").c_str(),beamCaptureHealth(),true);
    publishMQTT((base+"beamCaptureDropped").c_str(),String(uint32_t(dropped)),true);
    publishMQTT((base+"beamDrainMaxGapMs").c_str(),String(maxGap),true);
}
void setCountEventMillis(uint32_t ms){eventMillis=ms;eventActive=true;}
void clearCountEventTime(){eventActive=false;}
DateTime countEventTime() {
    DateTime now=rtc.now();
    if(!eventActive)return now;
    uint32_t age=(millis()-eventMillis)/1000U;
    return DateTime(now.unixtime()>=age?now.unixtime()-age:0);
}
bool countEventClockValid(){return rtcReady && !rtc.lostPower() && countEventTime().isValid() && countEventTime().year()==recordingYear();}
