from pathlib import Path
import json,sys,struct,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/gamestate-gui-c0d-terra');j=json.loads((O/'wip2/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('CgsAptCallbackRender.cpp')));A=F.M.S.AUX;AUX=A+0x1000;DATA=A+0x4000;BUF=A+0x5000;VT=A+0x6000;VSTRING=0x84f00000
class Stop(Exception):pass
def put(c,a,v):c.mem.wr(a,struct.pack('>I',v&0xffffffff))
def run(name,entry,compiled,invalid):
 sym='?'+name+'@AptCallbackRender@CgsGui@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);c=F.cpu(b);c.mem.add(0x8305a000,0x1000);c.mem.add(0x82f32000,0x1000);put(c,0x82f32260,BUF);put(c,0x82f32264,256);put(c,VT+4,VSTRING);put(c,0x8305a6c8,0xfffffbe0 if invalid=='handler' else AUX);c.mem.wr(DATA,struct.pack('>10f',*([1.0]*10)));c.setg(3,0 if invalid=='input' else DATA);trace=[]
 def stream(cc):
  text=F.cstring(cc,cc.g(4)&0xffffffff);dest=cc.rd32(cc.g(3)+8);cc.mem.wr(dest,text.encode()+b'\0');cc.setg(3,cc.g(3))
 def effect(cc,n):
  if 'BeginAssert' in n:trace.append('begin')
  elif 'FireAssert' in n:trace.append(('fire',cc.g(5),F.cstring(cc,cc.g(3)&0xffffffff)))
  elif 'EndAssert' in n:trace.append('end');raise Stop()
  elif 'StrStream@' in n and '??0' in n:put(cc,cc.g(3),VT);put(cc,cc.g(3)+8,BUF);put(cc,cc.g(3)+12,256)
  elif 'BasePriorityQueue__Clear' in n:pass
  else:raise AssertionError(('unexpected',n))
 if compiled:
  for at,n in b.unresolved.items():
   if 'mpAptAuxInst' in n:put(c,at,0xfffffbe0 if invalid=='handler' else AUX)
   elif 'gpcMessageBuffer' in n:put(c,at,BUF)
   elif 'giMessageBufferSize' in n:put(c,at,256)
   else:c.hooks[at]=lambda cc,nn=n:effect(cc,nn)
  c.hooks[VSTRING]=stream
  for n,(oi,v,sec) in b.definitions.items():
   if (oi,sec) in b.bases and '??0StrStream@' in n:c.hooks[b.bases[(oi,sec)]+v]=lambda cc,nn=n:effect(cc,nn)
  e=b.entry
 else:
  c.hooks[0x82817548]=lambda cc:effect(cc,'BeginAssert');c.hooks[0x82820810]=lambda cc:effect(cc,'FireAssert');c.hooks[0x82817558]=lambda cc:effect(cc,'EndAssert');c.hooks[0x82815e58]=lambda cc:effect(cc,'BasePriorityQueue__Clear');c.hooks[F.M.V.x360rd.u32(0x82000d0c)]=stream;e=entry
 try:c.run(e)
 except Stop:pass
 return trace
out=[]
for name,entry in [('SetVertexMatrix',0x8284b838),('SetColourTransform',0x82851cb8)]:
 for invalid in ['input','handler']:
  a=run(name,entry,False,invalid);z=run(name,entry,True,invalid);assert a==z,(name,invalid,a,z);out.append({'method':name,'invalid':invalid,'trace':a})
r={'cases':len(out),'results':out,'scope':'Original versus compiled first null-argument and null-derived-handler assertion Begin/Fire/End, including exact streamed message and source line; stop at End before invalid dereference. Stream and assertion functions controlled.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'apt-assert-comparison-wip2.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))

