import sys,struct,math,json,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
import validate_native_math as M

def matrix(angle,position):
 c,s=math.cos(angle),math.sin(angle)
 return [c,0,-s,0,0,1,0,0,s,0,c,0,*position,0]
def run(a,b,t,compiled):
 c=M.new_cpu();c.mem.wr(M.S.AUX,struct.pack('>16f',*a));c.mem.wr(M.S.AUX+0x100,struct.pack('>16f',*b));c.setg(3,M.S.SELF);c.setg(4,M.S.AUX);c.setg(5,M.S.AUX+0x100)
 if compiled:c.setg(7,M.S.SELF+0x200);c.setf(1,t);address=M.SYMBOLS['NativeSLerp']
 else:c.setg(6,M.S.SELF+0x200);c.setvf(1,[t]*4);address=0x82216858
 c.run(address)
 return struct.unpack('>20f',c.mem.rd(M.S.SELF,64)+c.mem.rd(M.S.SELF+0x200,16))
def rotation(axis,angle,position):
 length=math.sqrt(sum(x*x for x in axis));x,y,z=[v/length for v in axis];c,s=math.cos(angle),math.sin(angle);t=1-c
 return [t*x*x+c,t*x*y+s*z,t*x*z-s*y,0,t*x*y-s*z,t*y*y+c,t*y*z+s*x,0,t*x*z+s*y,t*y*z-s*x,t*z*z+c,0,*position,1]
random.seed(360);pairs=[(matrix(a,[1,2,3]),matrix(b,[4,6,8])) for a,b in [(0,0),(0,.5),(.1,.11),(1,-1),(0,math.pi)]]
pairs += [(matrix(0,[0,0,0]),m) for m in [
 [1,0,0,0,0,-1,0,0,0,0,-1,0,4,6,8,0],
 [-1,0,0,0,0,1,0,0,0,0,-1,0,4,6,8,0],
 [-1,0,0,0,0,-1,0,0,0,0,1,0,4,6,8,0]]]
for i in range(60):
 pairs.append((rotation([random.uniform(-1,1) for _ in range(3)],random.uniform(-math.pi,math.pi),[random.uniform(-100,100) for _ in range(3)]),rotation([random.uniform(-1,1) for _ in range(3)],random.uniform(-math.pi,math.pi),[random.uniform(-100,100) for _ in range(3)])))
checks=0;errors=[]
for pi,(a,b) in enumerate(pairs):
 for t in [-.2,0.,.0001,.2,.5,.95,1.,1.2,M.value(1),math.nan]:
  original=run(a,b,t,False);compiled=run(a,b,t,True);checks+=1
  if not all(M.same(x,y) for x,y in zip(original,compiled)):
   errors.append({'pair':pi,'amount':hex(M.bits(t)),'differences':[{'lane':i,'original':hex(M.bits(x)),'compiled':hex(M.bits(y))} for i,(x,y) in enumerate(zip(original,compiled)) if not M.same(x,y)]})
 if pi%10==0:print('Pairs',pi+1,'checked',flush=True)
(M.OUT/'slerp-comparison.json').write_text(json.dumps({'cases':checks,'mismatches':errors,'hardware_estimates':'interpreter models'},indent=2))
print('cases',checks,'mismatches',len(errors));print(json.dumps(errors[:4],indent=2));raise SystemExit(bool(errors))
