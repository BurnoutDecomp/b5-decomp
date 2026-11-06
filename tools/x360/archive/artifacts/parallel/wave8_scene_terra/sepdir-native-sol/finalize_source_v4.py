from pathlib import Path
import re
p=Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/wave8_scene_terra/src/vendor/renderware/collision/SeparatingDirection.cpp')
s=p.read_text()
# Retire obsolete PC-lowering and wave-history commentary. The complete
# preimage remains immutable in SeparatingDirection.before.cpp.
s='\n'.join(line.split('//',1)[0].rstrip() for line in s.splitlines())
s=re.sub(r'\n{3,}','\n\n',s)
header='''// Native ARTIST separating-axis family. Geometry values use the genuine
// Vector3/VecFloat ABI. VMX expressions retain their emitted rounding order;
// raw-estimate normalizations intentionally remain raw estimates.
// RimToRim's seven inputs are references (r3-r9), with output in r10.
// The radius class spelling is a machine-ABI-compatible reconstruction from
// native scalar-splat uses; no original DecFIGS rim declaration is available.
// The shared rim guard row is FLT_MIN after the attested CRT initializer.

'''
s=header+s
for name,at in [('FindBestSepDir_FindCandidates','82BB4038'),('FindBestSeparatingDirection','82BB4660'),('FindBestSeparatingDirectionBoxBox','82BB47F0'),('FindBestSeparatingDirectionBoxTri','82BAA1A0'),('FindBestSeparatingDirectionSphSph','82BAA1F0'),('FindBestSepDirWithCylinder','82BB76E8'),('FindBestSeparatingDirCylVol','82BAA250'),('FindBestSeparatingDirVolCyl','82BAA258'),('RimToEdge','82BB53F8'),('RimToRim','82BB56C8'),('AddRimToEdgeCandidate','82BB6118'),('AddRimToRimCandidates','82BB61C0'),('FindBestSeparatingDirectionTriBox','82BB4CA8'),('FindBestSepDirWithCylinder_Evaluate','82BB75B0'),('AddAxisToEdgeCandidate','82BB50E8'),('FindBestSepDirWithCylinder_FindCandidates','82BB64C0')]:
 s=re.sub(r'(?m)^((?:Vector3\*|Vector3|VecFloat|RwBool|void|u32|static void) '+name+r'\()', '// ARTIST 0x'+at+'\n'+r'\1',s)
# Split the literal register-local declarations for scanning against primary.
def regs(m):
 names=m[1].split(', ')
 return '\n'.join('    __vector4 '+', '.join(names[i:i+8])+';' for i in range(0,len(names),8))
s=re.sub(r'    __vector4 ((?:v\d+, )+v\d+);',regs,s)
s=s.replace('    Interval cylinderIntervals[16], otherIntervals[16];','    Interval cylinderIntervals[KU_CYLINDER_SAT_MAX_CANDIDATES];\n    Interval otherIntervals[KU_CYLINDER_SAT_MAX_CANDIDATES];')
s=s.replace('static const f32 KF_CYLINDER_SAT_INITIAL_SEPARATION = 0.0f;\n\n','')
a=s.index('Vector3 RimToEdge(');b=s.index('RwBool RimToRim(',a)
part=s[a:b].replace('    u32 iteration = 0;\n','').replace('    f32 scalarFraction = 0.0f;\n','')
s=s[:a]+part+s[b:]
a=s.index('RwBool RimToRim(');b=s.index('void AddRimToEdgeCandidate(',a)
s=s[:a]+s[a:b].replace('    u32 remaining = 0;\n','')+s[b:]
p.write_bytes(s.replace('\n','\r\n').encode())
print('final lines',len(s.splitlines()))
