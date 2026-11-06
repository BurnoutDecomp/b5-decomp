from pathlib import Path
import hashlib,json,random,struct,sys
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0,str(P/'scratch/x360_sdk'))
from native_atomic_fixture import Coff, Bundle, cpu
import validate_native_math as M
import vmxemu as V
assert V.x360rd.u32(0x82815570)==0x7C8A2378
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/director-math-917/wip1')
r=json.loads((O/'compile-results.json').read_text())
row=next(x for x in r['results'] if x['source'].endswith('BrnBehaviourRoadRunner.cpp'))
obj=Path(row['object']); sym='?Construct@BehaviourRoadRunner@Camera@BrnDirector@@UAAXXZ'
b=Bundle([Coff(obj,sym)]); b.select(sym)
print('unresolved',b.unresolved)
identity=struct.pack('>16I',0x3f800000,0,0,0,0,0x3f800000,0,0,0,0,0x3f800000,0,0,0,0,0)
rng=random.Random(0x8222BCE0); original_count=candidate_count=0
for i in range(64):
 poison=rng.randbytes(0x400)
 orig=M.new_cpu(); orig.mem.wr(M.S.SELF,poison); orig.setg(3,M.S.SELF); orig.run(0x8222BCE0)
 expected=orig.mem.rd(M.S.SELF+0x200,64); assert expected==identity; original_count+=1
 actual=cpu(b); actual.mem.wr(M.S.SELF,poison); actual.setg(3,M.S.SELF)
 for addr,name in b.unresolved.items():
  if name=='memcpy':
   def copy(c):
    dst,src,n=c.g(3),c.g(4),c.g(5); c.mem.wr(dst,c.mem.rd(src,n)); c.setg(3,dst)
   actual.hooks[addr]=copy
  elif name.startswith('?Construct@Behaviour@Camera@BrnDirector@@'):
   actual.hooks[addr]=lambda c:None
 actual.run(b.entry)
 assert actual.mem.rd(M.S.SELF+0x200,64)==expected, i
 candidate_count+=1
out={'original_address':'0x8222BCE0','cases':original_count,'candidate_cases':candidate_count,'compared_offset':'0x200','compared_bytes':64,'expected_hex':identity.hex(),'scope':'Full original and compiled RoadRunner Construct; compare 64-byte transform only. Compiled base Behaviour::Construct is a no-op hook and memcpy is modeled byte-for-byte; no other state compared.','source':str(Path.cwd()/'src/GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.cpp'),'source_sha256':hashlib.sha256(Path('src/GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.cpp').read_bytes()).hexdigest(),'object':str(obj),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'unresolved':list(b.unresolved.values())}
Path('D:/Reverse/BP-X360-Experimental-Artifacts/director-math-917/identity-comparison-wip.json').write_text(json.dumps(out,indent=2)+'\n'); print(json.dumps(out,indent=2))

