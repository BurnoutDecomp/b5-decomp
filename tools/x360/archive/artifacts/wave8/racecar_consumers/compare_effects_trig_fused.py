from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');obj=out/'BrnEffectsUtils/probe.obj';o=R.Coff(obj,'?Cos4_UnitCycles@');rng=random.Random(0x8227e850);errors=[]
def splat(x):return (x,)*4
c5=0xc28e5ba2;c3=0x422396e8;c1=0xc0c8e5ba
for i in range(256):
 t=struct.unpack('>I',struct.pack('>f',rng.uniform(0,0.0625)))[0]
 native=R.M.new_cpu();native.mem.add(0x8227e7a8,0x200,(out/'8227E7A8.bin').read_bytes());native.vr[12]=splat(t);native.vr[10]=splat(c3);native.vr[9]=splat(c5);native.mem.wr(0x8227e854,bytes.fromhex('4e800020'));native.run(0x8227e850);inner=native.vr[7]
 candidate=o.cpu();candidate.vr[127]=splat(t);candidate.vr[0]=splat(c5);candidate.vr[126]=splat(c3);candidate.mem.wr(o.entry+0xb8,bytes.fromhex('4e800020'));candidate.run(o.entry+0xb4)
 if candidate.vr[126]!=inner:errors.append([i,'inner']);continue
 native=R.M.new_cpu();native.mem.add(0x8227e7a8,0x200,(out/'8227E7A8.bin').read_bytes());native.vr[12]=splat(t);native.vr[8]=splat(c1);native.vr[7]=inner;native.mem.wr(0x8227e85c,bytes.fromhex('4e800020'));native.run(0x8227e858)
 candidate=o.cpu();candidate.vr[127]=splat(t);candidate.vr[126]=inner;candidate.vr[0]=splat(c1);candidate.mem.wr(o.entry+0xd0,bytes.fromhex('4e800020'));candidate.run(o.entry+0xcc)
 if candidate.vr[127]!=native.vr[12]:errors.append([i,'outer'])
d={'cases':256,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual compiled two fused trig polynomial instructions versus ARTIST 8227E850 and 8227E858, 256 finite T squared in native folded domain with exact original coefficient bits, register inputs seeded; constant construction and surrounding fold/matrix logic excluded, software VMX only'};(out/'effects-trig-fused-slices.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors

