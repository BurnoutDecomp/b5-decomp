from pathlib import Path
import hashlib,json,random,struct,sys
P=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path[:0]=[str(P/'scratch/x360_sdk'),str(P/'tools/re')]
import native_atomic_fixture as F
out=P/'b5-decomp/build/x360/parallel/debug_render_terra/state_cohort'; ctor=P/'b5-decomp/build/x360/parallel/debug_render_terra/state_ctor'
paths={Path(r['source']).name:Path(r['object']) for d in (out,ctor) for r in json.loads((d/'compile-results.json').read_text())['results']}
B=F.Bundle([F.Coff(paths[n],p) for n,p in [('CgsDebug3DImmediateRender.cpp','?CreateDebugRenderStates@'),('blendstate.cpp','?Initialize@BlendState@'),('depthstencilstate.cpp','?Initialize@DepthStencilState@'),('rasterizerstate.cpp','?Initialize@RasterizerState@'),('BaseResourceDescriptor.cpp','??0BaseResourceDescriptor@')]])
B.select('?CreateDebugRenderStates@')
addresses={'?sgBlend@':0x83018efc,'?sgDepth2D@':0x83018f04,'?sgDepth3D@':0x83018f00,'?sgRasterizer@':0x83018f08,'?sbCreated@':0x83018ef8}
for (oi,sec),base in B.bases.items():
 obj=B.objects[oi];body=B.images[base]
 for at,index,kind in obj.relocations(sec):
  name,_,_=obj.symbols[index];matches=[a for n,a in addresses.items() if name.startswith(n)]
  if not matches:continue
  address=matches[0];w=struct.unpack_from('>I',body,at)[0]
  if kind in [0x10,0x11]:w=(w&0xffff0000)|(((address+0x8000)>>16 if kind==0x10 else address)&0xffff)
  elif kind==2:w=address
  else:raise AssertionError((name,kind))
  struct.pack_into('>I',body,at,w)
A=F.M.S.AUX; rng=random.Random(0x82818b68); errors=[]
for i in range(64):
 data=rng.randbytes(0x1000); globals_=rng.randbytes(20); both=[]
 for compiled in (False,True):
  c=F.cpu(B);c.mem.add(0x83018000,0x1000);c.mem.wr(0x83018000,bytes(0x1000));c.mem.wr(0x83018ef8,globals_);c.mem.wr(0x83018ef8,bytes([1 if i%4==0 else 0]));c.mem.wr(A+0x8000,data);c.mem.wr((c.g(1)&0xffffffff)-0x3000,bytes([i])*0x3000)
  F.put(c,A+0x1020,0x84803000);F.put(c,A+0x1000,A+0x1010); c.setg(3,F.M.S.SELF);c.setg(4,A+0x1000)
  trace=[]
  def snap():return c.mem.rd(0x83018ef8,20).hex()
  def allocate(x):
   n=len(trace);trace.append((x.mem.rd(x.g(5)&0xffffffff,40).hex(),x.g(6)&0xffffffff,snap()));
   if n==2 and i%3==0:F.put(x,0x83018f04,0x76543210)
   x.mem.wr(x.g(3)&0xffffffff,struct.pack('>5I',A+0x8000+n*0x200,0xa0000000+n,0xb0000000+n,0xc0000000+n,0xd0000000+n))
  c.hooks[0x84803000]=allocate
  c.run(B.entry if compiled else 0x82818b68)
  both.append((trace,c.mem.rd(0x83018ef8,20),c.mem.rd(A+0x8000,0x800)))
 if both[0]!=both[1]:errors.append({'case':i,'trace':both[0][0],'actual':both[1][0],'globals':[v[1].hex() for v in both],'state_diff':[hex(n) for n,(a,b) in enumerate(zip(both[0][2],both[1][2])) if a!=b][:20]})
result={'cases':64,'errors':errors,'objects':{n:{'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for n,p in paths.items()},'limits':'Controlled allocator writes five resource lanes; compares service descriptors/global snapshots, once-guard early-outs and allocator mutation, resulting five globals and allocated state bytes against ARTIST. No runtime GPU or global vector ABI claim.'}
(out/'debug3d-comparison.json').write_text(json.dumps(result,indent=2));print('cases',64,'errors',len(errors));print(errors[:1]);raise SystemExit(bool(errors))

