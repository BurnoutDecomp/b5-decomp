from pathlib import Path
import hashlib,json,os,subprocess,sys
ROOT=Path('D:/Reverse/BP-X360-Experimental-Worktrees/wave8_scene_terra')
CANON=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp')
OUT=Path(__file__).resolve().parent;stage=OUT/sys.argv[1];stage.mkdir(exist_ok=False)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
paths=['src/vendor/renderware/collision/GPBox.cpp','src/vendor/renderware/collision/GPInstance.hpp','src/vendor/renderware/collision/Feature.hpp','src/vendor/renderware/collision/FeatureEdge.hpp']
pins={p:sha(ROOT/p)for p in paths}
cmd=[sys.executable,'-X','utf8','-B',str(ROOT/'tools/build_x360.py'),'--sdk',str(CANON/'build/x360/sdk-6534.4/portable/XDK'),'--out',str(stage),'--jobs','1','--timeout','180','--quiet','--compiler-arg=/showIncludes','--compiler-arg=/Z7','--source',paths[0]]
for p in ['EABase/include/Common','EASTL/include','EAThread/include']:cmd+=['--compiler-arg=/I'+str(CANON/'vendor'/p)]
env=dict(os.environ);temp=stage/'temp';temp.mkdir();env['TEMP']=env['TMP']=str(temp)
r=subprocess.run(cmd,cwd=ROOT,env=env,stdin=subprocess.DEVNULL,capture_output=True,text=True,encoding='utf-8',errors='replace');(stage/'driver.log').write_text(r.stdout+r.stderr)
report=json.loads((stage/'compile-results.json').read_text());row=report['results'][0];assert pins=={p:sha(ROOT/p)for p in paths}
includes={}
for line in Path(row['log']).read_text().splitlines():
 if 'including file:'in line:
  p=Path(line.split('including file:',1)[1].strip());includes[str(p.resolve())]=sha(p)
binding={'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip(),'shared_tree_dirty':True,'owned_path':paths[0],'source_and_required_headers':pins,'actual_includes':includes,'command':cmd,'driver_sha256':sha(__file__),'report_sha256':sha(stage/'compile-results.json'),'object_sha256':sha(row['object'])if row['passed']else None}
(stage/'source-object-binding.json').write_text(json.dumps(binding,indent=2)+'\n')
print('TERMINAL',row['passed'],row['source']);print('\n'.join(l for l in Path(row['log']).read_text().splitlines()if 'error 'in l))
