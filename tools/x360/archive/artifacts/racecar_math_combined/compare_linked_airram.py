import sys,struct,random,json,hashlib
from pathlib import Path
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
from validate_resource_pointer import Bundle
class CPU(R.CPU):
 def exec_vector(self,a,w,mn):
  if w>>26==5 and (w & 0x3d0)==0x250:
   vd=((w>>21)&31)|(((w>>2)&3)<<5);va=((w>>16)&31)|(((w>>5)&1)<<5)|(((w>>10)&1)<<6);vb=((w>>11)&31)|((w&3)<<5);self.vr[vd]=tuple((x & ~y)&0xffffffff for x,y in zip(self.vr[va],self.vr[vb]));return True
  return super().exec_vector(a,w,mn)
 def step(self,pc,w,nxt,depth):
  if pc==self.watch:self.reached+=1
  if w>>26==8:
   dst=(w>>21)&31;src=(w>>16)&31;imm=w&65535;imm=imm-65536 if imm&32768 else imm;a=(~self.g(src))&0xffffffffffffffff;total=a+(imm&0xffffffffffffffff)+1;self.ca=int(total>>64!=0);self.setg(dst,total);return nxt
  if w>>26==31 and (w>>1)&1023==136:
   dst=(w>>21)&31;a=self.g((w>>16)&31);b=self.g((w>>11)&31);total=((~a)&0xffffffffffffffff)+b+getattr(self,'ca',0);self.ca=int(total>>64!=0);self.setg(dst,total);return nxt
  if w>>26==31 and (w>>1)&1023==58:
   v=self.g((w>>21)&31)&0xffffffffffffffff;self.setg((w>>16)&31,64-v.bit_length());return nxt
  if w>>26==31 and (w>>1)&1023==27:
   shift=self.g((w>>11)&31)&127;v=self.g((w>>21)&31);self.setg((w>>16)&31,(v<<shift)&0xffffffffffffffff if shift<64 else 0);return nxt
  return super().step(pc,w,nxt,depth)
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_combined');pa=base/'physics/probe.obj';pb=base/'airram_provider/probe.obj';a=R.Coff(pa,'?SetPlayerVehicleInShowtime@RaceCarPhysics@');b=R.Coff(pb,'?AddAirRam@VehiclePhysics@');bundle=Bundle([a,b]);bundle.select('?SetPlayerVehicleInShowtime@RaceCarPhysics@');callee=next(name for name in bundle.definitions if name.startswith('?AddAirRam@VehiclePhysics@'));oi,v,sec=bundle.definitions[callee];compiled_callee=bundle.bases[(oi,sec)]+v;rng=random.Random(91745);mismatches=[]
for case in range(128):
 mass=rng.uniform(500.0,3000.0);half=tuple(rng.uniform(0.1,5.0) for _ in range(3));results=[]
 for compiled in (False,True):
  c=bundle.cpu() if compiled else R.M.new_cpu();c.__class__=CPU;c.ca=0;c.watch=compiled_callee if compiled else 0x825fe118;c.reached=0
  if compiled:
   for addr,name in bundle.unresolved.items():
    if 'fpr_' in name:c.hooks[addr]=lambda cpu:None
  else:
   for addr,file in [(0x826000f8,'826000F8.bin'),(0x825fe118,'825FE118.bin')]:
    data=(base/file).read_bytes();c.mem.add(addr,len(data),data)
  dest=R.M.S.SELF;attr=R.M.S.AUX;c.mem.wr(dest,b'\0'*0x2000);c.mem.wr(attr,b'\0'*0x1000);c.mem.wr(dest+0x720,struct.pack('>I',attr));c.mem.wr(attr+0x70,struct.pack('>f',mass));c.mem.wr(dest+0x6a0,struct.pack('>4f',*half,0.0));c.setg(30,dest);c.setg(26,0);c.setg(27,0x82f2a2c8);c.setg(28,attr+0x100);c.setg(11,case%2);c.setf(31,0.0);c.vr[126]=(0,0,0,0)
  if compiled:
   orig=R.M.new_cpu();c.mem.wr(attr+0x100-0x44,orig.mem.rd(0x82f2a2b0,4));c.mem.wr(attr+0x100-0x48,orig.mem.rd(0x82f2a2ac,4))
  start=bundle.entry+0x254 if compiled else 0x8260049c;end=bundle.entry+0x2a8 if compiled else 0x826004c8;c.mem.wr(end,bytes.fromhex('4e800020'))
  try:c.run(start)
  except Exception as e:print('ERROR',case,compiled,type(e).__name__,e);raise
  assert c.reached==1,(compiled,c.reached)
  results.append(c.mem.rd(dest+0x1158,0x38))
 if results[0]!=results[1]:mismatches.append({'case':case,'native':results[0].hex(),'object':results[1].hex()})
d={'cases':128,'mismatches':mismatches,'caller_sha256':hashlib.sha256(pa.read_bytes()).hexdigest(),'callee_sha256':hashlib.sha256(pb.read_bytes()).hexdigest(),'scope':'actual original caller slice and actual AddAirRam vs linked compiled caller slice and full compiled AddAirRam, flags both launch variants, empty slot, randomized finite mass and half extents, matched native constants; FPR-save helper controlled; original prior path and after-call controlled, software VMX estimate model, no hardware claim'};(base/'linked-launch-airram-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not mismatches
