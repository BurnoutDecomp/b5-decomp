from pathlib import Path
p=Path(r'C:/Users/adri1/.codex/worktrees/bp360/native_lion/src/SDKs/Packages/Lion/Final/eauk_common/Maths/Vector.h')
s=p.read_bytes().decode('utf8').replace('\r\n','\n')
s=s.replace('#include "types.hpp"','#include "types.hpp"\n#include <vectorintrinsics.h>')
a=s.index('    // ⚠ PARAMETERS ARE');b=s.index('    // ⛔ NO CONSTRUCTORS.',a)
s=s[:a]+'''    // The original methods take cVector values (DWARF Vector.h:120..149).
    // Keep those snapshots when the destination aliases a source. Whole-quad
    // operations below use the native VMX instructions observed in ARTIST.
    // The recovered call sites inline these methods; they do not attest a
    // standalone cVector register-passing contract for this storage type.
    //
'''+s[b:]
s=s.replace('const cVector& arA, const cVector& arB, const cVector& arT','cVector arA, cVector arB, cVector arT')
s=s.replace('const cVector& arA, const cVector& arB','cVector arA, cVector arB')
for name,op in [('Mul','__vmulfp'),('Add4','__vaddfp'),('Sub4','__vsubfp'),('Mul4','__vmulfp')]:
 a=s.index('    void '+name+'(');b=s.index('\n    }',a)+len('\n    }')
 s=s[:a]+'''    void %s(cVector arA, cVector arB)
    {
        const __vector4 lA = __lvx(&arA, 0);
        const __vector4 lB = __lvx(&arB, 0);
        __stvx(%s(lA, lB), this, 0);
    }'''%(name,op)+s[b:]
p.write_bytes(s.replace('\n','\r\n').encode('utf8'))
