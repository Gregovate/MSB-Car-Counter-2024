#!/usr/bin/env python3
"""Export verified SD TrafficJournal records to CSV without changing counts/files.
RTC epochs encode local wall time; deliberately do not apply a second timezone.
"""
import argparse
import csv
from datetime import datetime, timezone
from pathlib import Path
import struct

def fnv(data):
    h=2166136261
    for byte in data:h=((h^byte)*16777619)&0xffffffff
    return h

def decode(path):
    raw=path.read_bytes()
    if len(raw)!=320:raise ValueError(f'{path}: expected 320 bytes')
    u=struct.unpack('<80I',raw)
    if u[0]!=0x4d53424a or u[2] not in (1,2) or u[77]!=0x434d4954 or fnv(raw[:304])!=u[76]:
        raise ValueError(f'{path}: invalid journal schema/checksum/commit')
    undated=u[16]==2 and not any(u[20:76])
    if not undated and (u[20]!=0x4d534232 or fnv(raw[80:300])!=u[75]):
        raise ValueError(f'{path}: invalid count snapshot')
    if not undated:
        try:datetime.strptime(str(u[22]),'%Y%m%d')
        except ValueError:raise ValueError(f'{path}: invalid count date') from None
        if not u[21] or u[25] not in (0,u[22]) or u[74]>1 or any(n>0x7fffffff for n in [u[23],u[24],*u[26:74]]):
            raise ValueError(f'{path}: invalid count values')
    if path.stem.isdigit() and int(path.stem)!=u[1]:raise ValueError(f'{path}: filename/sequence conflict')
    kind={1:'VEHICLE',2:'UNASSIGNED',3:'SNAPSHOT',6:'CORRECTION',7:'SESSION_START',8:'SHOW_CLOSED'}.get(u[16],str(u[16]))
    counter='GC' if 'GC' in path.parts else 'CC' if 'CC' in path.parts else path.parent.as_posix()
    row=dict(counter=counter,sequence=u[1],kind=kind,
             localTime=datetime.fromtimestamp(u[5],timezone.utc).strftime('%Y-%m-%d %H:%M:%S') if u[5] else '',
             bootId=u[4],endUptimeMs=u[6],passageMs=u[7],followMs=u[8],bBrokenMs=u[9],
             betweenCarsMs=u[10],clearBeforeMs=u[11],maxForegroundGapMs=u[12],
             correctionEpoch=u[14] if u[2]==2 else 0,captureLossFlags=u[15],
             countSequence=u[21],countDate=u[22] or '',daily=u[23] if not undated else '',
             show=u[24] if not undated else '',showResetDate=u[25] or '',
             unassignedTotal=u[3] if u[2]==2 else (u[18] if kind=='UNASSIGNED' else 0),
             officialComplete=u[74] if not undated else '')
    row.update({f'hour{h:02d}':u[26+h] if not undated else '' for h in range(24)})
    row.update({f'official{h:02d}':u[50+h] if not undated else '' for h in range(24)})
    return row,raw

def export(source,destination,tests=False):
    records={}
    for path in sorted(source.rglob('*.bin')):
        if path.parent.name!='TrafficJournal':continue
        if any(part.endswith('-TEST') for part in path.parts)!=tests:continue
        row,raw=decode(path);key=(row['counter'],row['sequence'])
        if key in records and records[key][1]!=raw:raise ValueError(f'{path}: conflicting duplicate identity')
        records[key]=(row,raw)
    if not records:raise ValueError('No TrafficJournal/*.bin files found')
    # Validate everything before opening output; a bad record never yields a
    # deceptively complete partial export. Input bytes are never changed.
    rows=[records[key][0] for key in sorted(records)]
    with destination.open('w',newline='',encoding='utf-8-sig') as file:
        writer=csv.DictWriter(file,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
    return len(rows)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path,help='SD backup root or TrafficJournal folder')
    parser.add_argument('destination',type=Path,help='CSV output')
    parser.add_argument('--tests',action='store_true',help='Export only preserved -TEST folders')
    args=parser.parse_args();print(f'Exported {export(args.source,args.destination,args.tests)} verified records to {args.destination}')
