from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class RollingEnvironment(unittest.TestCase):
 def test_midnight_reboot_dated_replay_and_24_hour_expiry(self):
  with tempfile.TemporaryDirectory() as tmp:
   exe=str(Path(tmp)/'rolling')
   subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(ROOT/'tests/count_stubs'),'-I'+str(ROOT/'include'),str(ROOT/'tests/rolling_environment_test.cpp'),'-o',exe],check=True)
   subprocess.run([exe],check=True)
