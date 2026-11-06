from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');obj=out/'BrnPhysicalTrafficManager/probe.obj';o=R.Coff(obj,'?UpdateTriangleCache@PhysicalTrafficManager@');rng=random.Random(0x825ee);errors=[]
for i in range(256):
 ext=[rng.uniform(-2.0,10.0) for _ in range(4)];pos=rng.randbytes(16);body=bytearray(rng.randbytes(0x800));struct.pack_into('>4f',body,0x6a0,*ext);body[0x40:0x50]=pos;values=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();sp=c.g(1);c.mem.wr(R.M.S.SELF,body);c.setg(11,R.M.S.SELF);c.vr[127]=(0,)*4;c.vr[126]=(0x3f800000,)*4;c.vr[125]=(0x3f000000,)*4;c.vr[6]=(0x3f800000,)*4
  if compiled:c.setg(26,0x6a0);c.setg(27,0x40);c.vr[124]=(0x3f800000,)*4;start=o.entry+0x108;end=o.entry+0x16c
  else:
   image=(out/'825EE640.bin').read_bytes();c.mem.add(0x825ee640,len(image),image);c.setg(16,0x6a0);c.setg(17,0x40);c.setg(10,sp+0xa0);c.vr[0]=(0x3f800000,)*4;c.vr[13]=(0,)*4;c.vr[12]=(0,)*4;start=0x825ee7e4;end=0x825ee860
  c.mem.wr(end,bytes.fromhex('4e800020'));c.run(start);values.append((c.mem.rd(sp+0x70,16),c.mem.rd(R.M.S.SELF,0x800)))
 if values[0]!=values[1]:errors.append({'case':i,'native':values[0][0].hex(),'compiled':values[1][0].hex()})
d={'cases':256,'mismatch_count':len(errors),'mismatches':errors[:8],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual triangle-cache radius and event-vector slice vs ARTIST 825EE7E4..85C, random finite half-extents and raw position including NaN payloads, all 16 event bytes and body input preserved; global padding seeded to native 1, queue/prior paths and physical hardware excluded; reciprocal-sqrt estimate follows software model'};(out/'traffic-cache-vector-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors

