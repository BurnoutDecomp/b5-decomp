from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
class ImpulseCPU(F.AtomicCPU):
 def exec_vector(self,a,w,mn):
  if w >> 26 == 5 and w & 0x3d0 == 0x250:
   vd=((w>>21)&31)|(((w>>2)&3)<<5);va=((w>>16)&31)|(((w>>5)&1)<<5)|(((w>>10)&1)<<6);vb=((w>>11)&31)|((w&3)<<5)
   self.vr[vd]=tuple(self.vr[va][i] & (~self.vr[vb][i]&0xffffffff) for i in range(4));return True
  return super().exec_vector(a,w,mn)
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=p/'14.obj';n='?ApplySensorImpulse@DeformableObject@';o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);A=0x40000000;VEH=A+0x8000;VT=A+0xa000;S=A+0xb000;SVT=A+0xb800;SP=A+0xc000;CONTACT=A+0xd000;PARAM=A+0xe000;FUNC=0x84700000;rng=random.Random(0x826078b0);bad=[]
def run(comp,normal,impulse,mag,sensor,params,spy):
 c=F.cpu(b);c.__class__=ImpulseCPU;c.mem.add(A,0x10000);c.mem.add(0x82fb7000,0x4000);c.mem.add(0x82f2a000,0x1000);c.mem.wr(A+0x194c,VEH.to_bytes(4,'big'));c.mem.wr(A+0x6710,bytes(8));c.mem.wr(VEH,VT.to_bytes(4,'big'));c.mem.wr(S,sensor);c.mem.wr(S,SVT.to_bytes(4,'big'));c.mem.wr(S+0x19c,SP.to_bytes(4,'big'));c.mem.wr(SP,struct.pack('>4f',0,0,0,.5));c.mem.wr(CONTACT+0x20,normal);c.mem.wr(CONTACT+0x30,bytes(8));c.mem.wr(PARAM,params);c.mem.wr(VEH+0x10,struct.pack('>16f',1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1));c.mem.wr(A+0x66f0,struct.pack('>4f',-2,-2,-2,0));c.mem.wr(A+0x6700,struct.pack('>4f',2,2,2,0));c.mem.wr(0x82f2a346,b'\0');c.mem.wr(F.address(b,'kbAllowDriveTimeDeformation'),b'\0')
 for addr,arr in [(0x82fb9520,[.2]*4),(0x82fb9720,[.0001]*4),(0x82fb9d40,[0]*4)]:c.mem.wr(addr,struct.pack('>4f',*arr))
 for name,vals in [('KVF_DRIVE_TIME_DEFORMATION',[.2]*4),('KVF_APPLY_MIN_TANGENTIAL_SQ',[.0001]*4)]:
  try:c.mem.wr(F.address(b,name),struct.pack('>4f',*vals))
  except AssertionError:pass
 for off in [0x10,0x14,0x24]:c.mem.wr(VT+off,(FUNC+off).to_bytes(4,'big'));c.hooks[FUNC+off]=lambda cpu:cpu.setg(3,0)
 calls=[]
 def apply(cpu):calls.append(cpu.mem.rd(cpu.g(4)&0xffffffff,192).hex())
 c.mem.wr(SVT,(FUNC+0x100).to_bytes(4,'big'));c.hooks[FUNC+0x100]=apply
 def copy(cpu):cpu.mem.wr(cpu.g(3)&0xffffffff,cpu.mem.rd(cpu.g(4)&0xffffffff,cpu.g(5)&0xffffffff));cpu.setg(3,cpu.g(3))
 def ctor(cpu):
  a=cpu.g(3)&0xffffffff;v=cpu.mem.rd(cpu.g(4)&0xffffffff,4);cpu.mem.wr(a,v*4)
 c.hooks[0x82c0a630]=copy
 for k,v in b.unresolved.items():
  if v=='memcpy':c.hooks[k]=copy
  elif '??0VecFloat@' in v:c.hooks[k]=ctor
  elif 'GetVehiclePhysics@DeformableObject' in v:c.hooks[k]=lambda cpu:cpu.setg(3,VEH)
  elif v.startswith('__save') or v.startswith('__rest') or 'Assert' in v:c.hooks[k]=lambda cpu:None
 for i,vec in enumerate([struct.pack('>4f',.016,.016,.016,.016),bytes(16),impulse,mag],1):c.vr[i]=struct.unpack('>4I',vec)
 c.setg(3,A);c.setg(4,CONTACT);c.setg(5,PARAM);c.setg(6,S);c.setg(7,spy);c.run(b.entry if comp else 0x826078b0);return c.mem.rd(S+0x10,16).hex(),c.mem.rd(S+0x120,32).hex(),c.mem.rd(A+0x6728,1).hex(),calls
for i in range(32):
 rnd=lambda n:struct.pack('>'+str(n)+'f',*[rng.uniform(-3,3) for _ in range(n)])
 normal=struct.pack('>4f',0,1,0,0);impulse=rnd(4);mag=struct.pack('>4f',*[rng.uniform(0,5)]*4);sensor=bytearray(rng.randbytes(0x200));sensor[0x10:0x20]=rnd(4);sensor[0x120:0x140]=rnd(8);params=bytearray(rng.randbytes(192));params[0xb8]=1
 for spy in (0,1):
  x=run(False,normal,impulse,mag,sensor,params,spy);y=run(True,normal,impulse,mag,sensor,params,spy)
  if x!=y:bad.append({'i':i,'spy':spy,'original':x,'compiled':y});break
 if bad:break
print('mismatches',len(bad),bad[:1]);(p/'update-impulse-comparison.json').write_text(json.dumps({'cases':64,'mismatches':bad,'scope':'whole ApplySensorImpulse 826078B0: finite zero relative motion, world contact, not crashing/showtime, random impulses, spy off/on; controlled virtual/services, compares sensor latch and spy vector bytes, deformed flag, ordered full 192-byte dispatched params; no tangential branch, real services, nonfinite or hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
