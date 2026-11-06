from pathlib import Path
import hashlib,importlib.util,json,os,subprocess,sys
ROOT=Path('D:/Reverse/BP-X360-Experimental-Worktrees/boot-callers-526adc')
OUT=Path(__file__).resolve().parent
BASE=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave6')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(n,d):(OUT/n).write_text(json.dumps(d,indent=2)+'\n',encoding='utf-8')
full=json.loads((BASE/'full-stage1/compile-results.json').read_text())
meta=json.loads((BASE/'native-parallel-wave6-full-stage1-patch.json').read_text())
before={p:sha(ROOT/p)for p in meta['source_snapshot']['hashes']}
changed=[p for p in before if before[p]!=meta['source_snapshot']['hashes'][p]]
assert set(changed)=={'src/GameSource/Game/GameBridgeGameStateToX_Controller.cpp','src/GameSource/Game/GameBridgeRendererToX.cpp','src/GameShared/GameClasses/System/CgsHardwareInit.h','src/GameShared/GameClasses/System/PS3/CgsHardwareInitPS3.cpp'},changed
save('source-before.json',{'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip(),'hashes':before,'changed_from_526adc':changed})
canonical='c:/users/adri1/.codex/worktrees/fbaf/bp-decomp_workflow/b5-decomp'
extra=[x for x in full['results'][0]['command']if x.replace('\\','/').lower().startswith('/i'+canonical+'/vendor/')]
sources=['src/GameShared/GameClasses/System/PS3/CgsHardwareInitPS3.cpp','src/GameShared/Jobs/Relocator/CgsRelocator.cpp','src/GameShared/GameClasses/System/Resource/CgsResourceBundleLoaderModule.cpp','src/SDKs/EATech/eajobs/job_scheduler.cpp','src/SDKs/EATech/eajobs/job_thread_parameters.cpp','src/SDKs/EATech/eajobs/jobs.cpp','src/GameShared/GameClasses/Memory/CgsHeapMalloc.cpp']
temp=OUT/'compiler-temp';assert not temp.exists();temp.mkdir()
os.environ['TEMP']=os.environ['TMP']=str(temp)
argv=[str(ROOT/'tools/build_x360.py'),'--sdk',full['sdk'],'--out',str(OUT/'native-provider'),'--jobs','1','--timeout','180','--quiet','--compiler-arg=/showIncludes']
argv+=['--compiler-arg='+a for a in extra]
for source in sources:argv+=['--source',source]
save('command.json',{'argv':argv,'jobs':1,'private_temp':str(temp),'runner_sha256':sha(__file__),'driver_sha256':sha(ROOT/'tools/build_x360.py'),'source_snapshot_sha256':sha(OUT/'source-before.json')})
sys.argv=argv
spec=importlib.util.spec_from_file_location('native_driver',ROOT/'tools/build_x360.py');driver=importlib.util.module_from_spec(spec);spec.loader.exec_module(driver)
print('Native HardwareInit provider and real middleware/callers,7 whole CPPs, jobs1',flush=True)
code=driver.main()
after={p:sha(ROOT/p)for p in before};assert after==before;save('source-after.json',{'hashes':after,'unchanged_during_compile':True})
r=json.loads((OUT/'native-provider/compile-results.json').read_text())
for row in r['results']:
 print('PASS'if row['passed']else'FAIL',row['source'],flush=True)
 if not row['passed']:print('\n'.join(s for s in Path(row['log']).read_text().splitlines()if'including file:'not in s)[-5000:],flush=True)
raise SystemExit(code)
