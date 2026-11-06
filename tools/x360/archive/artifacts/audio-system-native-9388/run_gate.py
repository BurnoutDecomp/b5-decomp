"""Run one-job native source gates with immutable source/include/tool bindings."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,os,re,subprocess,sys
ROOT=Path('D:/Reverse/BP-X360-Experimental-Worktrees/audio-system-native-9388');OUT=Path(__file__).resolve().parent
CANON=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,d):p.write_text(json.dumps(d,indent=2)+'\n',encoding='utf-8')
p=argparse.ArgumentParser();p.add_argument('--stage',required=True);p.add_argument('--source',action='append',required=True);a=p.parse_args()
stage=OUT/a.stage;assert not stage.exists();stage.mkdir();temp=stage/'compiler-temp';temp.mkdir();os.environ['TEMP']=os.environ['TMP']=str(temp)
tracked=subprocess.check_output(['git','ls-files','src','vendor'],cwd=ROOT).decode().splitlines()
files=[x for x in tracked if Path(x).suffix.lower()in('.h','.hpp','.cpp','.c','.inl','.inc')and'pc'not in[p.lower()for p in Path(x).parts]]
before={x:sha(ROOT/x)for x in files};save(stage/'source-before.json',{'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip(),'hashes':before})
sdk=CANON/'build/x360/sdk-6534.4/portable/XDK'
extra=[CANON/x for x in ['vendor/EABase/include/Common','vendor/EASTL/include','vendor/EAThread/include','vendor/renderware/include','vendor/PPMalloc/include','vendor/coreallocator/include','vendor/dirtysdk/include','vendor/zlib/include','vendor/lua/src']]
argv=[str(ROOT/'tools/build_x360.py'),'--sdk',str(sdk),'--out',str(stage/'native'),'--jobs','1','--timeout','180','--quiet','--compiler-arg=/showIncludes']+['--compiler-arg=/I'+str(x)for x in extra]
for source in a.source:argv+=['--source',source]
save(stage/'command.json',{'argv':argv,'jobs':1,'private_temp':str(temp),'driver_sha256':sha(ROOT/'tools/build_x360.py'),'runner_sha256':sha(__file__),'source_manifest_sha256':sha(stage/'source-before.json'),'sdk_tools':{x.name:sha(x)for x in(sdk/'bin/win32').iterdir()if x.name.lower()in('cl.exe','c1.dll','c1xx.dll','c2.dll','msvcr80.dll','mspdb80.dll')}})
print('FROZEN',a.stage,len(files),'inputs;',len(a.source),'whole CPP requests; jobs1',flush=True)
sys.argv=argv;spec=importlib.util.spec_from_file_location('driver',ROOT/'tools/build_x360.py');driver=importlib.util.module_from_spec(spec);spec.loader.exec_module(driver);code=driver.main()
after={x:sha(ROOT/x)for x in files};assert before==after;save(stage/'source-after.json',{'hashes':after,'unchanged':True})
r=json.loads((stage/'native/compile-results.json').read_text());headers={};bindings=[]
for row in r['results']:
 h={}
 for line in Path(row['log']).read_text(encoding='utf-8').splitlines():
  m=re.match(r'\s*Note: including file:\s*(.+?)\s*$',line)
  if not m:continue
  x=Path(m.group(1)).resolve();ss=x.as_posix().lower();assert'/src/pc/'not in ss
  if'/src/'in ss:assert ss.startswith(ROOT.as_posix().lower()+'/')or'/vendor/'in ss or'/xdk/'in ss,ss
  h[str(x)]=sha(x);headers[str(x)]=h[str(x)]
 bindings.append({'source':row['source'],'source_sha256':before[row['source']],'includes':h,'artifacts':{k:{'path':row[k],'sha256':sha(row[k])}for k in('object','assembly','log')if Path(row[k]).exists()}})
 print('PASS'if row['passed']else'FAIL',row['source'],flush=True)
 if not row['passed']:print('\n'.join(s for s in Path(row['log']).read_text().splitlines()if'including file:'not in s)[-10000:],flush=True)
save(stage/'actual-inputs.json',{'results':bindings,'unique_headers':headers})
raise SystemExit(code)
