"""Reject builds that put the beam ISR or its state in cache-backed flash.
Runs automatically after linking. This checks placement, not physical timing.
"""
import re
import subprocess
from pathlib import Path

def verify_elf(elf, objdump):
    table=subprocess.check_output([objdump,'-t',str(elf)],text=True)
    symbols={}
    for line in table.splitlines():
        fields=line.split()
        if len(fields)>=6 and re.fullmatch(r'[0-9a-fA-F]+',fields[0]):
            symbols[fields[-1]]=(int(fields[0],16),fields[-3])
    handlers=[name for name in symbols if 'captureEdge' in name and 'GLOBAL__N_' in name]
    if len(handlers)!=1:raise RuntimeError('Expected exactly one beam capture ISR')
    def in_iram(name):
        address,section=symbols[name]
        return section.startswith('.iram') or 0x40000000<=address<0x40070000
    for name in handlers+['gpio_intr_service','esp_timer_get_time']:
        if name not in symbols or not in_iram(name):raise RuntimeError('Cache-unsafe ISR function: '+name)
    for label in ['edges','head','tail','dropped','maskA','maskB','highA','highB','captureMux']:
        matches=[name for name in symbols if 'GLOBAL__N_' in name and name.endswith(str(len(label))+label+'E')]
        if len(matches)!=1 or not symbols[matches[0]][1].startswith('.dram'):
            raise RuntimeError('Cache-unsafe ISR state: '+label)
    assembly=subprocess.check_output([objdump,'-d','--disassemble='+handlers[0],str(elf)],text=True)
    for name in re.findall(r'\bcall(?:0|4|8|12)\s+[0-9a-fA-F]+\s+<([^>]+)>',assembly):
        name=name.split('+')[0]
        if name in symbols and not in_iram(name):raise RuntimeError('ISR calls flash-backed function: '+name)
    print('PASS: beam ISR, timer, direct callees and queue state are in IRAM/ROM/DRAM')

# PlatformIO invokes this file with SCons' Import available. Direct CLI use is
# useful for reviewing a saved licensed-build ELF without rebuilding it.
if 'Import' in globals():
    Import('env')
    def after_link(source,target,env):
        compiler=env.WhereIs(env.subst('$CC'))
        if not compiler:raise RuntimeError('Cannot locate ESP32 compiler for ISR placement check')
        executable=Path(compiler).name.replace('gcc','objdump')
        objdump=str(Path(compiler).with_name(executable))
        verify_elf(Path(str(target[0])),objdump)
    env.AddPostAction('$BUILD_DIR/${PROGNAME}.elf',after_link)
elif __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser();parser.add_argument('elf');parser.add_argument('--objdump',default='xtensa-esp32-elf-objdump')
    args=parser.parse_args();verify_elf(args.elf,args.objdump)
