from pathlib import Path
import sys,struct,json,itertools,random
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu');import emu64 as E,vmxemu as V
OUT=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_resume');MOD=0x40000000;INPUT=0x41000000;CAM=0x41010000;SHADOW=0x41020000;STACK=0x44000000;TABLE=0x45000000
class CPU(E.CPU64):
 def exec_vector(self,pc,word,mn):
  if pc==0x822e7c90:
   a,b,m=self.vr[13],self.vr[9],self.vr[127];self.vr[127]=tuple((a[i]&(~m[i]&0xffffffff))|(b[i]&m[i]) for i in range(4));return True
  return super().exec_vector(pc,word,mn)
 def step(self,pc,word,nxt,depth):
  if pc in (0x822e7d78,0x822e83a0):self.decision=pc==0x822e7d78;raise E.Return()
  return super().step(pc,word,nxt,depth)
def put32(m,a,v):m.wr(a,struct.pack('>I',v&0xffffffff))
def put64(m,a,v):m.wr(a,struct.pack('>Q',v))
def original(case):
 owner,idx,hide,player,active,render,select,env,cascade,distance=case;m=V.Mem()
 for addr,size in [(MOD,0x20000),(INPUT,0x1000),(CAM,0x1000),(SHADOW,0x2000),(STACK,0x20000),(TABLE,0x100)]:m.add(addr,size)
 c=CPU(m);sp=STACK+0x10000;c.setg(1,sp);put32(m,sp+0x34c,INPUT);put32(m,MOD+0x182f8,player);put64(m,CAM+0x140,4 if hide else 0);put32(m,SHADOW+0x14f4,cascade)
 for i,val in enumerate([0.,64.,144.]):m.wr(TABLE+16*i,struct.pack('>4f',*[val]*4))
 car=MOD+0x1a60+idx*0x1cd0;m.wr(car+0x79d,bytes([render]));m.wr(car+0x79a,bytes([select]));c.setvf(123,[distance,0.,0.,0.]);
 for r,v in [(22,MOD),(29,(owner<<24)|(idx<<10)),(25,0x182f8),(18,0x11100),(21,env),(19,SHADOW),(24,TABLE),(26,48),(31,0),(20,1)]:c.setg(r,v)
 trace=[]
 def transform(cpu):
  trace.append('transform');dst=cpu.g(3)&0xffffffff;assert cpu.g(4)&0xffffffff==car;cpu.mem.wr(dst,bytes(64));cpu.setg(3,dst)
 def camera(cpu):trace.append('camera');cpu.setg(3,CAM)
 def isactive(cpu):trace.append('active');assert cpu.g(3)&0xffffffff==car;cpu.setg(3,active)
 def begin(cpu):trace.append('assert_begin')
 def fire(cpu):trace.append('assert_fire')
 def end(cpu):trace.append('assert_end')
 for a,fn in [(0x822b69c8,camera),(0x82817548,begin),(0x82820810,fire),(0x82817558,end)]:c.hooks[a]=fn
 # Resolve the two original direct targets from the image, avoiding guessed names.
 import x360rd
 for site,fn in [(0x822e7c38,transform),(0x822e7d3c,isactive)]:
  w=x360rd.u32(site);delta=w&0x03fffffc
  if delta&0x02000000:delta-=0x04000000
  c.hooks[(site+delta)&0xffffffff]=fn
 c.decision=None;c.run(0x822e7c20);assert c.decision is not None;return c.decision,trace,(c.g(21)&0xff if c.decision else None)
def model(case):
 owner,idx,hide,player,active,render,select,env,cascade,distance=case;trace=['transform']
 if env and [0.,64.,144.][cascade] > distance*distance:return False,trace,None
 if owner!=1:trace.extend(['assert_begin','assert_fire','assert_end'])
 trace.append('camera');visible=not(hide and idx==(player&0xffff));trace.append('active')
 if not active or not render or select:return False,trace,None
 return True,trace,int(visible)
rng=random.Random(3601031);cases=[]
for owner,hide,active,render,select,env,cascade,dist in itertools.product([1,33],[0,1],[0,1],[0,1],[0,1],[0,1],[0,1,2],[0.,8.,12.,20.]):
 idx=rng.randrange(8);player=idx if rng.randrange(2) else (idx+1)%8;cases.append((owner,idx,hide,player,active,render,select,env,cascade,dist))
errors=[]
for i,case in enumerate(cases):
 try:a=original(case);b=model(case)
 except Exception as ex:print('ERROR',i,case,repr(ex));raise
 if a!=b:errors.append({'case':case,'original':a,'model':b})
res={'cases':len(cases),'mismatches':errors[:20],'total_mismatches':len(errors),'original':'822E7C20..822E7D74','scope':'Actual ARTIST live-visible distance/cull, owner assertion, camera hide, ActiveRaceCar::IsActive and render/car-select gates, ordered call hooks; finite axis-aligned positions. Does not execute resource, shadow/corona, RenderRaceCar or hardware.','notable':'Distance and shadow cull precede owner assertion; invalid owner asserts non-gating. Camera hide suppresses attached geometry, not the render path.'};(OUT/'live-visible-gates-original-comparison.json').write_text(json.dumps(res,indent=2),encoding='utf8');print('cases',len(cases),'mismatches',len(errors));assert not errors,errors[:3]
