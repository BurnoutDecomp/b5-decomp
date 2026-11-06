from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');obj=out/'BrnLionBlendRenderer/probe.obj';o=R.Coff(obj,'?QuadDraw@?A');rng=random.Random(82330);errors=[];edge=[0x7f800001,0xff800001,0x7fc00001,0,0x80000000]
for i in range(128):
 u=edge[i] if i<len(edge) else rng.getrandbits(32);v=edge[(i+1)%len(edge)] if i<len(edge) else rng.getrandbits(32);data=bytearray(rng.randbytes(0x80));struct.pack_into('>I',data,0x3c,u);struct.pack_into('>I',data,0x4c,v);result=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();b=R.M.S.AUX;c.mem.wr(b,data)
  if compiled:
   c.setg(30,b);c.setg(29,b+0x400);c.mem.wr(o.entry+0x4c,bytes.fromhex('4e800020'));c.run(o.entry+0x2c)
  else:
   image=(out/'82282330.bin').read_bytes();c.mem.add(0x82282330,len(image),image);c.setg(5,b);c.setg(27,b+0x30);c.mem.wr(0x82282398,bytes.fromhex('4e800020'));c.run(0x82282388);c.mem.wr(0x822823c8,bytes.fromhex('4e800020'));c.run(0x822823b4)
  result.append((c.vr[1],c.vr[2]))
 if result[0]!=result[1]:errors.append({'case':i,'frame':hex(u),'next':hex(v),'native':str(result[0]),'compiled':str(result[1])})
d={'cases':128,'mismatch_count':len(errors),'mismatches':errors[:10],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual QuadDraw frame arguments v1/v2 vs original lvx/vsplt slices 82282388..94 and 822823B4..C4; random raw frame words, caller prestate controlled; no BuildUVs or rendering claim'};(out/'lion-frame-abi-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors

