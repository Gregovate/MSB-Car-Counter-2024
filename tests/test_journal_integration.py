from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class JournalIntegration(unittest.TestCase):
 def test_real_count_service_offline(self):
  with tempfile.TemporaryDirectory() as tmp:
   exe=str(Path(tmp)/'journal-counts')
   subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(ROOT/'tests/count_stubs'),'-I'+str(ROOT/'include'),str(ROOT/'tests/journal_integration_test.cpp'),'-o',exe],check=True)
   subprocess.run([exe],check=True)

 def test_production_store_and_archive(self):
  for name in ['journal_store_test.cpp','sd_journal_archive_test.cpp']:
   with self.subTest(component=name),tempfile.TemporaryDirectory() as tmp:
    exe=str(Path(tmp)/'component')
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(ROOT/'tests'/name),'-o',exe],check=True)
    subprocess.run([exe],check=True)
