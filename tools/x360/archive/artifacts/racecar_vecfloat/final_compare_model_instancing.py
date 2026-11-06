from pathlib import Path
import ast,re,json,struct,random,hashlib,sys
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu');import emu64 as E,vmxemu as V
p=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py');t=ast.parse(p.read_text());exec(compile(ast.Module(body=[n for n in t.body if isinstance(n,ast.ClassDef) and n.name=='Coff'],type_ignores=[]),'coff','exec'),globals())
CONSTS=0x40000000;PTRS=0x41000000;MATRICES=0x42000000;STACK=0x44000000;ORIGGLOBALS=0x83011000
class CPU(E.CPU64):
 def step(self,pc,word,nxt,depth):
  if pc==0x827fbd1c:raise E.Return()
  if (word >> 26)==8:
   rd=(word>>21)&31;ra=(word>>16)&31;imm=word&65535;imm=imm-65536 if imm&32768 else imm;self.setg(rd,imm-self.g(ra));return nxt
  return super().step(pc,word,nxt,depth)
def cpu():
 m=V.Mem()
 for a,z in [(CONSTS,0x1000),(PTRS,0x1000),(MATRICES,0x1000),(STACK,0x20000),(ORIGGLOBALS,0x1000)]:m.add(a,z)
 for i in range(5):m.wr(0x830111c0+i*16,struct.pack('>4f',0.,0.,0.,float(i)))
 c=CPU(m);c.setg(1,STACK+0x10000);return c
class M:pass
M.new_cpu=staticmethod(cpu)
obj=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/final_model/probe.obj');coff=Coff(obj,'?SetupShaderConstantsForInstancing@Model@');rng=random.Random(3608);errors=[]
for i in range(128):
 count=i%5;values=bytes(rng.randbytes(80));mats=bytes(rng.randbytes(5*64));outs=[]
 for native in (False,True):
  c=coff.cpu() if native else cpu();c.mem.wr(CONSTS,values);c.mem.wr(MATRICES,mats)
  for j in range(5):c.mem.wr(PTRS+4*j,struct.pack('>I',MATRICES+j*64))
  c.setg(3,count);c.setg(4,PTRS);c.setg(5,CONSTS)
  if native:
   captures={}
   for addr,name in coff.unresolved.items():
    if name.startswith('?SetShaderConstantArrayData@ShaderConstantTable@@'):
     def capture(cpu):
      k=cpu.g(4)&0xffffffff;a=cpu.g(5)&0xffffffff;captures[k]=cpu.mem.rd(a,80 if k==7 else 320)
     c.hooks[addr]=capture
   c.run(coff.entry);outs.append((captures.get(7),captures.get(6)))
  else:
   c.run(0x827fbb98);sp=STACK+0x10000-0x210;outs.append((c.mem.rd(sp+0x50,80),c.mem.rd(sp+0xa0,320)))
 if outs[0]!=outs[1]:errors.append({'case':i,'index_diffs':[j for j,(a,b) in enumerate(zip(outs[0][0],outs[1][0])) if a!=b][:10], 'matrix_diffs':[j for j,(a,b) in enumerate(zip(outs[0][1],outs[1][1])) if a!=b][:10]})
r={'cases':128,'mismatches':len(errors),'examples':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual compiled CgsModel instancing provider vs ARTIST 827FBB98..827FBD1C, 0..4 live matrices, all 80 index and 320 matrix bytes incl random bit patterns; shader uploads controlled, original static table initialized from CRT, no GPU/hardware proof'};Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/final-model-instancing-comparison.json').write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2));assert not errors
