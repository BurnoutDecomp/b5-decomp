from pathlib import Path
import sys,struct,json,re,hashlib
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
src=Path('C:/Users/adri1/.codex/worktrees/bp360/vehicle_physics_terra/tests/FxGateSteeredDirectionData.h').read_text();rows=re.findall(r'\{ \{ ([^}]+) \}, \{ ([^}]+) \}, ([01]) \}',src);assert len(rows)==300
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_combined');obj=out/'airram_provider/probe.obj';o=F.Coff(obj,'?SteeredDirection@');errors=[]
def val(v):return struct.unpack('>f',struct.pack('>I',v))[0]
for i,(inp,expected,_) in enumerate(rows):
 values=[int(x.strip().rstrip('uU'),16) for x in inp.split(',')];expect=[int(x.strip().rstrip('uU'),16) for x in expected.split(',')];angle,up,at=values[0],values[1:4],values[4:7];si,co=F.M.S.xm_sincos(val(angle));c=o.cpu();upaddr=F.M.S.AUX+0x100;ataddr=F.M.S.AUX+0x200;c.mem.wr(upaddr,struct.pack('>4I',*(up+[0])));c.mem.wr(ataddr,struct.pack('>4I',*(at+[0])));c.setg(3,upaddr);c.setg(4,ataddr);c.setf(1,float(si));c.setf(2,float(co));c.run(o.entry);actual=list(c.vr[1]);expect=expect+[expect[0]]
 if expect!=actual:errors.append({'case':i,'expected':expect,'actual':actual})
d={'cases':len(rows),'mismatches':errors[:10],'mismatch_count':len(errors),'source':'FxGateSteeredDirectionData.h existing native original blocks and same software sincos model','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual native SteeredDirection helper, all four output lanes; sincos modeled, no physical hardware claim'};(out/'steered-direction-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
