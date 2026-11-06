from pathlib import Path
import re
p=Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/wave8_scene_terra/src/vendor/renderware/collision/SeparatingDirection.cpp')
s=p.read_text()
for n in ['Add','Sub','Cross','Negate']:
 s=re.sub(r'\b'+n+r'\s*\(', 'Sep'+n+'(', s)
s=s.replace('const Vector3 lvSeed = (lvAxis.mV.x > KF_CYLINDER_SAT_HALF) ? rw::math::vpu::GetVector3_YAxis()\n                                                                  : rw::math::vpu::GetVector3_XAxis();', '''const __vector4 basisI = {1.0f, 0.0f, 0.0f, 0.0f};
            const __vector4 basisJ = {0.0f, 1.0f, 0.0f, 0.0f};
            const Vector3 lvSeed((lvAxis.mV.x > KF_CYLINDER_SAT_HALF) ? basisJ : basisI);''')
p.write_bytes(s.replace('\n','\r\n').encode())
