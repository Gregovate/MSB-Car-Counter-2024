from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class OperatingRelease(unittest.TestCase):
    def test_actual_integrated_services(self):
        with tempfile.TemporaryDirectory() as tmp:
            exe=str(Path(tmp)/'operating')
            subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(ROOT/'tests/count_stubs'),'-I'+str(ROOT/'include'),str(ROOT/'tests/operating_release_test.cpp'),'-o',exe],check=True)
            subprocess.run([exe],check=True)
if __name__=='__main__':unittest.main()
