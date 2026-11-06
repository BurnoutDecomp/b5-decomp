from pathlib import Path
import hashlib,json,random,struct,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'));sys.path.insert(0,str(parent/'tools/re'));import native_atomic_fixture as F;import x360rd as R
out=parent/'b5-decomp/build/x360/parallel/native_textures/texture-size-abi3';p=Path(next(x['object'] for x in json.loads((out/'compile-results.json').read_text())['results'] if x['source'].endswith('texture.cpp')));bundle=F.Bundle([F.Coff(p,'?GetPixelDataSize@Texture@')]);bundle.select('?GetPixelDataSize@Texture@');helper=F.address(bundle,'Xbox2SetTextureHeader');w=R.u32(0x82B610BC);delta=w&0x03fffffc;original_helper=(0x82B610BC+delta-(1<<26 if delta&(1<<25) else 0))&0xffffffff;rng=random.Random(0x82B61088);errors=[]
for i in range(128):
 type_=[0,1,2,3,4,0xffffffff][i%6];width=rng.getrandbits(16);height=rng.getrandbits(16);depth=rng.getrandbits(12);format_=[0x18280186,0x18280106,0x1a200152,0x1a200153,rng.getrandbits(32)][i%5];levels=[0,1,2,8,13,0xffffffff][(i//2)%6];both=[]
 for compiled in [False,True]:
  c=F.cpu(bundle);sp=c.g(1)&0xffffffff;c.mem.wr(sp-0x1000,b'\xa5'*0x1000)
  for n,value in enumerate([type_,width,height,depth,format_,levels]):c.setg(3+n,value)
  trace=[]
  def boundary(x):
   a=[x.g(n)&0xffffffff for n in range(3,10)];outptr=x.g(10)&0xffffffff;incoming=int.from_bytes(x.mem.rd(outptr,4),'big');extra=int.from_bytes(x.mem.rd((x.g(1)&0xffffffff)+0x54,4),'big');trace.append((a,incoming,extra));assert a==[0,width,height,depth,levels,format_,type_];assert incoming==0;assert extra==0
   if i%4:F.put(x,outptr,(width^height^depth^(format_<<3)^levels^type_))
   x.setg(3,0xabcdef01)
  c.hooks[original_helper]=boundary;c.hooks[helper]=boundary;c.run(bundle.entry if compiled else 0x82B61088);both.append((c.g(3)&0xffffffff,trace))
 if both[0]!=both[1]:errors.append(dict(case=i,results=both))
result=dict(cases=128,errors=errors,object=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),public_abi='type r3,width r4,height r5,depth r6,format r7,levels r8',helper_abi='nullheader r3,width r4,height r5,depth r6,levels r7,format r8,type r9,sizeout r10,nullpixels stack54',scope='whole actual public GetPixelDataSize versus ARTIST82B61088:allregisterroles,initialsizezero,outputreturn/untouchedhelperresultcases andoutgoingninthargument',limits='Xbox2SetTextureHeaderiscontrolled;itsSDKsizingalgorithmisnotreproved;internaldescriptorcalleronlyneededargument-orderupdate;noGPUclaim')
(out/'texture-size-abi-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('128 native texture-size ABI cases; errors',len(errors));raise SystemExit(bool(errors))
