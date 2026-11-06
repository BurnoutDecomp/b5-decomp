from pathlib import Path
import ast,re,json,struct,random,hashlib,sys
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu');import emu64 as E,vmxemu as V
p=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py');t=ast.parse(p.read_text());exec(compile(ast.Module(body=[n for n in t.body if isinstance(n,ast.ClassDef) and n.name=='Coff'],type_ignores=[]),'coff','exec'),globals())
HELP=0x40000000;ACTIVE=0x41000000;CAR=0x42000000;DEBUG=0x43000000;STACK=0x44000000
class CPU(E.CPU64):pass
def cpu():
 m=V.Mem()
 for a,n in [(HELP,0x1000),(ACTIVE,0x1000),(CAR,0x1000),(DEBUG,0x1000),(STACK,0x20000)]:m.add(a,n)
 c=CPU(m);c.setg(1,STACK+0x10000);return c
class M:pass
M.new_cpu=staticmethod(cpu)
obj=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/final_jump/probe.obj');coff=Coff(obj,'?SetVapourBlend@JumpStateMachine@');rng=random.Random(3613);errs=[]
for i in range(256):
 speed=[0,5,20,21,50,100][i%6];y=[-1,0,1][(i//6)%3];direction=[-1,0,1][(i//18)%3];fade=rng.uniform(0,4);delay=rng.uniform(0,1);ramp=rng.uniform(.01,3);force=(i//54)%2;forced=rng.uniform(-2,2);outs=[]
 for native in (False,True):
  c=coff.cpu() if native else cpu();c.mem.wr(HELP,struct.pack('>5I',0,ACTIVE,0,CAR,DEBUG));c.mem.wr(CAR+0x330,struct.pack('>4f',0,y,speed,0));c.mem.wr(CAR+0x210,struct.pack('>4f',0,0,direction,0));c.mem.wr(DEBUG+0x70,struct.pack('>2f',delay,ramp));c.mem.wr(DEBUG+0x1e,bytes([force]));c.mem.wr(DEBUG+0x2c,struct.pack('>f',forced));c.setg(3,0);c.setg(5,HELP);c.setf(1,fade);trace=[]
  def evaluate(q):
   k=len([x for x in trace if x[0]=='curve']);trace.append(('curve',q.mem.rd(q.g(3)&0xffffffff,12).hex(),q.mem.rd(q.g(4)&0xffffffff,8).hex(),struct.pack('>f',q.f(1)).hex()));q.setf(1,0.625 if k==0 else 0.5)
  def blend(q):trace.append(('blend',q.g(4)&0xffffffff,q.g(5)&0xffffffff,struct.pack('>f',q.f(1)).hex()))
  if native:
   for addr,name in coff.unresolved.items():
    if name.startswith('?Evaluate@SmoothStep@'):c.hooks[addr]=evaluate
    if name.startswith('?SetEffectStateBlend@'):c.hooks[addr]=blend
   c.run(coff.entry)
  else:c.hooks[0x8227d2f0]=evaluate;c.hooks[0x82278488]=blend;c.run(0x82288a58)
  outs.append(trace)
 if outs[0]!=outs[1]:errs.append({'case':i,'original':outs[0],'compiled':outs[1]})
r={'cases':256,'mismatches':len(errs),'examples':errs[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual compiled SetVapourBlend vs ARTIST 82288A58; controlled Evaluate and SetEffectStateBlend, compare FPU pair/triplet bytes, argument order and resulting blend across speed/y/direction/force gates; software VMX estimates not hardware'};Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/jump-curve-caller-comparison.json').write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2));assert not errs
