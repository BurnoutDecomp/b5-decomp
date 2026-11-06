from pathlib import Path
import concurrent.futures,hashlib,importlib.util,json,os,struct,subprocess,sys
ROOT=Path('D:/codex-native/rce-constraints-sol')
CANON=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp')
OUT=Path(__file__).resolve().parent
stage=OUT/'final-native-v1';stage.mkdir(exist_ok=False)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
previous=json.loads((OUT/'candidate-class128-v2/compile-results.json').read_text())
sources=[r['source']for r in previous['results']if 'candidate-probe.cpp'not in r['source']]
pins={p:sha(ROOT/p)for p in sources}
spec=importlib.util.spec_from_file_location('builder',ROOT/'tools/build_x360.py')
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
sdk=CANON/'build/x360/sdk-6534.4/portable/XDK';compiler=sdk/'bin/win32/cl.exe'
flags=b.FLAGS+['/Z7','/showIncludes']
flags+=['/I'+str(ROOT/p)for p in b.INCLUDES if (ROOT/p).is_dir()]
flags+=['/I'+str(CANON/'vendor'/p)for p in ['EABase/include/Common','EASTL/include','EAThread/include']]
flags+=['/I'+str(sdk/'include/xbox')]
temp=stage/'temp';temp.mkdir();env=dict(os.environ)
env.update(TEMP=str(temp),TMP=str(temp),XEDK=str(sdk),PATH=str(sdk/'bin/win32')+os.pathsep+str(Path(os.environ['SystemRoot'])/'System32'))
for n in ['CL','_CL_','LINK','_LINK_']:env.pop(n,None)
def build(p):
    key=Path(p).stem+'-'+hashlib.sha256(p.encode()).hexdigest()[:8]
    obj=stage/(key+'.obj');asm=stage/(key+'.asm');log=stage/(key+'.log')
    command=[str(compiler)]+flags+['/Fo'+str(obj),'/Fa'+str(asm),str(ROOT/p)]
    r=subprocess.run(command,cwd=ROOT,env=env,stdin=subprocess.DEVNULL,capture_output=True,text=True,errors='replace',timeout=180,creationflags=subprocess.CREATE_NO_WINDOW)
    text=r.stdout+r.stderr;log.write_text(text,encoding='utf-8');inputs={}
    for line in text.splitlines():
        if 'including file:'in line:
            f=Path(line.split('including file:',1)[1].strip());assert f.is_file();inputs[str(f.resolve())]=sha(f)
    errors=[l for l in text.splitlines()if 'error 'in l]
    passed=r.returncode==0 and obj.is_file()and struct.unpack('<H',obj.read_bytes()[:2])[0]==0x1F2
    return {'source':p,'source_sha256':pins[p],'passed':passed,'returncode':r.returncode,'errors':errors,'command':command,
        'object':str(obj)if obj.exists()else None,'object_sha256':sha(obj)if obj.exists()else None,'assembly':str(asm)if asm.exists()else None,'log':str(log),'inputs':inputs}
report={'lab_head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip(),'driver_sha256':sha(__file__),'compiler_sha256':sha(compiler),'results':[]}
with concurrent.futures.ThreadPoolExecutor(max_workers=2)as pool:
    for row in pool.map(build,sources):
        report['results'].append(row);print(row['passed'],row['source'],'errors',len(row['errors']),flush=True)
        (stage/'compile-results.json').write_text(json.dumps(report,indent=2)+'\n')
assert pins=={p:sha(ROOT/p)for p in sources}
allinputs={k:v for r in report['results']for k,v in r['inputs'].items()}
assert allinputs=={p:sha(p)for p in allinputs},'header changed during build'
report.update(passed=sum(r['passed']for r in report['results']),failed=sum(not r['passed']for r in report['results']),actual_input_count=len(allinputs))
(stage/'compile-results.json').write_text(json.dumps(report,indent=2)+'\n')
print('TERMINAL',report['passed'],report['failed'],len(allinputs),flush=True)
