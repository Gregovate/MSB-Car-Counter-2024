"""Host behavior tests; requires g++ (C++17). No device, secrets, or OTA library."""
import datetime
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class RecordingCalendarTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory()
        cls.exe=str(Path(cls.temp.name)/'calendar-test')
        subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                        '-I'+str(ROOT/'tests/calendar_stubs'),'-I'+str(ROOT/'include'),
                        str(ROOT/'tests/recording_calendar_test.cpp'),'-o',cls.exe],check=True)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def test_calendar_paths_and_rollover(self):subprocess.run([self.exe,'normal'],check=True)
    def test_backward_rtc_after_reboot(self):subprocess.run([self.exe,'backward'],check=True)
    def test_missing_clock_uses_saved_year(self):subprocess.run([self.exe,'offline'],check=True)
    def test_sd_override_recovery_with_missing_nvs(self):subprocess.run([self.exe,'sd-recovery'],check=True)
    def test_torn_sd_config_keeps_previous_copy_and_warns(self):subprocess.run([self.exe,'torn-write'],check=True)
    def test_black_friday_all_supported_years(self):
        result=subprocess.check_output([self.exe,'black-friday'],text=True)
        for line in result.splitlines():
            year,day=map(int,line.split())
            thursdays=[d for d in range(1,31) if datetime.date(year,11,d).weekday()==3]
            self.assertEqual(day,thursdays[3]+1)
    def test_no_unguarded_sd_mutations_outside_shared_service(self):
        import re
        for path in (ROOT/'src').rglob('*.cpp'):
            if path.name=='RecordingCalendar.cpp':continue
            self.assertIsNone(re.search(r'\bSD\.(open|mkdir|remove|rename)\s*\(',path.read_text(encoding='utf-8')),str(path))
    def test_no_firmware_publish_to_show_date_command(self):
        source=(ROOT/'src/services/MqttService.cpp').read_text(encoding='utf-8')
        self.assertNotIn('publishMQTT(MQTT_PUB_SHOW_START_DATE,',source)
if __name__=='__main__':unittest.main()
