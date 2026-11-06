from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'camera-final-v1/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('CameraUtils.cpp')));sym='?ApplyPitchAboutPointRads@Utils@Camera@BrnDirector@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;A=F.M.S.AUX
orig=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x822183E0.json'))['assembly'].splitlines()
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
addr=next(target(int(l.split()[0],16)) for l in orig if 'CreateLookAt' in l and '  bl ' in l)
def run(compiled,mat,center,angle,basis):
 c=F.cpu(b);c.mem.wr(S,mat);c.setg(3,S);c.vr[1]=struct.unpack('>4I',center);c.vr[2]=struct.unpack('>4I',angle);trace=[]
 def look(cc):trace.append((tuple(cc.vr[1]),tuple(cc.vr[2])));cc.mem.wr(cc.g(3),basis)
 if compiled:
  for at,n in b.unresolved.items():
   if 'CreateLookAt' in n:c.hooks[at]=look
   elif n.startswith('__save') or n.startswith('__rest'):c.hooks[at]=lambda cc:None
   else:c.hooks[at]=lambda cc,name=n:(_ for _ in ()).throw(AssertionError(name))
  for n,(oi,v,sec) in b.definitions.items():
   if (oi,sec) in b.bases and '?CreateLookAt@Utils@Camera' in n:c.hooks[b.bases[(oi,sec)]+v]=look
  c.run(b.entry)
 else:c.hooks[addr]=look;c.run(0x822183e0)
 return c.mem.rd(S,64),trace
rng=random.Random(0x822183e0)
for i in range(128):
 mat=struct.pack('>16f',*[rng.uniform(-4,4) for _ in range(16)]);center=struct.pack('>4f',*[rng.uniform(-4,4) for _ in range(4)]);v=struct.unpack('>f',struct.pack('>f',rng.uniform(-2,2)))[0];angle=struct.pack('>4f',v,v,v,v);basis=struct.pack('>16f',*[rng.uniform(-2,2) for _ in range(16)]);a=run(False,mat,center,angle,basis);z=run(True,mat,center,angle,basis)
 if a!=z:print('i',i,'trace',a[1],z[1],'diff',[hex(k) for k,(x,y) in enumerate(zip(a[0],z[0])) if x!=y]);raise AssertionError(i)
r={'cases':128,'scope':'Original 822183E0 vs actual compiled ApplyPitchAboutPointRads; Vector3 v1, genuine SDK VecFloat v2, full64-byte matrix and all look-at input lanes compared. CreateLookAt controlled to the same finite random matrix; remaining math executes, finite inputs only.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'apply-pitch-comparison-final-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
