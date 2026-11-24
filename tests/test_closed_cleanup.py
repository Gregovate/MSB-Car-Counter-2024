from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class ClosedCleanup(unittest.TestCase):
 def test_actual_count_service_cleanup_guards_and_reboot(self):
  with tempfile.TemporaryDirectory() as tmp:
   exe=str(Path(tmp)/'cleanup')
   subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(ROOT/'tests/count_stubs'),'-I'+str(ROOT/'include'),str(ROOT/'tests/closed_cleanup_test.cpp'),'-o',exe],check=True)
   subprocess.run([exe],check=True)
