from pathlib import Path
import hashlib,importlib.util,json,os,subprocess,sys
ROOT=Path('D:/Reverse/BP-X360-Experimental-Worktrees/boot-callers-526adc');OUT=Path(__file__).resolve().parent
full=json.loads(Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave6/full-stage1/compile-results.json').read_text());meta=json.loads(Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave6/native-parallel-wave6-full-stage1-patch.json').read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(n,d):(OUT/n).write_text(json.dumps(d,indent=2)+'\n')
before={p:sha(ROOT/p)for p in meta['source_snapshot']['hashes']};save('scene-source-before.json',{'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip(),'hashes':before})
canonical='c:/users/adri1/.codex/worktrees/fbaf/bp-decomp_workflow/b5-decomp';extra=[x for x in full['results'][0]['command']if x.replace('\\','/').lower().startswith('/i'+canonical+'/vendor/')]
argv=[str(ROOT/'tools/build_x360.py'),'--sdk',full['sdk'],'--out',str(OUT/'native-scene-callers'),'--jobs','1','--timeout','180','--quiet','--compiler-arg=/showIncludes']+['--compiler-arg='+x for x in extra]
for p in ['src/GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/CgsLooseOctree.cpp','src/GameShared/GameClasses/SceneManager/CgsSceneManagerModule.cpp','src/GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.cpp']:argv+=['--source',p]
os.environ['TEMP']=os.environ['TMP']=str(OUT/'compiler-temp');save('scene-command.json',{'argv':argv,'jobs':1,'private_temp':os.environ['TEMP'],'source_snapshot_sha256':sha(OUT/'scene-source-before.json')})
sys.argv=argv;spec=importlib.util.spec_from_file_location('driver',ROOT/'tools/build_x360.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
print('Actual recovered Scene callers,3 whole CPPs, jobs1',flush=True);code=m.main()
after={p:sha(ROOT/p)for p in before};assert before==after;save('scene-source-after.json',{'hashes':after,'unchanged_during_compile':True})
r=json.loads((OUT/'native-scene-callers/compile-results.json').read_text())
for row in r['results']:
 print('PASS'if row['passed']else'FAIL',row['source'],flush=True)
 if not row['passed']:print('\n'.join(s for s in Path(row['log']).read_text().splitlines()if'including file:'not in s)[-6000:],flush=True)
raise SystemExit(code)
