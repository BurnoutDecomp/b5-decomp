from pathlib import Path
import ast,json,struct,random,hashlib,sys,re
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu');import emu64 as E,vmxemu as V
p=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py');t=ast.parse(p.read_text());exec(compile(ast.Module(body=[n for n in t.body if isinstance(n,ast.ClassDef) and n.name=='Coff'],type_ignores=[]),'coff','exec'),globals())
LAYOUT=0x40000000;CONTACT=0x41000000;STACK=0x44000000
class CPU(E.CPU64):
 def step(self,pc,word,nxt,depth):
  if pc==0x82297fc8:raise E.Return()
  return super().step(pc,word,nxt,depth)
def cpu():
 m=V.Mem();m.add(LAYOUT,0x2000);m.add(CONTACT,0x1000);m.add(STACK,0x20000);c=CPU(m);c.setg(1,STACK+0x10000);return c
class M:pass
M.new_cpu=staticmethod(cpu)
obj=Path('D:/Reverse/BP-X360-Experimental-Artifacts/effects-native-terra/gate-final3/EffectsModule-87aee4b9.obj');coff=Coff(obj,'?RecordCarContact@');rng=random.Random(2513);errors=[]
for i in range(128):
 count=i%33;entityA=rng.getrandbits(32);entityB=rng.getrandbits(32);tag=rng.getrandbits(32);normal=[rng.getrandbits(32) for _ in range(4)];point=[rng.getrandbits(32) for _ in range(4)];initial=bytes(rng.getrandbits(8) for _ in range(0x12b0));outputs=[]
 for native in (False,True):
  q=coff.cpu() if native else cpu();q.mem.wr(LAYOUT,initial);q.mem.wr(LAYOUT+0x34,struct.pack('>i',count));q.setg(3,LAYOUT)
  if native:q.mem.wr(CONTACT,struct.pack('>III',entityA,entityB,tag));q.mem.wr(CONTACT+0x30,struct.pack('>4I',*normal));q.mem.wr(CONTACT+0x40,struct.pack('>4I',*point));q.setg(4,CONTACT)
  else:q.setg(30,entityA);q.setg(31,entityB);q.mem.wr(STACK+0x10000+0x178,struct.pack('>I',tag));q.vr[127]=tuple(normal);q.vr[122]=tuple(point)
  q.run(coff.entry if native else 0x82297f50);outputs.append(q.mem.rd(LAYOUT,0x12b0))
 if outputs[0]!=outputs[1]:errors.append({'case':i,'count':count,'offsets':[k for k,(x,y) in enumerate(zip(*outputs)) if x!=y][:20]})
r={'cases':128,'mismatches':len(errors),'examples':errors[:6],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual compiled RecordCarContact vs ARTIST 82297F50-82297FC4 inline slice; random raw lanes, 0..32 slots, exact full serialized layout including preserved W, full-table gate; excludes caller, replay runtime and hardware'}
Path('D:/Reverse/BP-X360-Experimental-Artifacts/effects-native-terra/replay-contact-comparison.json').write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2));assert not errors

