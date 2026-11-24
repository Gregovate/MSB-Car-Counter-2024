from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class DhtCompatibility(unittest.TestCase):
    def test_both_encodings_and_rejected_faults(self):
        with tempfile.TemporaryDirectory() as tmp:
            exe=str(Path(tmp)/'dht')
            subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(ROOT/'tests/dht_compatibility_test.cpp'),'-o',exe],check=True)
            subprocess.run([exe],check=True)
