from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra');out=base/'render-rce-rebind-v3';obj=out/'render/probe.obj';o=R.Coff(obj,'?GenerateDispatchLists@RaceCarEntityModule@BrnWorld@@');rng=random.Random(0x822e8040);errors=[]
for i in range(256):
 def fv():return struct.unpack('>4I',struct.pack('>4f',*[rng.uniform(-100,100) for _ in range(4)]))
 ext,height,offset,dim=fv(),fv(),fv(),fv();matrix=rng.randbytes(64);results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();sp=c.g(1);c.setg(11,R.M.S.SELF+0x1000);c.mem.wr(R.M.S.SELF+0x1040,struct.pack('>4I',*dim));c.mem.wr(sp+(0xb8 if compiled else 0x120),struct.pack('>I',R.M.S.AUX));c.mem.wr(sp+(0x160 if compiled else 0x150),matrix)
  if compiled:
   c.vr[125]=ext;c.vr[127]=height;c.vr[126]=offset;c.mem.wr(o.entry+0x834,bytes.fromhex('4e800020'));c.run(o.entry+0x814)
  else:
   image=(base/'822E79F8.bin').read_bytes();c.mem.add(0x822e79f8,len(image),image);c.vr[124]=ext;c.vr[126]=height;c.vr[125]=offset;c.mem.wr(0x822e8040,bytes.fromhex('4e800020'));c.run(0x822e8018)
  results.append((c.g(3),c.mem.rd(c.g(4),64),c.vr[1],c.vr[2],c.vr[3]))
 if results[0]!=results[1]:errors.append([i,str(results)])
d={'cases':256,'count':len(errors),'mismatches':errors[:2],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'compiled_slice':'GenerateDispatchLists COMDAT+814..830','original_slice':'822E8018..803C','scope':'actual emitted AddShadow call argument setup vs ARTIST; randomized full vector lanes and matrix bytes, matrix pointer normalized by contents, no provider execution or graphics claim'};(out/'actual-shadow-caller-comparison.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2));assert not errors
