from pathlib import Path
import ast,re,json,struct,random,hashlib,sys
root=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_resume');sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu');import emu64 as E,vmxemu as V
p=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py');t=ast.parse(p.read_text());exec(compile(ast.Module(body=[n for n in t.body if isinstance(n,ast.ClassDef) and n.name=='Coff'],type_ignores=[]),'coff','exec'),globals())
MEM=0x40000000;STACK=0x44000000;CAM=MEM;POS=MEM+0x100;SQ=MEM+0x200;DIST=MEM+0x300
class CPU(E.CPU64):
 def exec_vector(self,pc,word,mn):
  if pc==0x822e8214:
   a,b,m=self.vr[13],self.vr[9],self.vr[127];self.vr[127]=tuple((a[i]&(~m[i]&0xffffffff))|(b[i]&m[i]) for i in range(4));return True
  return super().exec_vector(pc,word,mn)
 def step(self,pc,word,nxt,depth):
  if pc==getattr(self,'stop',None):raise E.Return()
  return super().step(pc,word,nxt,depth)
def cpu():
 m=V.Mem();m.add(MEM,0x1000);m.add(STACK,0x20000);c=CPU(m);c.setg(1,STACK+0x10000);return c
class M:pass
M.new_cpu=staticmethod(cpu);obj=root/'replay_distance/probe.obj';coff=Coff(obj,'ReplayDistanceProbe')
def run(cam,pos,native):
 c=coff.cpu() if native else cpu();c.mem.wr(CAM,struct.pack('>4f',*cam));c.mem.wr(POS,struct.pack('>4f',*pos))
 if native:
  for r,v in [(3,CAM),(4,POS),(5,SQ),(6,DIST)]:c.setg(r,v)
  c.run(coff.entry);return c.mem.rd(SQ,16),c.mem.rd(DIST,16)
 c.setvf(123,cam);c.setg(11,POS-48);c.setg(26,48);c.setg(21,0);c.stop=0x822e8218;c.run(0x822e81c0);return struct.pack('>4I',*c.vr[0]),struct.pack('>4I',*c.vr[127])
rng=random.Random(3601031);cases=[([0.,0.,0.,0.],[0.,0.,0.,0.]),([8.,0.,0.,0.],[0.,0.,0.,0.]),([12.,0.,0.,0.],[0.,0.,0.,0.])]
for i in range(509):cases.append(([rng.uniform(-500,500) for _ in range(3)]+[0.],[rng.uniform(-500,500) for _ in range(3)]+[0.]))
errors=[]
for i,(a,b) in enumerate(cases):
 try:x=run(a,b,False);y=run(a,b,True)
 except Exception as e:print('error case',i,repr(e));raise
 if x!=y:errors.append({'case':i,'original':[z.hex() for z in x],'native':[z.hex() for z in y]})
res={'cases':len(cases),'mismatches':errors[:20],'total_mismatches':len(errors),'original':'822E81C0..822E8214','native_object':str(obj),'native_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual native intrinsic probe vs ARTIST VMX distance instruction slice, finite coordinates incl exact zero. Emulator uses mathematical reciprocal-sqrt estimate, not physical Xenon estimate. Probe is not integrated into product.'};(root/'replay-distance-comparison.json').write_text(json.dumps(res,indent=2),encoding='utf8');print('distance cases',len(cases),'mismatches',len(errors));assert not errors,errors[:2]
