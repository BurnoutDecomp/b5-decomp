from pathlib import Path
import hashlib,json,random,struct,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'));import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/renderer-texture-scope1';reports=json.loads((out/'compile-results.json').read_text())['results'];p=Path(next(x['object'] for x in reports if x['source'].endswith('native_texture_scope_setters.cpp')));objects={k:F.Coff(p,'NativeTextureScopeSet'+k) for k in ['Data','State']};rng=random.Random(0x83011A78);reg=0x83011A78;SELF=F.M.S.SELF;AUX=F.M.S.AUX;errors=[]
class Stop(Exception):pass
specs=[('Data',2,0x8240BE40,0x8240BE60),('State',2,0x8240BE60,0x8240BE7C),('Data',4,0x8240BE88,0x8240BEA0),('State',4,0x8240BEA0,0x8240BEBC)]
for kind,purpose,start,end in specs:
 for i in range(128):
  image=bytearray(rng.randbytes(0xc4));member=purpose*0x1c+(0xc if kind=='Data' else 0x10);old=rng.getrandbits(32);new=old if i%4==0 else (0 if i%4==1 else rng.getrandbits(32));struct.pack_into('>I',image,member,old);both=[]
  for compiled in [False,True]:
   c=F.cpu(objects[kind]);c.mem.add(0x83011000,0x2000);c.mem.wr(reg,image);c.setg(31,SELF);c.setg(30,0);c.setg(27,reg);F.put(c,SELF+(0x1724 if purpose==2 else 0x1644),new)
   if compiled:c.setg(3,reg);c.setg(4,purpose);c.setg(5,new);c.run(objects[kind].entry)
   else:
    c.setg(3,new)
    def observe(pc,word):
     if pc==end:raise Stop()
    c.instruction_observer=observe
    try:c.run(start)
    except Stop:pass
   both.append(c.mem.rd(reg,0xc4))
  if both[0]!=both[1]:errors.append(dict(kind=kind,purpose=purpose,case=i,different_bytes=[hex(n) for n,(a,b) in enumerate(zip(both[0],both[1])) if a!=b]))
  assert both[1][purpose*0x1c+0x18:purpose*0x1c+0x1c]==image[purpose*0x1c+0x18:purpose*0x1c+0x1c]
# The actual AddTexturePurpose body retains the source name and the unused copied allocation.
p2=Path(next(x['object'] for x in reports if x['source'].endswith('CgsTextureScopeTable.cpp')));b=F.Bundle([F.Coff(p2,'?AddTexturePurpose@TextureScopeTable@')]);b.select('?AddTexturePurpose@TextureScopeTable@')
for i in range(128):
 image=rng.randbytes(0xc4);purpose=i%7;scope=[-1,0,1,2,3][i%5];name=('scope_%d_'%i).encode()+bytes(65+n%26 for n in range(i%37))+b'\0';both=[]
 for compiled in [False,True]:
  c=F.cpu(b);c.mem.wr(SELF,image);c.mem.wr(AUX+0x1000,name);c.mem.wr(AUX+0x2000,b'\xa5'*128);c.setg(3,SELF);c.setg(4,purpose);c.setg(5,AUX+0x1000);c.setg(6,scope);trace=[]
  def allocate(x):trace.append(('allocate',x.g(3)&0xffffffff));x.setg(3,AUX+0x2000)
  c.hooks[0x82C08C00]=allocate
  for at,symbol in b.unresolved.items():
   if symbol.startswith('??_U@') or symbol.startswith('??2@'):c.hooks[at]=allocate
  c.run(b.entry if compiled else 0x827EC180);both.append((c.mem.rd(SELF,0xc4),c.mem.rd(AUX+0x2000,128),trace))
 if both[0]!=both[1]:errors.append(dict(kind='AddTexturePurpose',case=i))
# Constructor argument/order proof uses AddTexturePurpose as a separately verified boundary.
b.select('??0TextureScopeTable@')
for i in range(64):
 image=rng.randbytes(0xc4);both=[]
 for compiled in [False,True]:
  c=F.cpu(b);c.mem.wr(SELF,image);c.setg(3,SELF);trace=[]
  def add(x):trace.append((x.g(3)&0xffffffff,x.g(4)&0xffffffff,F.cstring(x,x.g(5)&0xffffffff),x.g(6)&0xffffffff));x.setg(3,0x12345678)
  c.hooks[0x827EC180]=add;c.hooks[F.address(b,'?AddTexturePurpose@TextureScopeTable@')]=add
  c.run(b.entry if compiled else 0x827EC260);both.append((trace,c.g(3)&0xffffffff,c.mem.rd(SELF,0xc4)))
 if both[0]!=both[1]:errors.append(dict(kind='constructor',case=i,traces=[x[0] for x in both]))
result=dict(cases=704,errors=errors,objects={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in [p,p2]},scope='512actual setter cases against4native inline windows,128wholeAddTexturePurposecases against827EC180,64constructorcall-ordercases against827EC260;all196registrybytes,cache+18preserved,unusednamecopyretained',limits='setter proof coversvalidpurposes2/4;allocationiscontrolled;constructor Add body iscontrolledandreprovedseparately;noGPUorMaterialdispatchclaim')
(out/'texture-scope-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('704 native texture-scope cases; errors',len(errors));print(errors[:3]);raise SystemExit(bool(errors))
