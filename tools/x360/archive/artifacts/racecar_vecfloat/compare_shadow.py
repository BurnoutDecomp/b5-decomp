from pathlib import Path
import ast,re,json,struct,random,hashlib,sys
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu');import emu64 as E,vmxemu as V
p=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py');t=ast.parse(p.read_text());exec(compile(ast.Module(body=[n for n in t.body if isinstance(n,ast.ClassDef) and n.name=='Coff'],type_ignores=[]),'coff','exec'),globals())
BASE=0x40000000;PARAM=0x41000000;STATE=0x42000000;SPEC=0x43000000;STACK=0x44000000;INPUT=0x45000000;OUTPUT=0x45001000
class CPU(E.CPU64):
 def exec_vector(self,pc,word,mn):
  if pc == 0x822e7f8c:
   self.vr[126] = struct.unpack('>4I', self.mem.rd((self.g(28)+self.g(11))&0xffffffff,16));return True
  if pc == 0x822e7f10:
   self.vr[124] = tuple((list(self.vr[13])+list(self.vr[0]))[2:6]);return True
  return super().exec_vector(pc,word,mn)
 def step(self,pc,word,nxt,depth):
  if pc==0x822e8040:raise E.Return()
  return super().step(pc,word,nxt,depth)
def cpu():
 m=V.Mem()
 for a,z in [(BASE,0x3000),(PARAM,0x2000),(STATE,0x1000),(SPEC,0x1000),(STACK,0x20000),(INPUT,0x2000)]:m.add(a,z)
 c=CPU(m);c.setg(1,STACK+0x10000);return c
class M:pass
M.new_cpu=staticmethod(cpu)
obj=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/shadow_probe/probe.obj');coff=Coff(obj,'ShadowProbe');print('unresolved',coff.unresolved)
rng=random.Random(3602);err=[]
for i in range(128):
 vectors=[[rng.uniform(-20,20) for _ in range(4)] for j in range(12)]; dist=rng.uniform(-3,3);buf=bytearray(192)
 for j in range(9): struct.pack_into('>4f',buf,j*16,*vectors[j])
 struct.pack_into('>f',buf,144,dist);struct.pack_into('>4f',buf,160,*vectors[9]);struct.pack_into('>4f',buf,176,*vectors[10]);outputs=[]
 for compiled in (False,True):
  c=coff.cpu() if compiled else cpu();c.mem.wr(INPUT,buf)
  if compiled:
   c.setg(3,INPUT);c.setg(4,OUTPUT);c.run(coff.entry);outputs.append(c.mem.rd(OUTPUT,112))
  else:
   c.mem.wr(BASE+0x2120,buf[:32]);c.mem.wr(PARAM+0xb40,buf[32:96]);c.mem.wr(PARAM+0x20,buf[96:112]);c.mem.wr(STATE+0x1c0,buf[112:128]);c.mem.wr(STATE+0x1d0,buf[128:144]);c.mem.wr(STATE+0x1e0,buf[144:148]);c.mem.wr(SPEC+0x680,buf[160:176]);c.mem.wr(SPEC+0x40,buf[176:192]);c.mem.wr(SPEC+0x690,struct.pack('>4f',0,0,0,0));c.setg(23,BASE);c.setg(29,PARAM);c.setg(28,STATE);c.setg(30,0x46000000);c.setg(15,0x82cdade0);c.setg(14,0x82cdadf0);c.setg(31,0);c.setf(31,0.1);c.hooks[0x82285db8]=lambda x:x.setg(3,SPEC);c.run(0x822e7ecc);outputs.append(c.mem.rd(STACK+0x10000+0x150,64)+struct.pack('>4I',*c.vr[1])+struct.pack('>4I',*c.vr[2])+struct.pack('>4I',*c.vr[3]))
 if outputs[0]!=outputs[1]:err.append({'case':i,'diff':[j for j,(a,b) in enumerate(zip(outputs[0],outputs[1])) if a!=b][:40]})
res={'cases':128,'mismatches':len(err),'sample':err[:5],'scope':'ARTIST 822E7ECC..8040 vs separately compiled shadow preparation fixture, resource access and asserts controlled, finite randomized inputs, not actual caller or rendering proof','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/shadow-preparation-comparison.json').write_text(json.dumps(res,indent=2));print(json.dumps(res,indent=2));assert not err
