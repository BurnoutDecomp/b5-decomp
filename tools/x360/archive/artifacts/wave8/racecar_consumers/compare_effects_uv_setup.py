from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');obj=out/'BrnEffectsBuildUVData/probe.obj';o=R.Coff(obj,'?SetupFromMaterial@BuildUVData@');rng=random.Random(822780);errors=[]
for i in range(256):
 before=rng.randbytes(0x100);mat=bytearray(rng.randbytes(0x100));struct.pack_into('>i',mat,0x34,rng.randint(-10000,10000));mat[0x38]=rng.randint(1,255);mat[0x39]=rng.randint(1,255);results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();a=R.M.S.SELF;b=R.M.S.AUX;c.mem.wr(a,before);c.mem.wr(b,mat);c.setg(3,a);c.setg(4,b)
  try:c.run(o.entry if compiled else 0x822780c8)
  except Exception as e:print('error',compiled,type(e).__name__,e);raise
  results.append((c.mem.rd(a,0x100),c.mem.rd(b,0x100)))
 if results[0]!=results[1]:errors.append({'case':i,'offsets':[j for j,(x,y) in enumerate(zip(results[0][0],results[1][0])) if x!=y][:20]})
d={'cases':256,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'whole actual BuildUVData::SetupFromMaterial vs ARTIST 822780C8; random signed frame counts, nonzero 8-bit atlas dimensions, poisoned 0x100 output/input and all bytes compared; zero-dimension and GPU behavior excluded'};(out/'effects-uv-setup-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
