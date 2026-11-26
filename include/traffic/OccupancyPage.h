#pragma once
static const char occupancyPage[] PROGMEM=R"OCC(<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>Hourly park occupancy</title>
<link rel="stylesheet" href="/counter-theme.css"><style>body{font:17px system-ui;margin:2rem;color:#202a36}label,button,a{margin-right:.7rem}input,button{font:inherit;padding:.3rem}button,nav a{background:var(--accent,#475569);color:white;border:0;border-radius:4px;padding:.5rem;text-decoration:none}button:disabled{opacity:.55}table{border-collapse:collapse}td,th{padding:.5rem;border:1px solid #aaa;text-align:right}@media print{nav,form{display:none}}</style>
<h1>Cars in the park by hour</h1>
<p>Starting at zero for the show, each recorded entrance adds one car and each exit subtracts one. The running count carries into the next hour.</p>
<p><strong>Hourly peak</strong> is the highest count reached as those passages occurred. For example, if the count rises from 40 to 70 and falls to 54, the hourly peak is 70 and the hour ends with 54 cars.</p><nav><a href="/showSummaryBuckets.html">Hourly show buckets</a></nav>
<form id="form"><p><label>Show date <input required type="date" id="night"></label></p>
<button>Load report</button><details><summary>Counter addresses</summary><label>Car Counter (entries) <input id="cc" value="192.168.3.31"></label><label>Gate Counter (exits) <input id="gc" value="192.168.3.30"></label></details></form>
<p id="status" role="status"></p><section id="report"></section><nav><button id="csv" disabled>Download CSV</button><button id="copy" disabled>Copy for email</button><button id="print" disabled onclick="print()">Print / Save PDF</button></nav><script>
const byId=id=>document.getElementById(id);
const headers=['Date','Hour beginning (local)','Entries','Exits','Net','End occupancy','Observed peak','Possible peak at tied timestamps','Evidence'];
// Keep the existing CSV headers for spreadsheet consumers.
const labels={'Net':'Change this hour','End occupancy':'Cars at end of hour','Observed peak':'Hourly peak','Possible peak at tied timestamps':'Possible peak if entries came first'};
function hash(bytes){let h=2166136261;for(const b of bytes)h=Math.imul(h^b,16777619);return h>>>0;}
function decode(bytes,sequence){
    const d=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength),u=i=>d.getUint32(i*4,true);
    if(bytes.length!==320||u(0)!==0x4d53424a||u(1)!==sequence||u(2)!==2||u(77)!==0x434d4954||u(76)!==hash(bytes.slice(0,304)))throw Error('Invalid journal record '+sequence);
    const kind=u(16);
    if(kind!==2&&(u(20)!==0x4d534232||u(75)!==hash(bytes.slice(80,300))))throw Error('Invalid count checkpoint '+sequence);
    return {kind,date:u(22),at:u(5),lost:u(15),show:u(24),before:u(19),complete:u(74),pending:u(3),hours:Array.from({length:24},(_,i)=>u(50+i))};
}
async function readNight(address,date){
    const url=new URL('/occupancyJournal','http://'+address);
    async function get(params){url.search=params;const r=await fetch(url,{cache:'no-store'});if(!r.ok)throw Error(await r.text());return r;}
    const meta=await (await get('')).json(),end=meta.last;
    if(!Number.isSafeInteger(end)||end<meta.floor||!meta.floor)throw Error(address+' has no verified production archive');
    const issues=new Set();
    async function batch(seq,n){
        const raw=new Uint8Array(await (await get('first='+seq+'&count='+n)).arrayBuffer());
        if(raw.length!==320*n)throw Error(address+' missing journal evidence near '+seq);
        return Array.from({length:n},(_,i)=>decode(raw.subarray(320*i,320*(i+1)),seq+i));
    }
    // Dated checkpoints are monotonic. An undated record cannot establish a search boundary.
    let lo=meta.floor,hi=end+1;
    while(lo<hi){let mid=lo+Math.floor((hi-lo)/2);const [r]=await batch(mid,1);
        if(!r.date){issues.add('undated event');lo=meta.floor;break;}
        if(r.date<date)lo=mid+1;else hi=mid;
    }
    const times=[],observed=Array(24).fill(0);let closed=false,lastHour=17,seen=false,lastCounts;
    for(let p=lo;p<=end;p+=16){
        const rows=await batch(p,Math.min(16,end-p+1));let past=false;
        for(const r of rows){
            if(!r.date){issues.add('undated event');continue;}
            if(r.date>date){past=true;break;}if(r.date!==date)continue;
            seen=true;lastCounts=r.hours;if(r.lost&1)issues.add('beam capture loss');
            if(r.lost&2)issues.add('unqualified clock');
            if(r.lost&4)issues.add('cutover after window opened');
            if(!r.complete)issues.add('incomplete count baseline');
            if(r.kind===8){closed=true;lastHour=Math.max(lastHour,Math.floor((r.before-1)/60));}
            if(r.kind===1){
                if(r.show!==r.before+1){issues.add('unqualified passage');continue;}
                const local=new Date(r.at*1000);
                if(Number(local.toISOString().slice(0,10).replaceAll('-',''))!==date){issues.add('event date mismatch');continue;}
                times.push(r.at);observed[local.getUTCHours()]++;lastHour=Math.max(lastHour,local.getUTCHours());
            }
        }
        if(past)break;
    }
    if(lastCounts&&lastCounts.some((n,h)=>n!==observed[h]))issues.add('journal passages disagree with hourly checkpoint');
    if(!seen)issues.add('no dated evidence');
    if(!closed)issues.add('closing checkpoint missing');
    return {device:meta.device,times,issues:[...issues],lastHour};
}
function hourly(date,car,gate){
    const bins=new Map(),issues=new Set([...car.issues.map(s=>'Car '+s),...gate.issues.map(s=>'Gate '+s)]);
    for(const [list,index] of [[car.times,0],[gate.times,1]])for(const t of list){const b=bins.get(t)||[0,0];b[index]++;bins.set(t,b);}
    const stamps=[...bins.keys()].sort((a,b)=>a-b);let balance=0;
    return Array.from({length:Math.max(car.lastHour,gate.lastHour)-16},(_,i)=>{
        const hour=17+i;let entries=0,exits=0,peak=balance,possible=balance;
        for(const t of stamps){if(new Date(t*1000).getUTCHours()!==hour)continue;
            const [a,b]=bins.get(t);possible=Math.max(possible,balance+a);balance+=a-b;
            peak=Math.max(peak,balance);entries+=a;exits+=b;if(balance<0)issues.add('negative balance');
        }
        return [date,String(hour).padStart(2,'0')+':00',entries,exits,entries-exits,balance,peak,possible,[...issues].join(' / ')||'verified recorded passages'];
    });
}
let output=[];
const today=new Date();byId('night').value=new Intl.DateTimeFormat('en-CA',{timeZone:'America/Chicago',year:'numeric',month:'2-digit',day:'2-digit'}).format(today);
byId('form').onsubmit=async e=>{
    e.preventDefault();const submit=e.target.querySelector('button');submit.disabled=true;
    output=[];byId('report').replaceChildren();byId('print').disabled=byId('csv').disabled=byId('copy').disabled=true;
    byId('status').textContent='Reading verified journal records from both counters…';
    try{const night=byId('night').value,date=Number(night.replaceAll('-',''));
        const [car,gate]=await Promise.all(['cc','gc'].map(async id=>{
            const name=id==='cc'?'Car Counter':'Gate Counter';
            try{return await readNight(byId(id).value.trim(),date);}
            catch(err){throw Error(name+' — '+err.message);}
        }));
        if(car.device!=='espCarCounter'||gate.device!=='espGateCounter')throw Error('Check Car and Gate addresses');
        output=[headers,...hourly(night,car,gate)];const table=document.createElement('table');
        for(const [index,row] of output.entries()){const tr=table.insertRow();for(const cell of row){const td=document.createElement(index?'td':'th');td.textContent=index ? cell : (labels[cell] || cell);tr.append(td);}}
        byId('report').append(table);byId('print').disabled=byId('csv').disabled=byId('copy').disabled=false;
        byId('status').textContent='If entries and exits share a second, their order is unknown. The possible peak counts those entries first. Check the Evidence column for gaps.';
    }catch(err){byId('status').textContent=err.message;}finally{submit.disabled=false;}
};
byId('csv').onclick=()=>{const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([output.map(row=>row.join(',')).join('\r\n')],{type:'text/csv'}));a.download='Occupancy-'+output[1][0]+'.csv';a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000);};
byId('copy').onclick=()=>{const range=document.createRange();range.selectNodeContents(byId('report'));const s=getSelection();s.removeAllRanges();s.addRange(range);if(!document.execCommand('copy'))byId('status').textContent='Press Ctrl+C to copy the selected report.';};
</script></html>
)OCC";
