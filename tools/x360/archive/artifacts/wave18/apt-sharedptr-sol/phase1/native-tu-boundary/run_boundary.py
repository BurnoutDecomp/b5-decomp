import ctypes,hashlib,json,os,re,struct,subprocess
from pathlib import Path
OUT=Path(__file__).resolve().parent;PARENT=OUT.parent
old=json.loads((PARENT/'PROBE_RESULTS.json').read_text());assert old['state']=='complete' and old['passed']==4
ROOT=Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/device_storage_terra');SDK=Path(old['sdk']);COMPILER=SDK/'bin/win32/cl.exe'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
oldbindings=json.loads((PARENT/'SHOWINCLUDES_BINDINGS.json').read_text())
paths=[Path(r['path']) for r in oldbindings]+[COMPILER,ROOT/'src/SDKs/EATech/include/Apt/AptSharedPtr.h',ROOT/'src/SDKs/EATech/include/Apt/AptSharedPtr.cpp',Path(__file__)]+list(OUT.glob('*.cpp'))
paths=list(dict.fromkeys(paths));before={str(p.resolve()).casefold():{'path':str(p.resolve()),'sha256':sha(p)} for p in paths}
for r in oldbindings:
 if before[str(Path(r['path']).resolve()).casefold()]['sha256']!=r['after_sha256']:raise SystemExit('Previously resolved input changed: '+r['path'])
(OUT/'FROZEN_INPUTS_BEFORE.json').write_text(json.dumps(list(before.values()),indent=2)+'\n')
temp=OUT/'task-temp';temp.mkdir(exist_ok=True)
env=dict(os.environ);env.update(TEMP=str(temp),TMP=str(temp),XEDK=str(SDK),PATH=str(SDK/'bin/win32')+os.pathsep+str(Path(os.environ['SystemRoot'])/'System32'),INCLUDE=str(SDK/'include/xbox'),LIB=str(SDK/'lib/xbox'))
for name in ['CL','_CL_','LINK','_LINK_']:env.pop(name,None)
ctypes.windll.kernel32.SetErrorMode(0x0001|0x0002|0x8000)
rows=[]
for source in sorted(OUT.glob('*.cpp')):
 obj=source.with_suffix('.obj');asm=source.with_suffix('.asm');log=source.with_suffix('.log')
 assert not obj.exists() and not asm.exists()
 command=[str(COMPILER)]+old['flags']+['/Fo'+str(obj),'/Fa'+str(asm),str(source)]
 run=subprocess.run(command,cwd=ROOT,env=env,stdin=subprocess.DEVNULL,capture_output=True,text=True,errors='replace',timeout=180,creationflags=subprocess.CREATE_NO_WINDOW)
 diagnostics=run.stdout+run.stderr;log.write_text(diagnostics,encoding='utf-8')
 machine=struct.unpack('<H',obj.read_bytes()[:2])[0] if obj.exists() else None
 includes=[str(Path(m.group(1)).resolve()) for line in diagnostics.splitlines() for m in [re.match(r'\s*Note:\s+including file:\s*(.+?)\s*$',line)] if m]
 rows.append({'source':str(source),'command':command,'exit_code':run.returncode,'machine':machine,'passed':run.returncode==0 and machine==0x1f2 and asm.exists(),'includes':includes,'hashes':{str(p):sha(p) for p in [source,obj,asm,log] if p.exists()}})
 print(('PASS ' if rows[-1]['passed'] else 'FAIL ')+source.name,flush=True)
 if not rows[-1]['passed']:print('\n'.join(l for l in diagnostics.splitlines() if 'error ' in l),flush=True)
after={str(p.resolve()).casefold():{'path':str(p.resolve()),'sha256':sha(p)} for p in paths}
changed=[v['path'] for k,v in before.items() if after[k]['sha256']!=v['sha256']]
unfrozen=[p for r in rows for p in r['includes'] if str(Path(p).resolve()).casefold() not in before]
(OUT/'FROZEN_INPUTS_AFTER.json').write_text(json.dumps(list(after.values()),indent=2)+'\n')
result={'state':'complete','reason':'Preserve actual CgsAptAux.cpp to AptLoader.cpp translation-unit boundary, avoiding artificial same-TU inlining in the initial ABI experiment','jobs':1,'flags':old['flags'],'fallback_roots':old['fallback_roots'],'temp':str(temp),'input_changes':changed,'unfrozen_includes':unfrozen,'results':rows,'passed':sum(r['passed'] for r in rows),'failed':sum(not r['passed'] for r in rows)}
(OUT/'PROBE_RESULTS.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:result[k] for k in ['passed','failed','input_changes','unfrozen_includes']},indent=2))
raise SystemExit(0 if result['passed']==2 and not changed and not unfrozen else 1)
