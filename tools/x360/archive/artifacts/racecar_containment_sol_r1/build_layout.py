from pathlib import Path
import hashlib, importlib.util, json, os, re, struct, subprocess, sys

ROOT = Path('D:/codex-native/racecar-containment-sol-r1')
PARENT = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
OUT = Path(__file__).resolve().parent / sys.argv[1]
OUT.mkdir(parents=True, exist_ok=False)
HEADER = ROOT / 'src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h'
text = re.sub(r'/\*.*?\*/', '', HEADER.read_text(encoding='utf-8-sig'), flags=re.S)
text = re.sub(r'//[^\n]*', '', text)
start = text.index('    u8 maPrecedingState') if '    u8 maPrecedingState' in text else text.index('    f32 mfClippedTime;')
body = text[start:text.index('\n};', start)]
names = []
for decl in body.split(';'):
    decl = decl.strip()
    if any(x in decl for x in ['(', '{', '}']) or not decl: continue
    match = re.search(r'\b([A-Za-z_]\w*)\s*(?:\[[^]]*\])?\s*$', decl)
    if match: names.append(match[1])
names = list(dict.fromkeys(names))
types = ['BrnWorld::RaceCarEntityModule', 'CgsModule::ModuleSingleBuffered',
         'BrnReplays::BaseSerialiser', 'BrnReplays::RaceCarEntitySerialiser',
         'BrnWorld::RaceCarStreamer', 'BrnWorld::PlaceOnTrackManager',
         'BrnWorld::BoostManager', 'BrnWorld::RaceCarEntityModuleDebugComponent',
         'BrnWorld::BoostDebugComponent', 'BrnWorld::NearMissManager',
         'BrnWorld::AirTimeManager', 'BrnWorld::TrafficCheckManager',
         'BrnWorld::CrashPlayManager', 'BrnWorld::PowerParkingManager',
         'CgsWorld::WorldMap2D', 'CgsNumeric::Random', 'Matrix44Affine',
         'BrnWorld::StoredStompeeData', 'Member16', 'Member64', 'Member128']
expressions = [expr for t in types for expr in ['sizeof('+t+')', '__alignof('+t+')']]
expressions += ['offsetof(BrnWorld::RaceCarEntityModule,'+n+')' for n in names]
expressions += ['offsetof(Member'+str(a)+','+f+')' for a in [16,64,128] for f in ['value','after']]
source = '''#include "types.hpp"
#define private public
#define protected public
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"
#include "GameSource/Replays/Serialisers/BrnReplayRaceCarEntitySerialiser.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#undef protected
#undef private
struct Member16 { char prior; __declspec(align(16)) BrnReplays::RaceCarEntitySerialiser value; char after; };
struct Member64 { char prior; __declspec(align(64)) BrnReplays::RaceCarEntitySerialiser value; char after; };
struct Member128 { char prior; __declspec(align(128)) BrnReplays::RaceCarEntitySerialiser value; char after; };
extern "C" const u32 gLayout[] = {
'''+',\n'.join(expressions)+'\n};\n'
(OUT/'layout.cpp').write_bytes(source.replace('\n','\r\n').encode())
spec = importlib.util.spec_from_file_location('builder', ROOT/'tools/build_x360.py')
b = importlib.util.module_from_spec(spec); spec.loader.exec_module(b)
sdk = PARENT/'b5-decomp/build/x360/sdk-6534.4/portable/XDK'
command = [str(sdk/'bin/win32/cl.exe')] + b.FLAGS + ['/Z7', '/showIncludes']
command += ['/I'+str(ROOT/n) for n in b.INCLUDES if (ROOT/n).is_dir()]
command += ['/I'+str(PARENT/'b5-decomp'/n) for n in ['vendor/EABase/include/Common','vendor/EASTL/include','vendor/EAThread/include']]
command += ['/I'+str(sdk/'include/xbox'), '/Fo'+str(OUT/'layout.obj'), '/Fa'+str(OUT/'layout.asm'), str(OUT/'layout.cpp')]
temp = OUT/'temp'; temp.mkdir()
env = dict(os.environ); env.update(XEDK=str(sdk), TEMP=str(temp), TMP=str(temp), PATH=str(sdk/'bin/win32')+os.pathsep+str(Path(os.environ['SystemRoot'])/'System32'))
for k in ['CL','_CL_','LINK','_LINK_']: env.pop(k,None)
r = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, errors='replace', timeout=180, creationflags=subprocess.CREATE_NO_WINDOW)
log = r.stdout+r.stderr
(OUT/'command.json').write_text(json.dumps(command,indent=2)+'\n')
(OUT/'layout.log').write_text(log,encoding='utf-8')
inputs = {str(HEADER):hashlib.sha256(HEADER.read_bytes()).hexdigest()}
for line in log.splitlines():
    if 'including file:' not in line: continue
    path = Path(line.split('including file:',1)[1].strip())
    if path.is_file(): inputs[str(path.resolve())]=hashlib.sha256(path.read_bytes()).hexdigest()
(OUT/'inputs.json').write_text(json.dumps(inputs,indent=2)+'\n')
if r.returncode:
    print('\n'.join(l for l in log.splitlines() if 'including file:' not in l)); raise SystemExit(r.returncode)
data=(OUT/'layout.obj').read_bytes()
machine,nsec,_,symoff,nsyms,optsize,_=struct.unpack_from('<HHIIIHH',data)
assert machine==0x1f2
sections={i+1:struct.unpack_from('<8sIIIIIIHHI',data,20+optsize+i*40) for i in range(nsec)}
strings=data[symoff+nsyms*18:]; i=0; found=[]
while i<nsyms:
    raw,val,sec,kind,storage,aux=struct.unpack_from('<8sIhHBB',data,symoff+i*18)
    if raw[:4]==bytes(4):
        at=struct.unpack_from('<I',raw,4)[0]; name=strings[at:strings.index(b'\0',at)].decode()
    else: name=raw.rstrip(b'\0').decode()
    if name=='gLayout' and sec>0: found.append((val,sec))
    i+=1+aux
assert len(found)==1,found
val,sec=found[0]; values=struct.unpack_from('>'+str(len(expressions))+'I',data,sections[sec][4]+val)
report={'commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip(),
        'types':{t:{'size':values[i*2], 'alignment':values[i*2+1]} for i,t in enumerate(types)},
        'offsets':dict(zip(names,values[len(types)*2:])),
        'alignment_controls':dict(zip(expressions[-6:],values[-6:])),
        'object_sha256':hashlib.sha256(data).hexdigest(), 'inputs':str(OUT/'inputs.json')}
(OUT/'measured-layout.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
