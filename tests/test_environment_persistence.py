from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class EnvironmentPersistence(unittest.TestCase):
 def test_average_survives_reboot_and_summary_rewrite_preserves_saved_temperature(self):
  with tempfile.TemporaryDirectory() as tmp:
   exe=str(Path(tmp)/'environment')
   subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(ROOT/'tests/count_stubs'),'-I'+str(ROOT/'include'),str(ROOT/'tests/environment_persistence_test.cpp'),'-o',exe],check=True)
   subprocess.run([exe],check=True)
