from pathlib import Path
import ast,re,json,struct,random,hashlib,sys
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu');import emu64 as E,vmxemu as V
p=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py');t=ast.parse(p.read_text());exec(compile(ast.Module(body=[n for n in t.body if isinstance(n,ast.ClassDef) and n.name=='Coff'],type_ignores=[]),'coff','exec'),globals())
LOC=0x40000000;SPEC=0x41000000;STACK=0x44000000
class CPU(E.CPU64):
 def step(self,pc,word,nxt,depth):
  if pc==self.stop:raise E.Return()
  return super().step(pc,word,nxt,depth)
def cpu():
 m=V.Mem();m.add(LOC,0x100);m.add(SPEC,0x100);m.add(STACK,0x20000);c=CPU(m);c.setg(1,STACK+0x10000);return c
class M:pass
M.new_cpu=staticmethod(cpu)
obj=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/body_assert_fixture/probe.obj');specs=[('NativeNormal',0x822cffc8,0x822d0028),('NativeOrthogonal',0x822d0058,0x822d0148),('NativeBodyOrthogonal',0x822cf91c,0x822cfa00)];rng=random.Random(3607);errors=[];details=[]
for sym,start,stop in specs:
 coff=Coff(obj,sym)
 for i in range(256):
  scale=[0.00001,0.001,0.1,1.0,10.0][i%5];vals=[rng.uniform(-scale,scale) for _ in range(16)]
  for k in (0,5,10):vals[k]+=1.0
  data=struct.pack('>16f',*vals);outs=[]
  for native in (False,True):
   c=coff.cpu() if native else cpu();c.stop=stop;c.mem.wr(LOC,data)
   if native:c.setg(3,LOC);c.run(coff.entry);outs.append(c.g(3)&0xffffffff)
   else:
    sp=STACK+0x10000;c.setg(11,LOC);c.setg(26,16);c.setg(24,32);c.setg(18,0x82cda350);c.setg(27,0x4080c);c.setg(22,0);c.setg(3,SPEC);c.mem.wr(SPEC+20,struct.pack('>I',LOC));c.mem.wr(sp+0x100,struct.pack('>I',0x82181500));c.mem.wr(sp+0xf4,struct.pack('>I',0x82181510));c.mem.wr(sp+0xf0,struct.pack('>I',0x82181520));c.setg(11,LOC);c.setf(30,0.01);
    if sym=="NativeBodyOrthogonal":
     c.setg(10,0x820147e0);c.setg(11,0x82181500);c.setg(22,0x82181510);c.setg(9,0x82181520)
     c.setvf(124,vals[0:4]);c.setvf(126,vals[4:8]);c.setvf(125,vals[8:12])
    c.setvf(121,[1.0]*4);c.vr[127]=(0xffffffff,)*4;c.vr[11]=(0x80000000,)*4;c.vr[7]=struct.unpack('>4I',bytes.fromhex("00010203141516170001020300010203"));c.setvf(122,([0.01] if sym=="NativeNormal" else [1.1920928955078125e-7])*4);c.run(start);outs.append(int(c.g(11)==0))
  if outs[0]!=outs[1]:errors.append({'kind':sym,'case':i,'values':outs})
 details.append({'kind':sym,'cases':256})
r={'details':details,'mismatches':len(errors),'sample':errors[:10],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Original locator assertion predicates vs compiled VMX helper; finite values near identity and non-orthonormal matrices; excludes assert calls and NaN, no whole render proof'};Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/body-assert-comparison.json').write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2));assert not errors
