from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class Environment(unittest.TestCase):
    def test_actual_sensor_and_average_services(self):
        with tempfile.TemporaryDirectory() as tmp:
            exe=str(Path(tmp)/'environment')
            subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(ROOT/'tests/count_stubs'),'-I'+str(ROOT/'include'),str(ROOT/'tests/environment_test.cpp'),'-o',exe],check=True)
            subprocess.run([exe],check=True)
            subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-DTRAFFIC_TEST_LEGACY_ENV','-I'+str(ROOT/'tests/count_stubs'),'-I'+str(ROOT/'include'),str(ROOT/'tests/environment_test.cpp'),'-o',exe],check=True)
            subprocess.run([exe],check=True)
if __name__=='__main__':unittest.main()
