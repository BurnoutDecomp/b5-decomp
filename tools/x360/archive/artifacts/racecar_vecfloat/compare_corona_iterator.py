from pathlib import Path
import ast,re,json,struct,random,hashlib,sys
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu');import emu64 as E,vmxemu as V
p=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py');t=ast.parse(p.read_text());exec(compile(ast.Module(body=[n for n in t.body if isinstance(n,ast.ClassDef) and n.name=='Coff'],type_ignores=[]),'coff','exec'),globals())
ITER=0x40000000;DATA=0x41000000;STACK=0x44000000
class CPU(E.CPU64):pass
def cpu():
 m=V.Mem();m.add(ITER,0x100);m.add(DATA,0x1000);m.add(STACK,0x20000);c=CPU(m);c.setg(1,STACK+0x10000);return c
class M:pass
M.new_cpu=staticmethod(cpu)
obj=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/vpu2_corona_iterator/probe.obj');coff=Coff(obj,'?Write@Iterator@CoronaBuffer@renderengine@@');rng=random.Random(3610);errors=[]
for i in range(128):
 idx=i%8;data=rng.randbytes(0x400);vs=[tuple(rng.getrandbits(32) for j in range(4)) for k in range(3)];dist=rng.uniform(-100,100);colour=rng.getrandbits(32);texture=rng.getrandbits(32);outs=[]
 for native in (False,True):
  c=coff.cpu() if native else cpu();c.mem.wr(DATA,data);c.mem.wr(ITER,struct.pack('>III',DATA,idx,512));c.setg(3,ITER);c.setg(5,colour);c.setg(6,texture);c.setf(1,dist)
  for r,v in zip((1,2,3),vs):c.vr[r]=v
  c.run(coff.entry if native else 0x823f3350);outs.append((c.mem.rd(ITER,12),c.mem.rd(DATA,0x400)))
 if outs[0]!=outs[1]:errors.append({'case':i,'iterator_diff':outs[0][0].hex()+' '+outs[1][0].hex(),'data_diff':[j for j,(a,b) in enumerate(zip(outs[0][1],outs[1][1])) if a!=b][:12]})
r={'cases':128,'mismatches':len(errors),'examples':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual compiled CoronaBuffer Iterator::Write vs ARTIST 823F3350, full iterator and record array checked with arbitrary vector bits/colors and finite distances; no GPU/runtime claim'};Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/corona-iterator-comparison.json').write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2));assert not errors
