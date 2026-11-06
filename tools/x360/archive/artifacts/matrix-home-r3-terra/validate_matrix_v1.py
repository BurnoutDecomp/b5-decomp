from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/matrix-home-r3-terra');obj=O/'probe_forward.obj';A=F.M.S.AUX;AFF=A+0x1000;FULL=A+0x2000;OUT=A+0x3000;CODE=A+0x5000

def bundle(n):
 b=F.Bundle([F.Coff(obj,'?'+n)]);b.select('?'+n);return b
bs={x:bundle(x) for x in ['matrix_product','copy_full','copy_affine','default_full','default_affine']}
def product(compiled,aff,full):
 c=F.cpu(bs['matrix_product']);c.mem.wr(AFF,aff);c.mem.wr(FULL,full);c.mem.wr(OUT,b'\xa5'*64)
 if compiled:c.setg(3,OUT);c.setg(4,AFF);c.setg(5,FULL);c.run(bs['matrix_product'].entry)
 else:
  c.mem.add(CODE,0x1000);c.mem.wr(CODE,b''.join(struct.pack('>I',F.M.V.x360rd.u32(pc)) for pc in range(0x823f8a40,0x823f8ad0,4))+struct.pack('>I',0x4e800020))
  for r,v in [(30,AFF),(4,AFF+16),(3,AFF+32),(28,AFF+48),(29,FULL),(11,48),(10,16),(9,32),(31,OUT),(27,OUT+16),(26,OUT+32),(25,OUT+48)]:c.setg(r,v)
  c.run(CODE)
 return c.mem.rd(OUT,64)
rng=random.Random(0x823f8a40)
for i in range(256):
 words=[0,0x80000000,1,0x80000001,0x00800000,0x80800000,0x3f800000,0xbf800000,0x7f7fffff,0xff7fffff]
 def mat():return b''.join(struct.pack('>I',rng.choice(words) if i%4==0 else struct.unpack('>I',struct.pack('>f',rng.uniform(-20,20)))[0]) for _ in range(16))
 aff,full=mat(),mat();a=product(False,aff,full);z=product(True,aff,full)
 if a!=z:raise AssertionError((i,a.hex(),z.hex()))
 raw=rng.randbytes(64)
 for n in ['copy_full','copy_affine']:
  b=bs[n];c=F.cpu(b);c.mem.wr(AFF,raw);c.mem.wr(OUT,b'\x77'*64);c.setg(3,OUT);c.setg(4,AFF);c.run(b.entry)
  if c.mem.rd(OUT,64)!=raw or c.g(3)!=OUT:raise AssertionError((i,n))
 for n in ['default_full','default_affine']:
  b=bs[n];c=F.cpu(b);c.mem.wr(OUT,raw);c.setg(3,OUT);c.run(b.entry)
  if c.mem.rd(OUT,64)!=raw:raise AssertionError((i,n))
r={'cases':256,'scope':'ARTIST 823F8A40..ACC actual inline affine×full VMX vs compiled shared-home Mult: full 64 output bytes, finite and signed-zero/subnormal/extreme lanes, interpreter VMX model. Compiled exact-header default poison preservation, full raw copy and hidden-pointer argument/return for both types, 256 cases each; no full world/game claim.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'matrix-comparison-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
