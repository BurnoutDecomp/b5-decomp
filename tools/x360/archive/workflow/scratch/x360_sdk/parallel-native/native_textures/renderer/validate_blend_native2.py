from pathlib import Path
import hashlib,json,random,struct,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'));import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/state-provider-core2';paths={Path(r['source']).name:Path(r['object']) for r in json.loads((out/'compile-results.json').read_text())['results']}
bundle=F.Bundle([F.Coff(paths['blendstate.cpp'],'?Initialize@BlendState@'),F.Coff(paths['BaseResourceDescriptor.cpp'],'??0BaseResourceDescriptor@')]);SELF=F.M.S.SELF;AUX=F.M.S.AUX;rng=random.Random(0x82b627c8);errors=[];checks=0
for prefix,original in [('?Initialize@BlendState@',0x82b627c8),('?GetParameters@BlendState@',0x82b60a50),('?GetResourceDescriptor@BlendState@',0x82b636b8)]:
 bundle.select(prefix)
 for i in range(128):
  image=rng.randbytes(0x80);params=bytearray(rng.randbytes(0x38));resource=struct.pack('>5I',SELF,AUX+0x400,AUX+0x800,AUX+0xc00,AUX+0x1000);both=[]
  params[0x30]=[0,1,2,3,128,255][i%6]
  for n in range(0x31,0x37):params[n]=[0,1,2,3,128,255][(i+n)%6]
  if i%4==0:image=struct.pack('>4I',0x10001,0x10001,0x10001,0x10001)+image[16:]
  for compiled in [False,True]:
   c=F.cpu(bundle);c.mem.wr(SELF,image);c.mem.wr(AUX,params);c.mem.wr(AUX+0x200,resource)
   if prefix.startswith('?Initialize@'):c.setg(3,AUX+0x200);c.setg(4,AUX)
   elif prefix.startswith('?GetParameters@'):c.setg(3,SELF);c.setg(4,AUX)
   else:c.setg(3,SELF);c.setg(4,AUX)
   c.run(bundle.entry if compiled else original);both.append((None if prefix.startswith('?GetParameters@') else c.g(3)&0xffffffff,c.mem.rd(SELF,0x80),c.mem.rd(AUX,0x38),c.mem.rd(AUX+0x200,20)))
  checks+=1
  if both[0]!=both[1]:errors.append(dict(method=prefix,case=i,returns=[x[0] for x in both],state_differences=[hex(n) for n,(a,b) in enumerate(zip(both[0][1],both[1][1])) if a!=b],parameter_differences=[hex(n) for n,(a,b) in enumerate(zip(both[0][2],both[1][2])) if a!=b]))
obj=F.Coff(paths['native_blend_parameters.cpp'],'ConstructBlendParameters')
class Stop(Exception):pass
for i in range(128):
 image=rng.randbytes(0x38);both=[]
 for compiled in [False,True]:
  c=F.cpu(obj);c.mem.add(0x83271000,0x1000);c.mem.wr(SELF,image);c.setg(3,SELF)
  if not compiled:
   c.setg(1,SELF-0x190)
   for n,value in [(24,0x7060706),(25,135),(26,7),(27,1),(28,0x8327187c),(29,15),(30,0xffffffff),(31,0)]:c.setg(n,value)
   def observe(pc,word):
    if pc==0x82b641d4:raise Stop()
   c.instruction_observer=observe
  try:c.run(obj.entry if compiled else 0x82b64154)
  except Stop:pass
  both.append(c.mem.rd(SELF,0x38))
 checks+=1
 if both[0]!=both[1]:errors.append(dict(method='Parameters constructor',case=i,differences=[hex(n) for n,(a,b) in enumerate(zip(*both)) if a!=b]))
result=dict(cases=checks,errors=errors,objects={n:dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for n,p in paths.items()},scope='128 whole Initialize,128 whole GetParameters,128 whole resource descriptor cases and128 actual-header parameter constructor cases. All state, descriptor, resource and parameter bytes compare, including noncanonical flag bytes and retained padding. Genuine native base-descriptor constructors execute.',limits='Constructor output compares the original inline parameter region82B64154..41D4 only; unrelated StateHelper global resets are outside that small constructor claim. Direct caller closure and StateHelper Initialize remain uncommitted WIP.')
(out/'blend-native-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(checks,'native Blend cases; errors',len(errors));print(errors[:3]);raise SystemExit(bool(errors))
