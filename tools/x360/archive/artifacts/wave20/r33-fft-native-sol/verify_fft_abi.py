
from pathlib import Path
import ast,struct,hashlib,json,subprocess
a=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts')
x=a/'wave23/commudp-tick-owner-terra/native1'
r=a/'wave19/realmc-finish-luna/native-gate/task-flags-targeted'
oldowner=a/'wave20/r24-intake-repaired/native-selected/commudp_tick_x360-955bad07.obj'
w=r'C:/Users/adri1/.codex/worktrees/bp360/wave4-intake'
t=r'D:/Reverse/BP-X360-Experimental-Worktrees/xaudio-xenon-native-terra'
git=lambda *arg,wd=w:subprocess.check_output(['git','--no-optional-locks','-C',wd,*arg])
parse=ast.parse((a/'wave20/r26-intake-repaired/verify_netconn.py').read_text())
exec(compile(ast.Module(body=[f for f in parse.body if isinstance(f,ast.FunctionDef) and f.name in ('coff','find')],type_ignores=[]),'<coff reader>','exec'))
sha=lambda b:hashlib.sha256(b).hexdigest()
def load(obj):
    b=Path(obj).read_bytes(); s,c=coff(b)
    print(Path(obj).name,sha(b),len(b))
    return s,c

from concurrent.futures import ThreadPoolExecutor
import re
base='6d8819bd88be09537bbf34752aa5d684a3f99f0f'
rd=lambda p:json.loads(Path(p).read_bytes())
def check(x):
    b=Path(x['path']).read_bytes()
    assert sha(b)==x['sha256'],x
    return b



from concurrent.futures import ThreadPoolExecutor
root=a/'wave20/r33-fft-native-sol'
p=root/'native-final'
gate=rd(p/'compile-results.json')
assert gate['requested']==gate['passed']==2 and gate['failed']==0
objects={}
for row in gate['results']:
    objects[Path(row['source']).stem]=coff(Path(row['object']).read_bytes())
    assert row['machine']==498
    log=Path(row['log']).read_text(errors='replace')
    assert not any('\\src\\pc\\' in line for line in log.splitlines())
    assert 'FastFirEngine.h' in log
def defs(o):
    return {n for n,v,s,k in o[0].values() if s>0 and k==2}
def undefined(o):
    return {n for n,v,s,k in o[0].values() if s==0 and k==2}
names={f'?FFT_{f}@core@audio@rw@@YAXPAUFFTState@123@PAM@Z' for f in ('ForwardReal','InverseReal','ForwardComplex','InverseComplex')}
assert names<=defs(objects['FFT'])
real={n for n in names if 'Real' in n}
assert real<=undefined(objects['FastFirEngine'])
for n in sorted(real):
    assert sum(k==6 and name==n for _, rels in objects['FastFirEngine'][1].values() for _,k,name in rels)==1
print('new public definitions',sorted(names))
print('FastFirEngine new undefined calls',sorted(real))
old={f'?FFT_{f}@core@audio@rw@@YAPAXPAXPAM@Z' for f in ('ForwardReal','InverseReal')}
base=a/'wave20/r33-intake/composite-active-native.json'
j=rd(base)
assert j['provenance']['source_head']=='f2eb2b7941b3af91bc6b1262ee82f52ab0d8de1e'
assert len(j['results'])==3695
pat=tuple(n.encode()+b'\0' for n in old)
def scan(row):
    obj=Path(row['object']).read_bytes()
    if not any(k in obj for k in pat):return None
    hit=[(n,sec,k) for n,v,sec,k in coff(obj)[0].values() if n in old]
    if hit:return row['source'],hit
with ThreadPoolExecutor(max_workers=8) as pool:
    hits=[h for h in pool.map(scan,j['results']) if h]
assert len(hits)==1 and hits[0][0].endswith('/FastFirEngine.cpp') and {n for n,s,k in hits[0][1] if s==0 and k==2}==old,hits
print('old declaration had one sealed caller',hits)

