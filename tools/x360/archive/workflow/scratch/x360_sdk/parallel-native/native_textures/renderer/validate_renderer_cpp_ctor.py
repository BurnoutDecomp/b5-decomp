from pathlib import Path
import hashlib,json,random,struct,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0,str(parent/'scratch/x360_sdk'))
import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/renderer-construct2'
deps=parent/'b5-decomp/build/x360/parallel/native_textures/renderer-ctor-dependencies1'
paths={Path(r['source']).name:Path(r['object']) for d in [out,deps] for r in json.loads((d/'compile-results.json').read_text())['results'] if r['passed']}
objects=[F.Coff(paths['BrnRendererModule.cpp'],'??0BrnRendererModule@@'),F.Coff(paths['CgsBufferedDispatchFrame.cpp'],'??0BufferedDispatchFrame@'),F.Coff(paths['CgsDepthStencilStateFactory.cpp'],'??0CgsDepthStencilStateFactory@@')]
bundle=F.Bundle(objects);bundle.select('??0BrnRendererModule@@')
vtables={
'??_7Module@CgsModule@@6B@':0x820CE500,
'??_7BrnRendererModule@@6B@':0x820CFEB0,
'??_7BufferedDispatchFrame@CgsGraphics@@6B@':0x820D5174,
'??_7Im2dRenderBuffer@CgsGraphics@@6B@':0x820CDCC0,
'??_7Im2d@CgsGraphics@@6B@':0x820CF694,
'??_7Im2dUntex@CgsGraphics@@6B@':0x820CF698,
'??_7Im3dRenderBuffer@CgsGraphics@@6B@':0x820CE578,
'??_7Im3d@CgsGraphics@@6B@':0x820CF69C,
'??_7Im3dRenderBufferUntex@CgsGraphics@@6B@':0x820CE5FC,
'??_7Im3dUntex@CgsGraphics@@6B@':0x820CF6A0,
'??_7Im3dSkyDome@BrnGraphics@@6B@':0x820CF6A4,
'??_7Im3dZOnly@CgsGraphics@@6B@':0x820CF6A8,
'??_7CgsBlendStateFactory@@6B@':0x820CDC1C,
'??_7CgsRasterizerStateFactory@@6B@':0x820CDC28,
'??_7CgsDepthStencilStateFactory@@6B@':0x820CDC34,
'??_7DebugComponent@BrnGraphics@@6B@':0x820CE600,
}
for (oi,sec),base in bundle.bases.items():
 obj=bundle.objects[oi];body=bundle.images[base]
 for at,index,kind in obj.relocations(sec):
  name,_,_=obj.symbols[index]
  if name not in vtables:continue
  address=vtables[name];word=struct.unpack_from('>I',body,at)[0]
  if kind in [0x10,0x11]:word=(word&0xffff0000)|(((address+0x8000)>>16 if kind==0x10 else address)&0xffff)
  elif kind==2:word=address
  else:raise AssertionError((name,kind))
  struct.pack_into('>I',body,at,word)
rng=random.Random(0x827DFF48);errors=[];SELF=F.M.S.SELF
for i in range(64):
 image=rng.randbytes(0xca80);both=[]
 for compiled in [False,True]:
  c=F.cpu(bundle);c.mem.wr(SELF,image);c.setg(3,SELF);trace=[]
  def lifecycle(kind):
   def run(x):
    address=x.g(3)&0xffffffff;args=[x.g(n)&0xffffffff for n in range(4,6 if kind=='mutex' else 5)]
    trace.append((kind,address-SELF,args,hashlib.sha256(x.mem.rd(SELF,0xca80)).hexdigest()))
    size=0x108 if kind=='mutex' else 0x350
    x.mem.wr(address,bytes(((k+17*len(trace)+i)&255 for k in range(size))));x.setg(3,address)
   return run
  c.hooks[0x82B43CA0]=lifecycle('mutex');c.hooks[0x82BCB0C0]=lifecycle('job')
  for at,name in bundle.unresolved.items():
   if name.startswith('??0RWMutex@'):c.hooks[at]=lifecycle('mutex')
   elif name.startswith('??0Job@'):c.hooks[at]=lifecycle('job')
  c.run(bundle.entry if compiled else 0x827DFF48)
  both.append((c.mem.rd(SELF,0xca80),trace,c.g(3)&0xffffffff))
 if both[0]!=both[1]:
  diff=[n for n,(a,b) in enumerate(zip(both[0][0],both[1][0])) if a!=b]
  errors.append(dict(case=i,different_bytes=diff,trace_first_difference=next((n for n,(a,b) in enumerate(zip(both[0][1],both[1][1])) if a!=b),None),traces=[v[1] for v in both]))
result=dict(cases=64,errors=errors,vtables={n:hex(a) for n,a in vtables.items()},objects={n:dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for n,p in paths.items()},scope='whole native compiled Renderer C++ ctor versus ARTIST827DFF48, all CA80 bytes including uninitialized regions and every pre-call snapshot; actual child buffered/factory C++ constructors linked',limits='mutex and Job constructors are controlled lifecycle services; vtable data operands are bound through genuine COFF relocations to original native addresses')
(out/'renderer-cpp-constructor-comparison.json').write_text(json.dumps(result,indent=2)+'\n')
print('64 whole Renderer C++ constructor comparisons; errors',len(errors));print([dict(case=e['case'],different_bytes=[hex(n) for n in e['different_bytes']],trace_first_difference=e['trace_first_difference']) for e in errors[:2]])
raise SystemExit(bool(errors))
