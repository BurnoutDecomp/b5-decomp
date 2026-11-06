import ctypes,hashlib,importlib.util,json,os,re,struct,subprocess,time
from pathlib import Path
ROOT=Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/device_storage_terra')
OUT=Path(__file__).resolve().parent
SDK=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK')
FALLBACK_BASE=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/vendor')
FALLBACK=[FALLBACK_BASE/p for p in ['EABase/include/Common','EASTL/include','EAThread/include','renderware/include','PPMalloc/include','coreallocator/include','dirtysdk/include','zlib/include','lua/src']]
spec=importlib.util.spec_from_file_location('native_build',ROOT/'tools/build_x360.py');native=importlib.util.module_from_spec(spec);spec.loader.exec_module(native)
INCLUDES=[OUT/'include']+[ROOT/p for p in native.INCLUDES if (ROOT/p).is_dir()]+[SDK/'include/xbox']
compiler=SDK/'bin/win32/cl.exe'
for p in FALLBACK+[compiler]:
 if not p.exists(): raise SystemExit('Missing approved native input: '+str(p))
assert len(FALLBACK)==9
flags=native.FLAGS+['/I'+str(p) for p in INCLUDES]+['/showIncludes']+['/I'+str(p) for p in FALLBACK]
temp=OUT/'task-temp';temp.mkdir(exist_ok=True)
env=dict(os.environ);env.update(TEMP=str(temp),TMP=str(temp),XEDK=str(SDK),PATH=str(SDK/'bin/win32')+os.pathsep+str(Path(os.environ['SystemRoot'])/'System32'),INCLUDE=str(SDK/'include/xbox'),LIB=str(SDK/'lib/xbox'))
for name in ['CL','_CL_','LINK','_LINK_']:env.pop(name,None)
ctypes.windll.kernel32.SetErrorMode(0x0001|0x0002|0x8000)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
key=lambda p:str(p.resolve()).casefold()
suffixes={'.h','.hpp','.hxx','.inl','.inc','.cpp','.c','.asm','.s'}
def snapshot():
 result={}
 for root in INCLUDES+FALLBACK+[OUT/'sources']:
  for folder,dirs,files in os.walk(root):
   dirs[:]=[n for n in dirs if n not in ['.git','build','node_modules','__pycache__']]
   for name in files:
    p=Path(folder)/name
    if p.suffix.lower() in suffixes or not p.suffix:
     k=key(p)
     if k not in result:result[k]={'path':str(p.resolve()),'sha256':sha(p)}
 for p in [compiler,ROOT/'tools/build_x360.py',Path(__file__)]:result[key(p)]={'path':str(p.resolve()),'sha256':sha(p)}
 return result
before=snapshot();(OUT/'FROZEN_INPUTS_BEFORE.json').write_text(json.dumps(list(before.values()),indent=2)+'\n')
print('Frozen %d native inputs; four probes, jobs=1'%len(before),flush=True)
rows=[]
for source in sorted((OUT/'sources').glob('*.cpp')):
 obj=OUT/(source.stem+'.obj');asm=OUT/(source.stem+'.asm');log=OUT/(source.stem+'.log')
 if obj.exists() or asm.exists():raise SystemExit('Preserve previous attempt artifacts; refusing overwrite')
 command=[str(compiler)]+flags+['/Fo'+str(obj),'/Fa'+str(asm),str(source)]
 started=time.time()
 try:
  run=subprocess.run(command,cwd=ROOT,env=env,stdin=subprocess.DEVNULL,capture_output=True,text=True,errors='replace',timeout=180,creationflags=subprocess.CREATE_NO_WINDOW)
  rc=run.returncode;diagnostics=run.stdout+run.stderr
 except subprocess.TimeoutExpired as e:
  rc=-1;diagnostics='Native compiler timed out\n'+str(e.stdout or '')+str(e.stderr or '')
 log.write_text(diagnostics,encoding='utf-8')
 machine=struct.unpack('<H',obj.read_bytes()[:2])[0] if obj.exists() else None
 paths=[]
 for line in diagnostics.splitlines():
  m=re.match(r'\s*Note:\s+including file:\s*(.+?)\s*$',line)
  if m:paths.append(str(Path(m.group(1)).resolve()))
 row={'source':str(source),'command':command,'seconds':round(time.time()-started,3),'exit_code':rc,'machine':machine,'passed':rc==0 and machine==0x1f2 and asm.exists(),'object':str(obj),'assembly':str(asm),'log':str(log),'includes':paths}
 row['hashes']={str(p):sha(p) for p in [source,log,obj,asm] if p.exists()};rows.append(row)
 print(('PASS ' if row['passed'] else 'FAIL ')+source.name,flush=True)
 if not row['passed']:print('\n'.join(line for line in diagnostics.splitlines() if 'error ' in line or 'fatal ' in line),flush=True)
 (OUT/'PROBE_RESULTS.json').write_text(json.dumps({'state':'running','results':rows},indent=2)+'\n')
after=snapshot();(OUT/'FROZEN_INPUTS_AFTER.json').write_text(json.dumps(list(after.values()),indent=2)+'\n')
changed=[k for k,v in before.items() if k not in after or after[k]['sha256']!=v['sha256']]
bindings=[];missing=[]
for path in dict.fromkeys(p for row in rows for p in row['includes']):
 k=key(Path(path));old=before.get(k);new=after.get(k)
 record={'path':path,'before_sha256':old['sha256'] if old else None,'after_sha256':new['sha256'] if new else None,'stable':bool(old and new and old['sha256']==new['sha256'])}
 if not record['stable']:missing.append(path)
 bindings.append(record)
(OUT/'SHOWINCLUDES_BINDINGS.json').write_text(json.dumps(bindings,indent=2)+'\n')
result={'state':'complete','target':'ARTIST PPC32 big endian','jobs':1,'sdk':str(SDK),'temp':str(temp),'fallback_roots':[str(p) for p in FALLBACK],'compiler_sha256':sha(compiler),'flags':flags,'repo_head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'status':subprocess.check_output(['git','status','--porcelain=v1'],cwd=ROOT,text=True),'input_count':len(before),'input_changes':changed,'unfrozen_includes':missing,'results':rows,'passed':sum(r['passed'] for r in rows),'failed':sum(not r['passed'] for r in rows)}
(OUT/'PROBE_RESULTS.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'passed':result['passed'],'failed':result['failed'],'input_changes':len(changed),'unfrozen_includes':missing},indent=2),flush=True)
raise SystemExit(0 if result['failed']==0 and not changed and not missing else 1)
