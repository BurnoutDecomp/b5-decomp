from pathlib import Path
import hashlib
OUT=Path(__file__).resolve().parent
p=Path('D:/Reverse/BP-X360-Experimental-Worktrees/wave8_scene_terra/src/vendor/renderware/collision/GPBox.cpp')
assert hashlib.sha256(p.read_bytes()).hexdigest()=='21812905b9fdbf24c74b085a938289cc8804b18a0cb76f3f14800007eaa80fce'
s=p.read_text();start=s.index('namespace\n{',s.index('namespace collision'));end=s.index('// flt_82004744:',start)
s=s[:start]+(OUT/'native-helpers.txt').read_text()+s[end:]
start=s.index('void GPBox::GetMaximumFeature(');end=s.index('// ===========================================================================\n// rw::collision::GPBox::GetInterval',start)
s=s[:start]+(OUT/'maximum-feature-new.txt').read_text()+s[end:]
s=s.replace('#include <cmath>   // sqrt, fabs','#include <cmath>   // fabs')
start=s.index('// Canonical declarations');end=s.index('// GPBox::GetBBox',start)
s=s[:start]+'''// DecFIGS and the actual X360 entry agree on non-static const members.
// GetMaximumFeature receives this in r3, winding in r4, direction by value
// in v1 and Feature& in r5. The inherited native Vector3 rows carry their
// genuine mV values; mDimensions contains the half-extents.
//
'''+s[end:]
start=s.index('// VMX decode notes');end=s.index('// ===========================================================================',start)
s=s[:start]+'''// VMX arithmetic is retained through the real XDK intrinsic type/operations.
// The wall cross products and their two fused reciprocal-root refinements
// preserve the original rounding boundaries and all four lanes.
'''+s[end:]
start=s.index('// flt_821801B4:');end=s.index('// ARTIST image data',start)
s=s[:start]+s[end:]
p.write_bytes(s.replace('\r\n','\n').replace('\n','\r\n').encode())
print(hashlib.sha256(p.read_bytes()).hexdigest())
