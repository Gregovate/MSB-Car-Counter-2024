import pathlib, shutil, subprocess, tempfile, unittest, re
ROOT=pathlib.Path(__file__).resolve().parents[1]
class CountRecovery(unittest.TestCase):
    def test_actual_count_service(self):
        compiler=shutil.which("g++")
        if not compiler: self.skipTest("g++ required for native fault-injection tests")
        with tempfile.TemporaryDirectory() as tmp:
            exe=str(pathlib.Path(tmp)/"counts")
            subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror", "-I"+str(ROOT/"tests/count_stubs"),"-I"+str(ROOT/"include"),str(ROOT/"tests/count_recovery_test.cpp"),"-o",exe],check=True)
            subprocess.run([exe],check=True)
    def test_real_vehicle_event_survives_immediate_reboot(self):
        compiler=shutil.which("g++")
        if not compiler: self.skipTest("g++ required for native fault-injection tests")
        source=(ROOT/"src/services/VehicleDetector.cpp").read_text(encoding="utf-8")
        body=re.search(r"void countTheCar\(\) \{.*?^\}", source, re.S|re.M).group(0)
        harness='#define main recovery_scenarios\n#include "'+str(ROOT/"tests/count_recovery_test.cpp")+'"\n#undef main\n'+re.search(r'void recordOfficialVehicle\(\) \{.*?^\}',(ROOT/'src/services/ShowSummary.cpp').read_text(),re.S|re.M).group(0)+'\n'+body+\
            r"""
int main() {
    legacy();
    assert(restoreCountState());
    showTime=true;
    countTheCar();
    reboot();
    assert(restoreCountState());
    assert(totalDailyCars==41 && totalShowCars==401 && hourlyCount[16]==41);
    reboot();
    rtc.lost=true;
    assert(!restoreCountState());
    auto before=disk;
    auto pending=pendingVehicles;
    countTheCar();
    assert(pendingVehicles==pending+1 && disk==before && totalDailyCars==0);

    nativeProduction=true;
    rtc.lost=false;
    reboot();
    assert(restoreCountState());
    showTime=false;
    auto daily=totalDailyCars;
    auto show=totalShowCars;
    auto hourly=hourlyCount[16];
    auto official=officialDailyPassages();
    auto events=vehicleActivations;
#ifdef HIGH
    auto greenBefore=greenWrites;
#endif
    countTheCar();
    assert(totalDailyCars==daily+1 && totalShowCars==show);
    assert(hourlyCount[16]==hourly && officialDailyPassages()==official);
    assert(vehicleActivations==events);
#ifdef HIGH
    assert(greenWrites==greenBefore+1);
#endif
    reboot();
    assert(restoreCountState() && totalDailyCars==daily+1);
    assert(resetCountsForShow() && totalDailyCars==0);
    showTime=true;
    countTheCar();
    assert(totalDailyCars==1 && totalShowCars==show+1 && officialDailyPassages()==1);
    assert(vehicleActivations==events+1);
    showTime=false;
    countTheCar();
    assert(totalDailyCars==2 && totalShowCars==show+1 && officialDailyPassages()==1);
    assert(vehicleActivations==events+1);
}
"""
        with tempfile.TemporaryDirectory() as tmp:
            cpp=pathlib.Path(tmp)/"event.cpp";cpp.write_text(harness)
            exe=str(pathlib.Path(tmp)/"event")
            subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-I"+str(ROOT/"tests/count_stubs"),"-I"+str(ROOT/"include"),str(cpp),"-o",exe],check=True)
            subprocess.run([exe],check=True)
if __name__=="__main__": unittest.main()
