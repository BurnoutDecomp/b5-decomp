from pathlib import Path
import json,subprocess,os,hashlib
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave16/ice-layout-recovery-sol')
at=out/'layout-probe-all';at.mkdir(exist_ok=False);(at/'temp').mkdir()
p=at/'NativeICEContainmentLayout.cpp'
p.write_text('#include "GameSource/Game/BrnGameModule.hpp"\nint NativeICEContainmentLayoutProbe() { return sizeof(BrnGame::BrnGameModule); }\n',encoding='utf-8')
r=json.loads((out/'attempt01-native-storage/compile-results.json').read_text(encoding='utf-8'))
row=next(v for v in r['results'] if v['source']=='src/GameSource/Game/BrnGameModule.cpp')
cmd=[]
for a in row['command'][:-1]:
 if a.startswith('/d1reportSingleClassLayout'):continue
 if a.startswith('/Fo'):a='/Fo'+str(at/'NativeICEContainmentLayout.obj')
 if a.startswith('/Fa'):a='/Fa'+str(at/'NativeICEContainmentLayout.asm')
 cmd.append(a)
cmd+=['/d1reportAllClassLayout',str(p)]
(at/'command.json').write_text(json.dumps(cmd,indent=2),encoding='utf-8')
env=dict(os.environ);env['TEMP']=env['TMP']=str(at/'temp')
x=subprocess.run(cmd,cwd='D:/Reverse/BP-X360-Experimental-Worktrees/cxx03_five_luna_04ed',env=env,capture_output=True,text=True,errors='replace',timeout=180)
log=at/'NativeICEContainmentLayout.log';log.write_text(x.stdout+x.stderr,encoding='utf-8')
(at/'result.json').write_text(json.dumps({'returncode':x.returncode,'compiler_output_sha256':hashlib.sha256(log.read_bytes()).hexdigest(),'object_exists':(at/'NativeICEContainmentLayout.obj').exists(),'not_a_collector_source':True,'does_not_change_GameModule_cpp_failure':True},indent=2),encoding='utf-8')
print('probe exit',x.returncode,'log bytes',log.stat().st_size)
lines=log.read_text(encoding='utf-8').splitlines()
for i,l in enumerate(lines):
 if l.startswith(('class ICEController\t','class ICEManager\t','class ICEWrapper\t','class MainDirector\t','class DirectorModule\t','class ReplayDirector\t','class BrnGameModule\t','class GuiModule\t','class EffectsModule\t','class RootSoundModule\t','class ReplayModule\t')):
  print(l)
  if l.startswith('class BrnGameModule\t'):
   print('\n'.join(z for z in lines[i:i+200] if any(v in z for v in ['mInputModule','mGuiModule','mNetworkModule','mEffectsModule','mRootSoundModule','mReplayModule','mDirectorModule'])))
