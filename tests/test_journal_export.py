from pathlib import Path
import importlib.util,struct,tempfile,unittest,csv
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('journal_export',ROOT/'tools/export_traffic_journal.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class JournalExport(unittest.TestCase):
 def test_verified_export_preserves_timing_and_rejects_corruption(self):
  u=[0]*80;u[0]=0x4d53424a;u[1]=1;u[2]=2;u[4]=42;u[5]=1795474800
  u[6:13]=[123456,582000,200,581800,1300,150,2125];u[16]=1
  u[20:26]=[0x4d534232,3,20261123,1,1,20261123];u[26+17]=1;u[50+17]=1;u[74]=1
  u[75]=module.fnv(struct.pack('<80I',*u)[80:300]);u[76]=module.fnv(struct.pack('<80I',*u)[:304]);u[77]=0x434d4954;u[78]=u[79]=0xffffffff
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);folder=root/'CC/2026/TrafficJournal';folder.mkdir(parents=True)
   file=folder/'0000000001.bin';raw=struct.pack('<80I',*u);file.write_bytes(raw)
   dest=root/'TrafficFlow.csv';self.assertEqual(module.export(root,dest),1)
   with dest.open(encoding='utf-8-sig',newline='') as f:row=next(csv.DictReader(f))
   self.assertEqual(row['passageMs'],'582000');self.assertEqual(row['betweenCarsMs'],'1300');self.assertEqual(row['daily'],'1')
   self.assertEqual(file.read_bytes(),raw)
   previous=dest.read_bytes();bad=bytearray(raw);bad[100]^=1;file.write_bytes(bad)
   with self.assertRaises(ValueError):module.export(root,dest)
   self.assertEqual(dest.read_bytes(),previous)
