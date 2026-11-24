"""Power-cut the production state machine at each persistent write boundary."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ProductionSession(unittest.TestCase):
    def test_every_durable_boundary_and_duplicate_command(self):
        service = (ROOT / 'src/services/TrafficJournal.cpp').read_text()
        calendar = (ROOT / 'src/services/RecordingCalendar.cpp').read_text()
        receipt = service[service.index('Preferences sessionPrefs;'):service.index('const char* health=')]
        transition = service[service.index('namespace {\nbool validSession'):service.index('bool commitClosingCheckpoint')]
        media = calendar[calendar.index('namespace {\nbool transitionFolderAllowed'):]
        harness = r'''
#define TRAFFIC_SESSION_TEST
#define main old_recovery_tests
#include "count_recovery_test.cpp"
#undef main
#include "traffic/SdJournalArchive.h"
namespace fs {using SDFS=Sd;}
auto& journal=nativeJournal;
const char* root(){return "CC";}
const char* storeHealth(){return "OK";}
const char* health="OK";
uint32_t archivedThrough=0,bootId=9;
bool opened=true,durable=true,vehiclePending=false;
bool productionDateMatches(){return rtc.current.d==27;}
void applyRecordingCalendar(){}
RECEIPT
MEDIA
TRANSITION
void coldBoot(){
    reboot();session={};sessionFault=false;archivedThrough=0;
    assert(journal.open(true));initializeProductionSession();
    if(productionBlocked())assert(!restoreCountState());
    else assert(restoreCountState());
}
int main(int argc,char** argv){
    assert(argc==2);
    legacy();disk["/CC/2026"]="";nativeJournalEnabled=true;assert(journal.open(true));
    initializeProductionSession();assert(!productionRequested());
    assert(restoreCountState());recordPendingVehicle();
    ticks+=110;serviceProductionSession();assert(!productionRequested());
    rtc.current=DateTime(2026,11,27,10);serviceCountState();
    assert(totalShowCars==400 && pendingVehicles==1 && journal.pendingCount()==1);
    auto floor=journal.sequence()+1;
    // Reopen with existing test calendar/data before any command.
    coldBoot();assert(!productionRequested() && totalShowCars==400);
    nativeMutations=0;nativeCutAfter=atoi(argv[1]);
    try {
        assert(requestProductionSession());
        for(int i=0;i<100 && !productionActive();++i){ticks+=1100;serviceProductionSession();}
    } catch(int) {
        nativeCutAfter=-1;coldBoot();
        if(!productionRequested())assert(requestProductionSession());
        for(int i=0;i<100 && !productionActive();++i){ticks+=1100;serviceProductionSession();}
    }
    nativeCutAfter=-1;
    int boundaries=nativeMutations;
    assert(productionActive() && productionCardMatches());
    assert(totalShowCars==0 && totalDailyCars==0 && pendingVehicles==0 && journal.pendingCount()==0);
    assert(stringPrefs["vehicles"]=="1" && disk["/CC/2026-TEST/ShowTotal.txt"]=="400");
    for(unsigned seq=1;seq<floor;++seq){
        char name[32];snprintf(name,sizeof(name),"/TrafficJournal/%010u.bin",seq);
        assert(disk.count(std::string("/CC/2026-TEST")+name) && !disk.count(std::string("/CC/2026")+name));
    }
    totalDailyCars=totalShowCars=hourlyCount[17]=officialHourly[17]=1;
    assert(persistCountState());auto saved=journal.sequence();coldBoot();
    assert(requestProductionSession() && productionActive());
    assert(totalShowCars==1 && totalDailyCars==1 && session.floor==floor && journal.sequence()==saved);
    assert(disk["/CC/2025/ShowSummaryBuckets.csv"]=="OFFICIAL UNCHANGED");
    // A stale card must never restore even a higher-sequence test checkpoint.
    disk.erase("/CC/2026/TrafficJournal/"+std::string(10-std::to_string(floor).size(),'0')+std::to_string(floor)+".bin");
    assert(!productionCardMatches());
    std::cout<<boundaries;
}
'''.replace('RECEIPT', receipt).replace('MEDIA', media).replace('TRANSITION', transition)
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / 'session.cpp'
            source.write_text(harness)
            exe = Path(tmp) / 'session'
            subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            '-I'+str(ROOT/'tests'), '-I'+str(ROOT/'tests/count_stubs'),
                            '-I'+str(ROOT/'include'), str(source), '-o', str(exe)], check=True)
            boundaries = int(subprocess.check_output([exe, '-1']))
            self.assertGreater(boundaries, 12)
            for cut in range(1, boundaries+1):
                with self.subTest(cut=cut):
                    subprocess.run([exe, str(cut)], stdout=subprocess.DEVNULL, check=True)
