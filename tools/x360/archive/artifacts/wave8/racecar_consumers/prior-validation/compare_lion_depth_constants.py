import sys,random,struct,json,hashlib
from pathlib import Path
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers/prior-validation');obj=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers/BrnLionBlendIm3d/final-d08b4c45f5a0.obj');o=R.Coff(obj,'?BeginRendering@Im3dBlend@');rng=random.Random(82060);errors=[]
for i in range(128):
 vals=[struct.unpack('>f',struct.pack('>f',rng.uniform(0.1,10.0)))[0] for _ in range(6)];colour,near,far,depth,u,v=vals;result=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();sp=c.g(1);c.setg(31,R.M.S.SELF)
  if compiled:
   for reg,x in [(29,colour),(28,near),(30,far),(27,depth),(26,u),(25,v),(31,0.0)]:c.setf(reg,x)
   start=o.entry+0xc4;end=o.entry+0x1ac;offs=[0x70,0x90,0xa0,0x60]
  else:
   image=(out/'82282060.bin').read_bytes();c.mem.add(0x82282060,len(image),image)
   for reg,x in [(30,colour),(29,near),(31,far),(28,depth),(27,u),(26,v)]:c.setf(reg,x)
   start=0x82282110;end=0x822821a8;offs=[0x70,0x80,0x90,0xa0]
  try:c.mem.wr(end,bytes.fromhex('4e800020'));c.run(start)
  except Exception as e:print('error',compiled,type(e).__name__,e);raise
  result.append([c.mem.rd(sp+x,16) for x in offs])
 if result[0]!=result[1]:errors.append({'case':i,'native':[x.hex() for x in result[0]],'compiled':[x.hex() for x in result[1]]})
d={'cases':128,'mismatch_count':len(errors),'mismatches':errors[:3],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual Im3d depth-fade constant construction slice versus ARTIST 82282110..1A4; random finite color/near/far/depth/U/V, compares all four 16-byte offset, scale, depth and fade constants; prior shader/device calls controlled and GPU excluded'};(out/'lion-depth-constants-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
