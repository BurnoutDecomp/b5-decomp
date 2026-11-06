from pathlib import Path
import hashlib,json,os,subprocess,sys
ROOT=Path('D:/codex-native/replay-base-sol-r3')
CANON=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp')
OUT=Path(__file__).resolve().parent
stage=OUT/sys.argv[1];stage.mkdir(exist_ok=False)
sources=['src/GameSource/Replays/BrnReplayBaseSerialiser.cpp',
 'src/GameSource/Replays/Serialisers/BrnReplayRaceCarEntitySerialiser.cpp',
 'src/GameShared/GameClasses/Core/CgsStringUtils.cpp',
 'src/GameSource/Replays/Serialisers/BrnReplayDirectorBridgeSerialiser.cpp',
 'src/GameSource/Replays/Serialisers/BrnReplayEffectsSerialiser.cpp',
 'src/GameSource/Replays/Serialisers/BrnReplayGameModuleSerialiser.cpp',
 'src/GameSource/Replays/Serialisers/BrnReplayGuiModuleSerialiser.cpp',
 'src/GameSource/Replays/Serialisers/BrnReplayPropEntitySerialiser.cpp',
 'src/GameSource/Replays/Serialisers/BrnReplaySoundSerialiser.cpp',
 'src/GameSource/Replays/Serialisers/BrnReplayTrafficEntitySerialiser.cpp',
 'src/GameSource/Director/BrnDirectorModule.cpp']
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
pins={p:sha(ROOT/p)for p in sources}
head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip()
command=[sys.executable,'-X','utf8','-B',str(ROOT/'tools/build_x360.py'),'--sdk',str(CANON/'build/x360/sdk-6534.4/portable/XDK'),'--out',str(stage),'--jobs','2','--timeout','180','--quiet','--compiler-arg=/showIncludes','--compiler-arg=/Z7']
for p in sources:command+=['--source',p]
for p in ['EABase/include/Common','EASTL/include','EAThread/include']:command+=['--compiler-arg=/I'+str(CANON/'vendor'/p)]
env=dict(os.environ);temp=stage/'temp';temp.mkdir();env['TEMP']=env['TMP']=str(temp)
r=subprocess.run(command,cwd=ROOT,env=env,stdin=subprocess.DEVNULL,capture_output=True,text=True,encoding='utf-8',errors='replace')
(stage/'driver.log').write_text(r.stdout+r.stderr,encoding='utf-8')
report=json.loads((stage/'compile-results.json').read_text());inputs={}
for row in report['results']:
    errors=[l for l in Path(row['log']).read_text(encoding='utf-8').splitlines()if 'error 'in l]
    for line in Path(row['log']).read_text(encoding='utf-8').splitlines():
        if 'including file:'in line:
            p=Path(line.split('including file:',1)[1].strip());assert p.is_file();inputs[str(p.resolve())]=sha(p)
    print(row['passed'],row['source'],len(errors),flush=True)
    if errors:print('\n'.join(errors[:2]),flush=True)
assert pins=={p:sha(ROOT/p)for p in sources}
assert inputs=={p:sha(p)for p in inputs}
bind={'head':head,'sources':pins,'actual_inputs':inputs,'driver_sha256':sha(__file__),'report_sha256':sha(stage/'compile-results.json'),'command':command}
(stage/'source-object-binding.json').write_text(json.dumps(bind,indent=2)+'\n')
print('TERMINAL',report['passed'],report['failed'],len(inputs),flush=True)
