from pathlib import Path
import ast, hashlib, json, re

ROOT=Path('D:/Reverse/BP-X360-Experimental-Worktrees/external-body-native-root')
HERE=Path(__file__).resolve().parent
tree=ast.parse((HERE/'remove_pc_probes.py').read_text())
exec(compile(ast.Module(body=[n for n in tree.body if isinstance(n,ast.FunctionDef)],type_ignores=[]),'brace-parser','exec'))
paths=['src/GameSource/Physics/BrnPhysicsModuleUpdateFunctions.cpp',
       'src/GameSource/Physics/DeformationManager/BrnDeformationManager_Contacts.cpp',
       'src/GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.cpp']
before={p:(ROOT/p).read_text()for p in paths}
s=before[paths[0]]
s=cut_block(s,'// ---- [T-ystep] DIAG.','void TrafficYStep(')
s,n=re.subn(r'^\s*TrafficYStep\([^\n]*\);[^\n]*\n','\n',s,flags=re.M);assert n==16,n
s=cut_block(s,'// [pausebit] witness. NOT X360.','        {')
s=cut_block(s,'// ---- [slomo] THE LAST RUNG','if (lbCrashedThisFrame)')
s=re.sub(r'^#include [^\n]*BrnSimpleVehiclePhysics.h[^\n]*\n','',s,flags=re.M)
s=re.sub(r'^#include <cstdlib>[^\n]*\n','',s,flags=re.M)
after={paths[0]:s}
s=before[paths[1]]
s=cut_block(s,'// ⭐ BOOT WITNESS (log-once)','            {')
s=cut_block(s,'// ---- [wall] PC bring-up instrument','            {')
s=re.sub(r'^\s*// ---- end \[(?:wall|wedge-pen)\][^\n]*\n','\n',s,flags=re.M)
s=re.sub(r'^#include <(?:cstdlib|cmath)>[^\n]*\n','',s,flags=re.M)
after[paths[1]]=s
s=before[paths[2]]
s=cut(s,'// [T5-imp] DIAG counters','namespace Deformation')
s=cut(s,'        // [T5-imp] DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.\n','        // The contact wakes THIS body')
s=cut(s,'        // [T5-imp] DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. Recorded BEFORE','        // Separating (or at rest)')
s=cut_block(s,'// [T5-solve] DIAG. NOT IN THE X360 BINARY.','        {')
s=cut(s,'        // [T5-imp] DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. The two numbers','        // ---- [kerb-imp] THE CAR-CAR LEG.')
s=cut_block(s,'// ---- [kerb-imp] THE CAR-CAR LEG.','if ( BrnPhysics::Vehicle::KerbProbeArmed()')
s=cut_block(s,'// ---- [kerb-imp] one line per world impulse','        {')
s=cut_block(s,'// ---- [worldimp] PC bring-up instrument','        {')
s=re.sub(r'^\s*// ---- end \[kerb-imp\][^\n]*\n','\n',s,flags=re.M)
s=re.sub(r'^#include <cstdlib>[^\n]*\n','',s,flags=re.M)
assert 'CgsDev::Log'not in s
s=re.sub(r'^#include [^\n]*CgsLog.h[^\n]*\n','',s,flags=re.M)
after[paths[2]]=s
for p,s in after.items():
    code=re.sub(r'//[^\n]*','',s)
    assert not any(t in code for t in ['TrafficYStep','gT5','KerbProbe','[worldimp]','[pausebit]','[slomo]','[wall]']),p
    q=HERE/'before-callers-2'/p;q.parent.mkdir(parents=True,exist_ok=True);assert not q.exists();q.write_text(before[p],encoding='utf-8')
for p,s in after.items():(ROOT/p).write_text(s,encoding='utf-8')
sha=lambda s:hashlib.sha256(s.encode()).hexdigest()
receipt={p:{'before_sha256':sha(before[p]),'after_sha256':sha(after[p]),'lines_removed':len(before[p].splitlines())-len(after[p].splitlines())}for p in paths}
(HERE/'caller-pc-removal-source.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'paths':3,'lines_removed':sum(v['lines_removed']for v in receipt.values())}))
