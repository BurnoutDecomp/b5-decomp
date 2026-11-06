from pathlib import Path
import ast,json,struct,random,hashlib,sys,re
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu');import emu64 as E,vmxemu as V
p=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py');t=ast.parse(p.read_text());exec(compile(ast.Module(body=[n for n in t.body if isinstance(n,ast.ClassDef) and n.name=='Coff'],type_ignores=[]),'coff','exec'),globals())
MOD=0x40000000;STACK=0x44000000
class CPU(E.CPU64):pass
def cpu():
 m=V.Mem();m.add(MOD,0x40000);m.add(STACK,0x20000);c=CPU(m);c.setg(1,STACK+0x10000);return c
class M:pass
M.new_cpu=staticmethod(cpu)
obj=Path('D:/Reverse/BP-X360-Experimental-Artifacts/effects-native-terra/gate-final3/EffectsModule-87aee4b9.obj');coff=Coff(obj,'?JunkyardVfxStop@EffectsModule@BrnEffects@@');rng=random.Random(2360);errors=[]
for i in range(64):
 handles=[(rng.getrandbits(24)<<7)|j for j in range(10)]; stored=[h if rng.randrange(2) else h^0x80 for h in handles];outputs=[]
 for native in (False,True):
  q=coff.cpu() if native else cpu();q.mem.wr(MOD+0x2f524,struct.pack('>10I',*handles));events=[]
  for j,h in enumerate(stored):q.mem.wr(MOD+0x5e70+j*112,struct.pack('>I',h))
  q.setg(3,MOD)
  def get(q):
   h=q.g(4)&0xffffffff;slot=MOD+0x5e70+(h&127)*112;old=struct.unpack('>I',q.mem.rd(slot,4))[0];q.setg(3,slot if old==h else 0)
  def stop(q):events.append((q.g(3)&0xffffffff,q.g(4)&0xffffffff))
  if native:
   for addr,name in coff.unresolved.items():
    if name.startswith('?GetLionEffect@'):q.hooks[addr]=get
    elif name.startswith('?StopLionEffect@'):q.hooks[addr]=stop
    elif name.startswith('__savegprlr_') or name.startswith('__restgprlr_'):q.hooks[addr]=lambda c:None
  else:q.hooks[0x8228a238]=stop
  q.run(coff.entry if native else 0x82292028);outputs.append((events,q.mem.rd(MOD+0x2f524,40).hex()))
 if outputs[0]!=outputs[1]:errors.append({'case':i,'original':outputs[0],'compiled':outputs[1]})
r={'cases':64,'mismatches':len(errors),'examples':errors[:4],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual compiled JunkyardVfxStop vs ARTIST 82292028; 10 mixed live/stale handles, stop calls and final handle array; GetLionEffect and StopLionEffect hooks; no particle simulation or hardware'}
Path('D:/Reverse/BP-X360-Experimental-Artifacts/effects-native-terra/junkyard-stop-comparison.json').write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2));assert not errors


