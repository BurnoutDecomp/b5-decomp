from pathlib import Path
import hashlib,json,math,random,re,struct,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'));sys.path.insert(0,str(parent/'tools/re'));import native_atomic_fixture as F;import x360rd as R
out=parent/'b5-decomp/build/x360/parallel/native_textures/aspect-correction3';paths={Path(r['source']).name:Path(r['object']) for r in json.loads((out/'compile-results.json').read_text())['results']}
bundle=F.Bundle([F.Coff(paths['CgsIm2dTransform.cpp'],'?SetAspectCorrectionMatrix@'),F.Coff(paths['RwVector3TemplateFloat.cpp'],'??0?$Vector3Template@M')]);bundle.select('?SetAspectCorrectionMatrix@')
def branch(pc):
 w=R.u32(pc);d=w&0x3fffffc
 return (pc+d-(1<<26 if d&(1<<25) else 0))&0xffffffff
save25=branch(0x827f98cc);rest25=branch(0x827f9b94)
def bind():
 for (oi,sec),base in bundle.bases.items():
  obj=bundle.objects[oi];body=bundle.images[base]
  for at,index,kind in obj.relocations(sec):
   name,_,_=obj.symbols[index];address=None
   for prefix,a in [('?mgDefault@',0x830112d0),('?mgAspectCorrected@',0x83011090),('?mgAspectRatioCorrection@',0x830110f0)]:
    if name.startswith(prefix):address=a
   m=re.fullmatch(r'__(save|rest)fpr_(\d+)',name)
   if m:address=(save25 if m[1]=='save' else rest25)+4*(int(m[2])-25)
   if address is None:continue
   w=struct.unpack_from('>I',body,at)[0]
   if kind in [0x10,0x11]:w=(w&0xffff0000)|(((address+0x8000)>>16 if kind==0x10 else address)&0xffff)
   elif kind==2:w=address
   elif kind==6:w=(w&0xfc000003)|((address-base-at)&0x3fffffc)
   else:raise AssertionError((name,kind))
   struct.pack_into('>I',body,at,w)
# Check the actual initialized storage against the entire native static initializer.
default_compiled=F.address(bundle,'?mgDefault@');c=F.cpu(bundle);default_bytes=c.mem.rd(default_compiled,64);c.mem.add(0x83011000,0x1000);c.run(0x82c6c3e8);assert c.mem.rd(0x830112d0,64)==default_bytes
bind();SELF=F.M.S.SELF;AUX=F.M.S.AUX;rng=random.Random(0x827f98c0);errors=[]
def float_bytes(values):return struct.pack('>'+str(len(values))+'f',*values)
for i in range(384):
 kind=i%6;matrix=[rng.randint(-40,40)*0.125 for n in range(9)];default=[rng.randint(-40,40)*0.125 for n in range(16)];corrected=[rng.randint(-40,40)*0.125 for n in range(16)];initial_matrix=[rng.randint(-40,40)*0.125 for n in range(9)]
 if kind in [1,2]:matrix[(i//6)%9]=float('nan')
 if kind==3:default[[0,1,4,5,6,7][(i//6)%6]]=float('nan')
 if kind==4:matrix=[1.0,0.0,-0.0,0.0,0.75,0.0,0.0,0.0,1.0];default=[-1,1,0,0,2,0,0,-2,0,0,0,0,1,1,1,1]
 if kind==5:matrix[8]=float('inf')
 both=[]
 for compiled in [False,True]:
  c=F.cpu(bundle);c.mem.add(0x83011000,0x1000);c.mem.wr(0x830112d0,float_bytes(default));c.mem.wr(0x83011090,float_bytes(corrected));c.mem.wr(0x830110f0,float_bytes(initial_matrix));input_=0x830110f0 if i%7==0 else SELF;c.mem.wr(input_,float_bytes(matrix));c.setg(3,input_);trace=[];stage=[None]
  def begin(x):trace.append(('begin',))
  def fire(x):
   message=F.cstring(x,x.g(3)&0xffffffff);stage[0]=message;trace.append(('assert',message,F.cstring(x,x.g(4)&0xffffffff),x.g(5)&0xffffffff,x.mem.rd(0x830110f0,36).hex(),x.mem.rd(0x83011090,64).hex()))
  def end(x):
   trace.append(('end',))
   if stage[0]=='IsValid(lAspectRatioCorrection)' and kind==1:x.mem.wr(input_,float_bytes([1,0,0,0,1,0,0,0,1]))
   if stage[0]=='IsValid(lMatrix)' and i%2:x.mem.wr(input_,float_bytes([2,0,0,0,0.5,0,0.125,-0.25,1]))
   if i%4==1:
    F.put(x,0x83011098,0x80000000);F.put(x,0x830112f0,0x12345678+i)
  c.hooks.update({0x82817548:begin,0x82820810:fire,0x82817558:end})
  for at,name in bundle.unresolved.items():
   if name.startswith('?BeginAssert@'):c.hooks[at]=begin
   elif name.startswith('?FireAssert@'):c.hooks[at]=fire
   elif name.startswith('?EndAssert@'):c.hooks[at]=end
  c.run(bundle.entry if compiled else 0x827f98c0);both.append((c.mem.rd(input_,36),c.mem.rd(0x830110f0,36),c.mem.rd(0x83011090,64),c.mem.rd(0x830112d0,64),trace))
 if both[0]!=both[1]:errors.append(dict(case=i,kind=kind,differences=[[hex(n) for n,(a,b) in enumerate(zip(both[0][part],both[1][part])) if a!=b] for part in range(4)],traces=[x[-1] for x in both]))
result=dict(cases=384,errors=errors,objects={n:dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for n,p in paths.items()},scope='Whole actual static SetAspectCorrectionMatrix versus ARTIST827F98C0: complete stored matrix and corrected/default transforms, all three validation/assert sites, aliased matrix input, finite/signed-zero/NaN/infinity cases, late input/default/output mutations. Actual native math/validity and compiled FPU constructors execute. Default transform bytes match whole native initializer82C6C3E8.',limits='Assert services are controlled; shared VPU type ABI is unchanged. Genuine CRT save/rest FPR relocations bind to the actual original helpers. No boot/GPU claim.')
(out/'aspect-correction-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('384 whole aspect setter cases; errors',len(errors));print(errors[:2]);raise SystemExit(bool(errors))
