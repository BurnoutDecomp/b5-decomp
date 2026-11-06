from pathlib import Path
import ast,re,json,struct,random,hashlib,sys
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu');import emu64 as E,vmxemu as V
p=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py');t=ast.parse(p.read_text());exec(compile(ast.Module(body=[n for n in t.body if isinstance(n,ast.ClassDef) and n.name=='Coff'],type_ignores=[]),'coff','exec'),globals())
LOC=0x40000000;BODY=0x41000000;OUT=0x42000000;PARAM=0x43000000;STACK=0x44000000
class CPU(E.CPU64):
 def step(self,pc,word,nxt,depth):
  if pc==0x822d0b0c:raise E.Return()
  return super().step(pc,word,nxt,depth)
def cpu():
 m=V.Mem()
 for a,z in [(LOC,0x100),(BODY,0x100),(OUT,0x100),(PARAM,0x1000),(STACK,0x20000)]:m.add(a,z)
 c=CPU(m);c.setg(1,STACK+0x10000);return c
class M:pass
M.new_cpu=staticmethod(cpu)
obj=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/glass_matrix_probe/probe.obj');coff=Coff(obj,'GlassMatrixProbe');rng=random.Random(3606);errors=[]
for i in range(256):
 loc=[rng.uniform(-100,100) for _ in range(16)];body=[rng.uniform(-100,100) for _ in range(16)];outputs=[]
 for compiled in (False,True):
  c=coff.cpu() if compiled else cpu();c.mem.wr(LOC,struct.pack('>16f',*loc));c.mem.wr(BODY,struct.pack('>16f',*body))
  if compiled:c.setg(3,LOC);c.setg(4,BODY);c.setg(5,OUT);c.run(coff.entry);outputs.append(c.mem.rd(OUT,64))
  else:
   for r,v in [(11,LOC),(27,48),(26,32),(25,16),(16,PARAM),(10,STACK+0x10000+0x250)]:c.setg(r,v)
   for r,k in [(124,0),(126,4),(125,8),(117,12),(4,12)]:c.setvf(r,body[k:k+4])
   c.run(0x822d0a6c);outputs.append(c.mem.rd(STACK+0x10000+0x240,64))
 if outputs[0]!=outputs[1]:errors.append({'case':i,'diff':[j for j,(a,b) in enumerate(zip(outputs[0],outputs[1])) if a!=b][:20]})
r={'cases':256,'mismatches':len(errors),'examples':errors[:5],'original':'0x822D0A6C..0x822D0B08','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'original fused matrix composition vs separately compiled equivalent native fixture; finite randomized lanes, full 64-byte result; excludes real glass caller, queue, render, ABI and hardware'};Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/glass-matrix-comparison.json').write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2));assert not errors
