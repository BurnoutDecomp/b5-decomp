from pathlib import Path
import hashlib, importlib.util, json, os, re, struct, subprocess, sys

ROOT = Path('C:/Users/adri1/.codex/worktrees/bp360/racecar_layout_sol')
PARENT = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
OUT = Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_resume') / (sys.argv[1] if len(sys.argv)>1 else 'baseline')
OUT.mkdir(parents=True, exist_ok=True)
HEADER = ROOT / 'src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h'
text = re.sub(r'/\*.*?\*/', '', HEADER.read_text(encoding='utf-8'), flags=re.S)
text = re.sub(r'//[^\n]*', '', text)
body = text[text.index('private:'):text.index('\n};', text.index('private:'))]
names=[]
for declaration in body.split(';'):
    declaration=declaration.strip()
    if '(' in declaration or '{' in declaration or '}' in declaration or not declaration: continue
    match=re.search(r'\b([A-Za-z_]\w*)\s*(?:\[[^]]*\])?\s*$', declaration)
    if match: names.append(match[1])
names=list(dict.fromkeys(names))
types = ['BrnReplays::RaceCarEntitySerialiser', 'BrnWorld::RaceCarEntityModule', 'CgsModule::ModuleSingleBuffered',
         'CgsModule::DataBuffer', 'EA::Thread::RWMutex', 'BrnWorld::RaceCarStreamer',
         'BrnWorld::PlaceOnTrackManager','BrnWorld::BoostManager',
         'BrnWorld::RaceCarEntityModuleDebugComponent','BrnWorld::BoostDebugComponent',
         'BrnWorld::NearMissManager','BrnWorld::AirTimeManager','BrnWorld::TrafficCheckManager',
         'BrnWorld::CrashPlayManager','BrnWorld::PowerParkingManager', 'CgsWorld::WorldMap2D',
         'Attrib::Gen::surfacelist', 'BrnWorld::PlayerVehicleControls', 'CgsNumeric::Random']
expressions = [expr for t in types for expr in ['sizeof('+t+')','__alignof('+t+')']]
expressions += ['offsetof(BrnWorld::RaceCarEntityModule,'+n+')' for n in names]
source = '#include "GameSource/Replays/Serialisers/BrnReplayRaceCarEntitySerialiser.h"\n#include "types.hpp"\n#define private public\n#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"\n#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"\n#undef private\nextern "C" const u32 gRaceCarLayout[] = {\n'+',\n'.join(expressions)+'\n};\n'
(OUT/'layout-probe.cpp').write_bytes(source.replace('\n','\r\n').encode('utf-8'))
spec=importlib.util.spec_from_file_location('native_builder', ROOT/'tools/build_x360.py')
builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
sdk=PARENT/'b5-decomp/build/x360/sdk-6534.4/portable/XDK'
command=[str(sdk/'bin/win32/cl.exe')]+builder.FLAGS
command += ['/I'+str(ROOT/name) for name in builder.INCLUDES if (ROOT/name).is_dir()]
command += ['/I'+str(PARENT/'b5-decomp'/name) for name in ['vendor/EABase/include/Common','vendor/EASTL/include','vendor/EAThread/include']]
command += ['/I'+str(sdk/'include/xbox'),'/Fo'+str(OUT/'layout-probe.obj'),'/Fa'+str(OUT/'layout-probe.asm'),str(OUT/'layout-probe.cpp')]
temp=OUT/'temp';temp.mkdir(exist_ok=True)
env=dict(os.environ);env.update(XEDK=str(sdk), TEMP=str(temp), TMP=str(temp), PATH=str(sdk/'bin/win32')+os.pathsep+str(Path(os.environ['SystemRoot'])/'System32'))
for k in ['CL','_CL_','LINK','_LINK_']:env.pop(k,None)
result=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,errors='replace',timeout=180,creationflags=subprocess.CREATE_NO_WINDOW)
(OUT/'command.json').write_text(json.dumps(command,indent=2)+'\n',encoding='utf-8')
(OUT/'layout-probe.log').write_text(result.stdout+result.stderr,encoding='utf-8')
if result.returncode:print(result.stdout+result.stderr);raise SystemExit(result.returncode)
data=(OUT/'layout-probe.obj').read_bytes()
machine,nsec,_,symoff,nsyms,optsize,_=struct.unpack_from('<HHIIIHH',data);assert machine==0x1f2
sections={i+1:struct.unpack_from('<8sIIIIIIHHI',data,20+optsize+i*40) for i in range(nsec)}
strings=data[symoff+nsyms*18:];i=0;found=[]
while i<nsyms:
    raw,value,sec,kind,storage,aux=struct.unpack_from('<8sIhHBB',data,symoff+i*18)
    if raw[:4]==bytes(4):
        at=struct.unpack_from('<I',raw,4)[0];name=strings[at:strings.index(b'\0',at)].decode()
    else:name=raw.rstrip(b'\0').decode()
    if name=='gRaceCarLayout' and sec>0:found.append((value,sec))
    i+=1+aux
assert len(found)==1,found
value,sec=found[0];values=struct.unpack_from('>'+str(len(expressions))+'I',data,sections[sec][4]+value)
report={'types':{t:{'size':values[i*2], 'alignment':values[i*2+1]} for i,t in enumerate(types)},
        'offsets':dict(zip(names,values[len(types)*2:])), 'object':str(OUT/'layout-probe.obj'),
        'object_sha256':hashlib.sha256(data).hexdigest(), 'header_sha256':hashlib.sha256(HEADER.read_bytes()).hexdigest()}
(OUT/'measured-layout.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report,indent=2))
