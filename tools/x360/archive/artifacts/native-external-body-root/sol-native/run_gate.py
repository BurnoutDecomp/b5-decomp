from pathlib import Path
import hashlib,json,os,subprocess,sys
ROOT=Path('D:/Reverse/BP-X360-Experimental-Worktrees/external-body-native-root');CANON=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp');OUT=Path(__file__).resolve().parent
stage=OUT/sys.argv[1];stage.mkdir(exist_ok=False);sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
sources=['src/GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.cpp','src/SDKs/EATech/rw/math/vpu/vecfloat.cpp']
extra=['src/GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.h','src/GameSource/Physics/PhysicsUtilities/ExternallySimulatedBody.h']
pins={p:sha(ROOT/p)for p in sources+extra}
cmd=[sys.executable,'-X','utf8','-B',str(ROOT/'tools/build_x360.py'),'--sdk',str(CANON/'build/x360/sdk-6534.4/portable/XDK'),'--out',str(stage),'--jobs','1','--timeout','180','--quiet','--compiler-arg=/showIncludes','--compiler-arg=/Z7']
for p in sources:cmd+=['--source',p]
for p in ['EABase/include/Common','EASTL/include','EAThread/include']:cmd+=['--compiler-arg=/I'+str(CANON/'vendor'/p)]
temp=stage/'temp';temp.mkdir();env=dict(os.environ);env['TEMP']=env['TMP']=str(temp)
r=subprocess.run(cmd,cwd=ROOT,env=env,stdin=subprocess.DEVNULL,capture_output=True,text=True,encoding='utf-8',errors='replace');(stage/'driver.log').write_text(r.stdout+r.stderr)
report=json.loads((stage/'compile-results.json').read_text());includes={}
for row in report['results']:
    log=Path(row['log']).read_text();print(row['passed'],row['source']);print('\n'.join(l for l in log.splitlines()if 'error 'in l)[:4000])
    for l in log.splitlines():
        if 'including file:'in l:
            p=Path(l.split('including file:',1)[1].strip());includes[str(p.resolve())]=sha(p)
assert pins=={p:sha(ROOT/p)for p in sources+extra}
(stage/'source-object-binding.json').write_text(json.dumps({'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip(),'owned_source':sources[0],'unchanged_required_provider':sources[1],'source_header_pins':pins,'actual_includes':includes,'command':cmd,'driver_sha256':sha(__file__),'report_sha256':sha(stage/'compile-results.json')},indent=2)+'\n');print('TERMINAL',report['passed'],report['failed'])
