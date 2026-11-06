from pathlib import Path
import sys,struct,json,itertools,random,hashlib
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu');import emu64 as E,vmxemu as V
OUT=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_resume');MOD=0x40000000;INPUT=0x41000000;CAM=0x41010000;SHADOW=0x41020000;LAYOUT=0x42000000;STATE=0x43000000;STACK=0x44000000;TABLE=0x45000000
class CPU(E.CPU64):
 def exec_vector(self,pc,word,mn):
  if pc==0x822e8214:
   # vsel128 v127, v13, v9, v127 -- preserve mask bitwise.
   a,b,m=self.vr[13],self.vr[9],self.vr[127]
   self.vr[127]=tuple(((a[i] & (~m[i] & 0xffffffff)) | (b[i] & m[i])) for i in range(4));return True
  return super().exec_vector(pc,word,mn)
 def step(self,pc,word,nxt,depth):
  if pc in (0x822e8240,0x822e83a0):self.decision=pc==0x822e8240;raise E.Return()
  return super().step(pc,word,nxt,depth)
def put32(mem,addr,v):mem.wr(addr,struct.pack('>I',v&0xffffffff))
def put64(mem,addr,v):mem.wr(addr,struct.pack('>Q',v))
def original(case):
 mode,owner,idx,hide,player,active,loaded,frameactive,env,cascade,distance=case
 m=V.Mem()
 for addr,size in [(MOD,0x20000),(INPUT,0x1000),(CAM,0x1000),(SHADOW,0x2000),(LAYOUT,0x20000),(STATE,0x1000),(STACK,0x20000),(TABLE,0x100)]:m.add(addr,size)
 c=CPU(m);sp=STACK+0x10000;c.setg(1,sp);put32(m,sp+0x34c,INPUT);put32(m,MOD+0x18600,mode);put32(m,MOD+0x182f8,player);put64(m,CAM+0x140,4 if hide else 0);put32(m,SHADOW+0x14f4,cascade)
 for i,val in enumerate([0.,64.,144.]):m.wr(TABLE+16*i,struct.pack('>4f',*[val]*4))
 m.wr(STATE+0x1f0+0x30,struct.pack('>4f',0.,0.,0.,0.));c.setvf(123,[distance,0.,0.,0.]);
 for r,v in [(22,MOD),(29,(owner<<24)|(idx<<10)),(25,0x182f8),(18,0x11100),(21,env),(19,SHADOW),(24,TABLE),(26,48),(31,0),(20,1)]:c.setg(r,v)
 trace=[]
 def h(name,val):
  def f(cpu):trace.append(name);cpu.setg(3,val)
  return f
 for addr,name,val in [(0x822b69c8,'camera',CAM),(0x822a1500,'stream_active',active),(0x822a1450,'stream_loaded',loaded),(0x822a3400,'layout',LAYOUT),(0x82277b10,'frame_active',frameactive),(0x8227d690,'state',STATE)]:c.hooks[addr]=h(name,val)
 c.decision=None;c.run(0x822e80c4);assert c.decision is not None;return c.decision,trace

def model(case):
 mode,owner,idx,hide,player,active,loaded,frameactive,env,cascade,distance=case;trace=[]
 if mode not in (4,5,6) or owner!=0x21:return False,trace
 trace.append('camera')
 if hide and idx==(player&0xffff):return False,trace
 trace.append('stream_active')
 if not active:return False,trace
 trace.append('stream_loaded')
 if not loaded:return False,trace
 trace.extend(['layout','frame_active'])
 if not frameactive:return False,trace
 trace.append('state')
 if env and [0.,64.,144.][cascade] > distance*distance:return False,trace
 return True,trace
rng=random.Random(3601031);cases=[]
for mode,owner,hide,active,loaded,frameactive,env,cascade,dist in itertools.product([0,4,5,6,7],[1,0x21],[0,1],[0,1],[0,1],[0,1],[0,1],[0,1,2],[0.,8.,12.,20.]):
 idx=rng.randrange(8);player=idx if rng.randrange(2) else (idx+1)%8;cases.append((mode,owner,idx,hide,player,active,loaded,frameactive,env,cascade,dist))
errors=[]
for i,case in enumerate(cases):
 try:a=original(case);b=model(case)
 except Exception as ex:print('ERROR',i,case,repr(ex));raise
 if a!=b:errors.append({'case':case,'original':a,'model':b})
res={'cases':len(cases),'mismatches':errors[:20],'total_mismatches':len(errors),'original':'822E80C4..822E823C','scope':'Actual ARTIST replay mode, entity owner, camera hide, streamed/loaded, recorded-active and shadow cascade gates, plus controlled callee order. Finite axis-aligned positions only; VMX rsqrt is modeled by emulator, no RenderRaceCar, live-visible path or hardware claim.','cull_thresholds_source':'82C4BCBC..82C4BD18 initializes vectors [0,64,144] at 82FAD4A0; compares squared distance.'};(OUT/'replay-gate-original-comparison.json').write_text(json.dumps(res,indent=2),encoding='utf8');print('cases',len(cases),'mismatches',len(errors));assert not errors,errors[:3]
