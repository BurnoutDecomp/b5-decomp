from pathlib import Path
import sys,random,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra');obj=out/'Wheel/probe.obj';rng=random.Random(0x825d5500);errors=[]
table=bytearray()
for lane in range(4):
 for src in range(4):
  v=list(range(16));v[lane*4:lane*4+4]=list(range(16+4*src,20+4*src));table.extend(v)
for symbol,entry in [('?PrepareDefaultFrontTire@TireAttribs@Wheel@',0x825d5500),('?PrepareDefaultRearTire@TireAttribs@Wheel@',0x825d57d8)]:
 o=R.Coff(obj,symbol)
 for i in range(256):
  before=rng.randbytes(0x100);results=[]
  for compiled in (False,True):
   c=o.cpu() if compiled else R.M.new_cpu();c.mem.wr(R.M.S.SELF,before);c.setg(3,R.M.S.SELF+32)
   if not compiled:
    image=(out/('%08X.bin'%entry)).read_bytes();c.mem.add(entry,len(image),image)
    for j in range(0,len(table),16):
     try:c.mem.wr(0x8327f140+j,table[j:j+16])
     except KeyError:c.mem.add(0x8327f140+j,16,table[j:j+16])
   if not compiled and i==0:print("TABLE",c.mem.rd(0x8327f140,16).hex())
   c.run(o.entry if compiled else entry);results.append(c.mem.rd(R.M.S.SELF,0x100))
  if results[0]!=results[1]:errors.append({'symbol':symbol,'case':i,'offsets':[j for j,(a,b) in enumerate(zip(*results)) if a!=b][:20], 'original':results[0][32:96].hex(), 'actual':results[1][32:96].hex()})
d={'cases_per_function':256,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'whole actual default front/rear tire init vs ARTIST 825D5500/825D57D8 with 256 random poisoned 0x100 objects each; original 16 vperm lane-insert masks seeded from recovered table contract, software VMX'};(out/'wheel-tire-default-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors



