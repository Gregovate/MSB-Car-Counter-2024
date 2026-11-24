"""Exercise the same date lookup, integrity checks and hourly arithmetic as the web report."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Occupancy(unittest.TestCase):
    def test_paired_archives_and_uncertainty(self):
        js = (ROOT/'include/traffic/OccupancyPage.h').read_text().split('<script>')[1].split('let output=[];')[0]
        harness = r'''
const assert=require('node:assert/strict');
const epoch=s=>Date.parse('2026-11-27T'+s+'Z')/1000;
let damaged=false,missing=false,wrongTotal=false;
const archive=[
    [7,20261126,0,0],[3,20261127,0,0],[1,20261127,epoch('17:00:00'),1],
    [1,20261127,epoch('17:08:00'),2],[8,20261127,epoch('21:00:00'),2],
    [3,20261128,0,0]
];
global.fetch=async url=>{
    const q=url.searchParams;
    if(!q.has('first'))return {ok:true,json:async()=>({device:'espCarCounter',floor:1,last:6})};
    const start=+q.get('first'),count=+q.get('count');
    const bytes=new Uint8Array(count*320);
    for(let i=0;i<count;i++){
        const b=bytes.subarray(i*320,(i+1)*320),v=new DataView(b.buffer,b.byteOffset,320);
        const put=(n,x)=>v.setUint32(n*4,x,true);
        const [kind,date,at,total]=archive[start+i-1];
        put(0,0x4d53424a);put(1,start+i);put(2,2);put(5,at);put(16,kind);
        put(19,kind===8?1260:total-1);put(20,0x4d534232);put(21,start+i);put(22,date);
        put(24,total);put(67,kind===8&&wrongTotal?3:total);put(74,1);
        put(75,hash(b.slice(80,300)));put(76,hash(b.slice(0,304)));put(77,0x434d4954);
        if(damaged)b[160]^=1;
    }
    return {ok:true,arrayBuffer:async()=>missing?bytes.slice(0,319).buffer:bytes.buffer};
};
(async()=>{
    const car=await readNight('car',20261127);
    assert.deepEqual(car.issues,[]);assert.equal(car.times.length,2);
    const gate={times:[epoch('17:13:00'),epoch('21:24:59')],lastHour:21,issues:[]};
    const rows=hourly('2026-11-27',car,gate);
    assert.deepEqual(rows[0].slice(2,8),[2,1,1,1,2,2]);
    assert.equal(rows[3][5],1);assert.equal(rows[4][5],0);
    gate.times=[epoch('17:00:00'),epoch('17:00:00'),epoch('17:15:00')];
    const negative=hourly('2026-11-27',car,gate)[0];
    assert.equal(negative[5],-1);assert.equal(negative[6],0);assert.equal(negative[7],1);
    assert.match(negative[8],/negative/);
    wrongTotal=true;assert.match((await readNight('car',20261127)).issues.join(','),/disagree/);
    damaged=true;await assert.rejects(readNight('car',20261127),/Invalid journal/);
    damaged=false;missing=true;await assert.rejects(readNight('car',20261127),/missing journal/);
})().catch(e=>{console.error(e);process.exitCode=1;});
'''
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)/'occupancy.js'
            path.write_text(js+harness)
            subprocess.run(['node',str(path)],check=True)

    def test_live_dated_entrance_replay_and_expiry(self):
        program = r'''
#include "traffic/DatedOccupancy.h"
#include <cassert>
int main(){
    traffic::EntranceReading reading;
    assert(!reading.fresh(20261127,1000));
    assert(!reading.accept("20261126,72,998",20261127,1000));
    assert(reading.accept("20261127,3,999",20261127,1000));
    assert(!reading.accept("20261127,0,995",20261127,1000));
    assert(!reading.accept("20261127,0,1100",20261127,1000));
    assert(!reading.accept("20261127,4294967296,1000",20261127,1000));
    assert(reading.fresh(20261127,1119) && !reading.fresh(20261127,1120));
    char payload[48];reading.encode(payload);assert(!strcmp(payload,"20261127,3,999"));
    assert(reading.accept("20261128,0,86499",20261128,86500));
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)/'live.cpp'
            path.write_text(program)
            exe = Path(tmp)/'live'
            subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(path),'-o',str(exe)],check=True)
            subprocess.run([exe],check=True)
