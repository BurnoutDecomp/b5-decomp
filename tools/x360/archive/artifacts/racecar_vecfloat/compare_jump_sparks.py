from pathlib import Path
import ast,re,json,struct,random,hashlib,sys,numpy as np
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu');import emu64 as E,vmxemu as V
p=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py');t=ast.parse(p.read_text());exec(compile(ast.Module(body=[n for n in t.body if isinstance(n,ast.ClassDef) and n.name=='Coff'],type_ignores=[]),'coff','exec'),globals())
HELP=0x40000000;ACTIVE=0x41000000;CAR=0x42000000;STATE=0x43000000;MOD=0x45000000;STACK=0x44000000;RNG=MOD+0x2c3c0
class CPU(E.CPU64):pass
def cpu():
 m=V.Mem()
 for a,n in [(HELP,0x1000),(ACTIVE,0x1000),(CAR,0x1000),(STATE,0x1000),(MOD,0x30000),(STACK,0x20000)]:m.add(a,n)
 c=CPU(m);c.setg(1,STACK+0x10000);return c
class M:pass
M.new_cpu=staticmethod(cpu)
obj=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/final_jump/probe.obj');coff=Coff(obj,'?FireWheelSparks@JumpStateMachine@');rng=random.Random(3614);errors=[]
for i in range(128):
 contact=[int(i%4!=1),int(i%4!=2)];wheels=[[rng.uniform(-100,100) for j in range(8)] for k in range(2)];seed=rng.getrandbits(64);cursor=i%8;buffer=[rng.uniform(1,1.999) for j in range(8)];ground=rng.uniform(-20,20);dt=.016;time=rng.uniform(0,100);outs=[]
 for native in (False,True):
  c=coff.cpu() if native else cpu();c.mem.wr(HELP,struct.pack('>5I',0,ACTIVE,MOD,CAR,0));c.mem.wr(STATE+0x40,struct.pack('>I',CAR));c.mem.wr(STATE+0x10,struct.pack('>2f',dt,time));c.mem.wr(ACTIVE+0x138,struct.pack('>f',ground))
  for off,vals,on in zip((0xe0,0x150),wheels,contact):c.mem.wr(CAR+off,struct.pack('>8f',*vals));c.mem.wr(CAR+off+0x24,struct.pack('>I',0x12340000+off));c.mem.wr(CAR+off+0x28,bytes([on]))
  c.mem.wr(RNG,struct.pack('>8f',*buffer));c.mem.wr(RNG+0x20,struct.pack('>Q',seed));c.mem.wr(RNG+0x28,struct.pack('>I',cursor));c.setg(3,0);c.setg(4,STATE);c.setg(5,HELP);events=[]
  def fire(q):events.append((struct.pack('>4I',*q.vr[1]).hex(),struct.pack('>4I',*q.vr[2]).hex(),*[struct.pack('>f',q.f(n)).hex() for n in (1,2,3)],q.g(6)&0xffffffff,q.mem.rd(q.g(8)&0xffffffff,4).hex()))
  def rand(q):
   p=q.g(4)&0xffffffff;dest=q.g(3)&0xffffffff;cur=struct.unpack('>I',q.mem.rd(p+0x28,4))[0];slot=(cur+3)&4;x=struct.unpack('>f',q.mem.rd(p+slot*4,4))[0];old=struct.unpack('>Q',q.mem.rd(p+0x20,8))[0];new=(old*0x5851f42d4c957f2d+1)&0xffffffffffffffff;bits=0x3f800000|((old>>32)>>9);q.mem.wr(p+0x20,struct.pack('>Q',new));q.mem.wr(p+slot*4,struct.pack('>I',bits));q.mem.wr(p+0x28,struct.pack('>I',slot+1));q.mem.wr(dest,struct.pack('>4f',*[float(np.float32(x-1.0))]*4))
  if native:
   for addr,name in coff.unresolved.items():
    if name.startswith('?RandomVecFloat@'):c.hooks[addr]=rand
    elif name.startswith('?FireJumpSparks@'):c.hooks[addr]=fire
    elif name.startswith('__savevmx_') or name.startswith('__restvmx_'):c.hooks[addr]=lambda q:None
   c.run(coff.entry)
  else:
   c.hooks[0x82c0a1bc]=lambda q:None;c.hooks[0x82c0a454]=lambda q:None;c.hooks[0x822969e0]=fire;c.run(0x82299670)
  outs.append((events,c.mem.rd(RNG,44).hex()))
 if outs[0]!=outs[1]:errors.append({'case':i,'original':outs[0],'compiled':outs[1]})
r={'cases':128,'mismatches':len(errors),'examples':errors[:3],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual compiled FireWheelSparks vs original 82299670 with controlled random provider reproducing original LCG/ring and FireJumpSparks captured; full RNG mutation+vector/FPR arguments, finite values and ground gates; no GPU/hardware proof'};Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/jump-sparks-comparison.json').write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2));assert not errors
