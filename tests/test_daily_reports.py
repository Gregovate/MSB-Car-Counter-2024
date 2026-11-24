from pathlib import Path
import subprocess,shutil,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class DailyReports(unittest.TestCase):
 def test_shared_report_viewer_renders_latest_night_and_handles_quoted_csv(self):
  page=(ROOT/'data/showSummary.html').read_text();header=(ROOT/'include/traffic/DailyReportPage.h').read_text()
  self.assertEqual(page,header.split('R"CCREPORT(',1)[1].rsplit(')CCREPORT";',1)[0]);self.assertEqual(page,(ROOT/'data/showSummaryBuckets.html').read_text())
  if not shutil.which('node'):self.skipTest('Node required for report UI check')
  script=page.split('<script>')[1].split('</script>')[0]
  harness=r'''
const assert=require('assert');const elements={};function el(){return {textContent:'',value:'',children:[],replaceChildren(){this.children=[];},appendChild(v){this.children.push(v);},get childElementCount(){return this.children.length;}};}
global.document={getElementById(id){return elements[id]||(elements[id]=el());},createElement:el};global.location={pathname:process.argv[2]};
global.fetch=async url=>({ok:true,text:async()=> 'Date,ShowTotal,Temperature\r\n2026-11-22,18,32\r\n2026-11-23,21,26.4\r\n'});
'''
  checks=r'''
setImmediate(()=>{assert.equal(elements.date.value,'2026-11-23');assert.equal(elements.rows.children.length,1);assert.equal(elements.rows.children[0].children[1].textContent,'21');assert.equal(elements.csv.href,location.pathname.includes('Buckets')?'/ShowSummaryBuckets.csv':'/ShowSummary.csv');assert.deepEqual(parseCsv('a,b\n"x,y","line1\nline2"\n'),[['a','b'],['x,y','line1\nline2']]);assert.deepEqual(parseCsv('a,b\n0,\n'),[['a','b'],['0','']]);elements.date.value='';render();assert.equal(elements.rows.children.length,2);});
'''
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp)/'report.js';p.write_text(harness+script+checks)
   for path in ['/showSummary.html','/showSummaryBuckets.html']:subprocess.run(['node',str(p),path],check=True)
