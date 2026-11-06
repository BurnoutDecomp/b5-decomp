"""Compile retained layout probes against the actual final frozen source headers."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

HERE = Path(__file__).resolve().parent
PARENT = HERE.parents[3]
WORK = Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
ARTIFACT = PARENT / 'b5-decomp'
META = json.loads((PARENT/'scratch/x360_sdk/native-parallel-wave4-final-patch.json').read_text())
RAW = json.loads((ARTIFACT/'build/x360/full-native-parallel-wave4-stage1/compile-results.json').read_text())
OLD = json.loads((ARTIFACT/'build/x360/parallel/effects_contained_terra/final/compile-results.json').read_text())
OUT = ARTIFACT/'build/x360/native-parallel-wave4-final-layout-probes-v2'
assert not OUT.exists()
OUT.mkdir()
assert subprocess.check_output(['git','-C',str(WORK),'rev-parse','HEAD'],text=True).strip() == META['head']
assert not subprocess.check_output(['git','-C',str(WORK),'status','--porcelain=v1']).strip()
env = dict(os.environ)
sdk = Path(RAW['sdk'])
env.update(XEDK=str(sdk), PATH=str(sdk/'bin/win32')+os.pathsep+str(Path(os.environ['SystemRoot'])/'System32'),
           INCLUDE=str(sdk/'include/xbox'), LIB=str(sdk/'lib/xbox'))
temp = HERE/'probe-temp'
temp.mkdir(exist_ok=True)
env['TEMP'] = env['TMP'] = str(temp)
for key in ('CL','_CL_','LINK','_LINK_'):
    env.pop(key,None)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def symbol_data(path, fragment):
    data = Path(path).read_bytes()
    assert struct.unpack_from('<H',data)[0] == 0x1F2
    count = struct.unpack_from('<H',data,2)[0]
    symtab, nsyms = struct.unpack_from('<II',data,8)
    optional = struct.unpack_from('<H',data,16)[0]
    strings = data[symtab+18*nsyms:]
    found = []
    index = 0
    while index < nsyms:
        record = data[symtab+18*index:symtab+18*(index+1)]
        if record[:4] == b'\0'*4:
            offset = struct.unpack_from('<I',record,4)[0]
            name = strings[offset:].split(b'\0',1)[0].decode('ascii',errors='replace')
        else:
            name = record[:8].split(b'\0',1)[0].decode('ascii',errors='replace')
        value, section = struct.unpack_from('<Ih',record,8)
        if fragment in name and section > 0:
            assert section <= count
            header = 20+optional+(section-1)*40
            size, pointer = struct.unpack_from('<II',data,header+16)
            found.append((name,data[pointer+value:pointer+size]))
        index += 1+record[17]
    assert len(found) == 1, (fragment,[x[0] for x in found])
    return found[0][1]


rows = []
for name in ('effects_contained_probe','native_dispatch_io_probe','effects_replay_flags_probe'):
    source = PARENT/'scratch/parallel-native/effects_contained_terra'/(name+'.cpp')
    obj, asm, log = (OUT/(name+ext) for ext in ('.obj','.asm','.log'))
    command = RAW['results'][0]['command'][:-3]+['/Fo'+str(obj),'/Fa'+str(asm),str(source)]
    result = subprocess.run(command,cwd=WORK,env=env,stdin=subprocess.DEVNULL,capture_output=True,
                            text=True,errors='replace',timeout=180,creationflags=subprocess.CREATE_NO_WINDOW)
    log.write_text(result.stdout+result.stderr,encoding='utf-8')
    assert result.returncode == 0 and obj.is_file() and asm.is_file(), (name,result.returncode,str(log))
    assert struct.unpack_from('<H',obj.read_bytes())[0] == 0x1F2
    rows.append({'source':str(source),'source_sha256':digest(source),'command':command,
                 'object':str(obj),'object_sha256':digest(obj),'assembly':str(asm),'assembly_sha256':digest(asm),
                 'log':str(log),'log_sha256':digest(log),'returncode':result.returncode})

old_index = {Path(x['source']).stem:x for x in OLD['results']}
comparisons = []
for name, symbol, length in (('effects_contained_probe','gContainedLayout',31),
                            ('native_dispatch_io_probe','native_dispatch_shape',20)):
    actual = symbol_data(OUT/(name+'.obj'),symbol)[:4*length]
    previous = symbol_data(old_index[name]['object'],symbol)[:4*length]
    assert actual == previous, name
    values = list(struct.unpack('>'+str(length)+'I',actual))
    comparisons.append({'name':name,'values':values,'values_hex':[hex(x) for x in values],
                         'matches_worker_final_values':True,'count':length,
                         'worker_object_sha256':digest(old_index[name]['object'])})
# These constants are independently tied to the named ARTIST parent anchors and
# explicit type alignments in POSTMORTEM.md/types.json, not inferred gap filling.
contained = comparisons[0]['values']
assert contained[2:8] == [0x1E80,128,0,0x1E00,0x1E04,0x1E08]
assert contained[8:10] == [0x210,16]
assert contained[14:16] == [0x9A00,128]
assert contained[18:20] == [0x3B00,0x5980]
assert contained[23:25] == [0x2F600,128]
assert contained[26:31] == [0x2D400,0x2F280,0x2F510,0x2F550,0x5C]
dispatch = comparisons[1]['values']
assert dispatch[:8] == [0x14,4,0,4,8,12,16,0x9A00]
assert dispatch[10:14] == [0x3B00,0x5980,0x9990,0x999C]
assert dispatch[-1] == 0x99C0
final_flags = symbol_data(OUT/'effects_replay_flags_probe.obj','NativeEffectsReplayFlags')
old_flags = symbol_data(old_index['effects_replay_flags_probe']['object'],'NativeEffectsReplayFlags')
assert final_flags == old_flags
report = {'head':META['head'],'patch_sha256':META['sha256'],'passed':3,'failed':0,
          'layout_comparisons':51,'comparisons':comparisons,'results':rows,
          'replay_flags_instruction_bytes_identical_to_worker':True,
          'replay_flags_code_sha256':hashlib.sha256(final_flags).hexdigest(),
          'limits':['Layout and inline setter evidence only; full EffectsModule lifecycle, global vector ABI, native image/link/boot not proved.']}
with (OUT/'probe-results.json').open('x',encoding='utf-8') as stream:
    json.dump(report,stream,indent=2)
print(json.dumps({k:v for k,v in report.items() if k not in ('comparisons','results')},indent=2))
