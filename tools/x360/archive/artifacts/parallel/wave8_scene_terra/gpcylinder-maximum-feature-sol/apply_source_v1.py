from pathlib import Path
import hashlib
OUT=Path(__file__).resolve().parent;p=Path('D:/Reverse/BP-X360-Experimental-Worktrees/wave8_scene_terra/src/vendor/renderware/collision/GPCylinder.cpp')
assert hashlib.sha256(p.read_bytes()).hexdigest()=='5edfe592670eb328fa7de197eb7d69a8d9200b0864e75b741cc5346b520e04ba'
s=p.read_text();start=s.index('namespace\n{',s.index('namespace collision'));end=s.index('// flt_821803A0:',start);s=s[:start]+(OUT/'native-helpers.txt').read_text()+s[end:]
start=s.index('void GPCylinder::GetMaximumFeature(');end=s.index('// ===========================================================================\n// rw::collision::GPCylinder::GetInterval',start);s=s[:start]+(OUT/'maximum-feature-new.txt').read_text()+s[end:]
s=s.replace('#include <cmath>   // sqrt, fabs','#include <cmath>   // fabs\n#include <cstring> // complete FeatureEdge object copies')
start=s.index('// Canonical declarations');end=s.index('// ===========================================================================',start);s=s[:start]+'''// Native const members consume the direction by value in v1. GetMaximumFeature
// receives this/r3, winding/r4 and Feature&/r5. Real inherited Vector3 rows
// retain mV storage; no scalar callback facade is introduced here.
//
// The local-frame transpose and all product/fused accumulation boundaries are
// retained. VMX estimates receive the original ordered two refinements.
'''+s[end:]
p.write_bytes(s.replace('\r\n','\n').replace('\n','\r\n').encode());print(hashlib.sha256(p.read_bytes()).hexdigest())
