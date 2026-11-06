"""Compile only two genuine Main bridge caller fixes with private D: artifacts/TEMP."""
from pathlib import Path
import hashlib,importlib.util,json,os,subprocess,sys
ROOT=Path('D:/Reverse/BP-X360-Experimental-Worktrees/boot-callers-526adc')
OUT=Path(__file__).resolve().parent
BASE=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave6')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(n,d):(OUT/n).write_text(json.dumps(d,indent=2)+'\n',encoding='utf-8')
manifest=json.loads((BASE/'native-parallel-wave6-full-stage1-patch.json').read_text())
receipt=json.loads((OUT/'source-edit-receipt.json').read_text())
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip()==manifest['head']==receipt['base']
before={p:sha(ROOT/p)for p in manifest['source_snapshot']['hashes']}
changed=[p for p,v in before.items()if v!=manifest['source_snapshot']['hashes'][p]]
assert set(changed)==set(receipt['files']),changed
save('compiler-source-before.json',{'base':receipt['base'],'hashes':before,'changed_from_frozen':changed,'all_other_6275_match':True})
full=json.loads((BASE/'full-stage1/compile-results.json').read_text())
canonical='c:/users/adri1/.codex/worktrees/fbaf/bp-decomp_workflow/b5-decomp'
extra=[x for x in full['results'][0]['command']if x.replace('\\','/').lower().startswith('/i'+canonical+'/vendor/')]
sdk=full['sdk'];temp=OUT/'compiler-temp';temp.mkdir()
os.environ['TEMP']=os.environ['TMP']=str(temp)
output=OUT/'native-two-callers';assert not output.exists()
argv=[str(ROOT/'tools/build_x360.py'),'--sdk',sdk,'--out',str(output),'--jobs','1','--timeout','180','--quiet','--compiler-arg=/showIncludes']
argv+=['--compiler-arg='+x for x in extra]
for p in receipt['files']:argv+=['--source',p]
save('compile-command.json',{'argv':argv,'jobs':1,'private_temp':str(temp),'driver_sha256':sha(ROOT/'tools/build_x360.py'),'runner_sha256':sha(__file__),'source_before_sha256':sha(OUT/'compiler-source-before.json')})
print('Native whole caller compile, jobs1, D: TEMP; only2 owned CPP changes from frozen526adc',flush=True)
sys.argv=argv
spec=importlib.util.spec_from_file_location('native_driver',ROOT/'tools/build_x360.py');driver=importlib.util.module_from_spec(spec);spec.loader.exec_module(driver)
code=driver.main()
after={p:sha(ROOT/p)for p in before};assert after==before
save('compiler-source-after.json',{'base':receipt['base'],'hashes':after,'unchanged_during_compile':True})
r=json.loads((output/'compile-results.json').read_text());assert r['state']=='complete'
for row in r['results']:
 assert row['source']in receipt['files']
 print('PASS'if row['passed']else'FAIL',row['source'],flush=True)
 if not row['passed']:print(Path(row['log']).read_text(encoding='utf-8')[-5000:],flush=True)
print('SOURCE-BOUND',len(before),'files; two changed; remaining6275 identical',flush=True)
raise SystemExit(code)
