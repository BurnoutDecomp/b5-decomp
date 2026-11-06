from pathlib import Path
import hashlib,json,random,struct,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'));import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/state-provider-core1';paths={Path(r['source']).name:Path(r['object']) for r in json.loads((out/'compile-results.json').read_text())['results']}
SELF=F.M.S.SELF;AUX=F.M.S.AUX;rng=random.Random(0x82b62890);errors=[];checks=0
for typ,file_,size,psize,init,desc in [('DepthStencilState','depthstencilstate.cpp',0x60,0x4c,0x82b62890,0x82b636f8),('RasterizerState','rasterizerstate.cpp',0x34,0x20,0x82b62958,0x82b63738)]:
 b=F.Bundle([F.Coff(paths[file_],'?Initialize@'+typ+'@'),F.Coff(paths['BaseResourceDescriptor.cpp'],'??0BaseResourceDescriptor@')])
 for action,original in [('Initialize',init),('GetResourceDescriptor',desc)]:
  b.select('?'+action+'@'+typ+'@')
  for i in range(128):
   image=rng.randbytes(0x80);params=rng.randbytes(psize);resource=struct.pack('>5I',SELF,AUX+0x400,AUX+0x800,AUX+0xc00,AUX+0x1000);both=[]
   for compiled in [False,True]:
    c=F.cpu(b);c.mem.wr(SELF,image);c.mem.wr(AUX,params);c.mem.wr(AUX+0x200,resource);c.setg(3,AUX+0x200 if action=='Initialize' else SELF);c.setg(4,AUX);c.run(b.entry if compiled else original);both.append((c.g(3)&0xffffffff,c.mem.rd(SELF,0x80),c.mem.rd(AUX,psize),c.mem.rd(AUX+0x200,20)))
   checks+=1
   if both[0]!=both[1]:errors.append(dict(type=typ,action=action,case=i,returns=[x[0] for x in both],state_differences=[hex(n) for n,(a,b_) in enumerate(zip(both[0][1],both[1][1])) if a!=b_]))
class Stop(Exception):pass
for name,size,entry,stop,offset in [('ConstructDepthParameters',0x4c,0x82b643e4,0x82b64450,0x100),('ConstructRasterParameters',0x20,0x82b64494,0x82b644e4,0x50)]:
 obj=F.Coff(paths['native_depth_raster_parameters.cpp'],name)
 for i in range(128):
  image=rng.randbytes(size);both=[]
  for compiled in [False,True]:
   c=F.cpu(obj);c.mem.add(0x83271000,0x1000);c.mem.wr(SELF,image);c.setg(3,SELF)
   if not compiled:
    c.setg(1,SELF-offset)
    for n,value in [(26,7),(27,1),(28,0x8327187c),(29,15),(30,0xffffffff),(31,0)]:c.setg(n,value)
    def observe(pc,word):
     if pc==stop:raise Stop()
    c.instruction_observer=observe
   try:c.run(obj.entry if compiled else entry)
   except Stop:pass
   both.append(c.mem.rd(SELF,size))
  checks+=1
  if both[0]!=both[1]:errors.append(dict(action=name,case=i,differences=[hex(n) for n,(a,b_) in enumerate(zip(*both)) if a!=b_]))
result=dict(cases=checks,errors=errors,objects={n:dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for n,p in paths.items()},scope='128 whole Initialize and128 whole descriptor cases for each real state, plus128 actual-header ctor cases for each parameter class. Complete state/resource/parameter bytes compare, including arbitrary float-bias bits, noncanonical flags, unused rasterizer word and natural parameter padding.',limits='Parameter constructors compare only their original inline regions inside StateHelper; unrelated global publication stores are outside those two narrow ctor claims. Caller closure is still WIP.')
(out/'depth-raster-native-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(checks,'depth/raster native cases; errors',len(errors));print(errors[:3]);raise SystemExit(bool(errors))
