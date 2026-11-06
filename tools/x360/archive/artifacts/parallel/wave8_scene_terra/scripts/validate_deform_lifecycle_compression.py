from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=p/'13.obj';n='?GetInitialCompressionScalesAndLimits@DeformableObject@';o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);A=0x40000000;VEH=A+0x8000;ATTR=A+0xa000;R=A+0xc000;rng=random.Random(0x825df6f8);bad=[]
def run(comp,typ,dt,limits,initial):
 c=F.cpu(b);c.mem.add(A,0x10000);c.mem.add(0x82fb9000,0x1000);c.mem.wr(A+0x194c,VEH.to_bytes(4,'big'));c.mem.wr(VEH+0x720,ATTR.to_bytes(4,'big'));c.mem.wr(ATTR+0x40,limits);c.mem.wr(R,initial)
 for name,addr,values in [('EVENT',0x82fb9540,[1,1,1,.8]),('CAR_SELECT',0x82fb9db0,[.8,.7,.8,.75]),('DEFAULT',0x82fb9760,[0,0,0,0])]:
  data=struct.pack('>4f',*values);c.mem.wr(addr,data)
  for sn,val,sec in o.symbols.values():
   if sn=='KV3P_'+name+'_COMPRESSION_SCRATCH_RATIO' and (0,sec) in b.bases:c.mem.wr(b.bases[(0,sec)]+val,data)
 for k,v in b.unresolved.items():
  if 'Assert' in v or v.startswith('__save') or v.startswith('__rest'):c.hooks[k]=lambda cpu:None
 c.setg(3,A);c.setg(4,typ);c.setg(5,R);c.setg(6,R+16);c.setg(7,R+32);c.vr[1]=struct.unpack('>4I',dt);c.run(b.entry if comp else 0x825df6f8);return c.mem.rd(R,48).hex()
for i in range(128):
 rnd=lambda n:struct.pack('>'+str(n)+'f',*[rng.uniform(-4,4) for _ in range(n)])
 dt,limits,initial=rnd(4),rnd(4),rnd(12)
 for typ in [0,1,2]:
  x=run(False,typ,dt,limits,initial);y=run(True,typ,dt,limits,initial)
  if x!=y:bad.append({'i':i,'type':typ,'original':x,'compiled':y});break
 if bad:break
print('mismatches',len(bad),bad[:1]);(p/'lifecycle-compression-comparison.json').write_text(json.dumps({'cases':384,'mismatches':bad,'scope':'whole GetInitialCompressionScalesAndLimits original 825DF6F8 vs current whole object; finite distinct damage lanes, three reset types, initialized original ratio globals, valid attributes, compares all 48 output bytes; no nonfinite/hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
