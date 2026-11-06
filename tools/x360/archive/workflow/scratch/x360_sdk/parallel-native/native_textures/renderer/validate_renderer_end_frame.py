from pathlib import Path
import hashlib,json,random,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'));import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/renderer-checkpoint1';r=next(x for x in json.loads((out/'compile-results.json').read_text())['results'] if x['source'].endswith('BrnRendererModule.cpp'));p=Path(r['object']);bundle=F.Bundle([F.Coff(p,'?EndOfFrame@BrnRendererModule@@')]);bundle.select('?EndOfFrame@BrnRendererModule@@');SELF=F.M.S.SELF;rng=random.Random(0x823FFE28);errors=[]
for i in range(512):
 image=rng.randbytes(0xca80);stage=[0,1,2,3,0xffffffff,0x80000000,0,1][i%8];count=[0,1,2,0xffffffff,0x80000000,0x7fffffff,rng.getrandbits(32)][i%7];stalled=(i//8)&1;argument=(rng.getrandbits(24)<<8)|stalled;both=[]
 for compiled in [False,True]:
  c=F.cpu(bundle);c.mem.wr(SELF,image);F.put(c,SELF+0xc574,stage);F.put(c,SELF+0xc578,count);c.mem.wr(SELF+0xc464,bytes([(i//16)&1]));c.mem.wr(SELF+0xc465,bytes([(i//32)&1]));c.setg(3,SELF);c.setg(4,argument);trace=[]
  def swap(x):
   trace.append((x.g(3)&0xffffffff,hashlib.sha256(x.mem.rd(SELF,0xca80)).hexdigest()))
   F.put(x,SELF+0xc44c,i+7)
   if i%3==1:x.mem.wr(SELF+0xc464,b'\1')
   elif i%3==2:x.mem.wr(SELF+0xc464,b'\0')
  c.hooks[0x823fc678]=swap
  for at,name in bundle.unresolved.items():
   if name.startswith('?SwapBuffers@BrnRendererModule@@'):c.hooks[at]=swap
  c.run(bundle.entry if compiled else 0x823ffe28);both.append((c.mem.rd(SELF,0xca80),trace))
 if both[0]!=both[1]:errors.append(dict(case=i,stage=hex(stage),count=hex(count),stalled=stalled,different_bytes=[hex(n) for n,(a,b) in enumerate(zip(both[0][0],both[1][0])) if a!=b],traces=[x[1] for x in both]))
result=dict(cases=512,errors=errors,object=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),scope='whole actual Renderer::EndOfFrame versus ARTIST823FFE28:stallstate/countdown,32bitwrap,8bitboolABIwithundefinedupperregisterbits,screenshottransferandpost-swaprequestreads,allCA80bytesandpre-swapstate',limits='SwapBuffersisacontrolledboundary;nativewholeSwapBuffersbodyremainsseparatefirst-framework')
(out/'renderer-end-frame-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('512 native EndOfFrame cases; errors',len(errors));print(errors[:3]);raise SystemExit(bool(errors))
