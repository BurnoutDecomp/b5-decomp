#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX. CgsSceneManager::CgsCollision result
// containers:
//   CollisionResultList::SetNumResults @ 0x8280FFE8 — set the 16-bit result count.
//   CollisionResultList::GetResult     @ 0x828A9EF8 — bounds-checked result accessor.
//   PrimitiveTestResult::IsValid       @ 0x82921378 — VMX validity test (KEYSTONE).

namespace CgsSceneManager
{
namespace CgsCollision
{
    // -------------------------------------------------------------------------
    // CollisionResultList::SetNumResults @ 0x8280FFE8
    //
    //   0x8280FFE8  sth  r4, 0xC(r3)   ; this->munNumResults = (u16)liNumResults
    //   0x8280FFEC  blr                ; return this (r3 unchanged)
    //
    // A single 16-bit store of the count into offset 0xC, returning `this`. The arg
    // arrives as a halfword (the caller PolygonSoupTesterJob::ExecuteFillTriangleCache
    // passes the triangle-cache fill count); truncated to u16 by the sth.
    // -------------------------------------------------------------------------
    CollisionResultList* CollisionResultList::SetNumResults(s32 liNumResults)
    {
        munNumResults = static_cast<u16>(liNumResults);
        return this;
    }

    // -------------------------------------------------------------------------
    // CollisionResultList::GetResult @ 0x828A9EF8
    //
    //   clrlwi r30, r4, 16                 ; lu16Index (zero-extended)
    //   lhz    r11, 0xC(r31)               ; munNumResults
    //   cmplw  cr6, r30, r11 ; blt ...     ; if (lu16Index >= munNumResults) assert
    //   lwz    r10, 0(r31)                 ; mpResults
    //   mulli  r11, r30, 0x70              ; 112 * lu16Index
    //   add    r3,  r11, r10               ; &mpResults[lu16Index]
    //
    // Returns the lu16Index'th 112-byte result record, bounds-checked against the
    // live count. The assert message + source path are verbatim X360 rodata
    // (CgsCollisionResultList.h:143; file/line dropped by CGS_ASSERT).
    // -------------------------------------------------------------------------
    CollisionResult* CollisionResultList::GetResult(u16 lu16Index)
    {
        CGS_ASSERT(lu16Index < munNumResults, "lu16Index < mu16NumResults");
        return &mpResults[lu16Index];
    }

    // -------------------------------------------------------------------------
    // PrimitiveTestResult::IsValid @ 0x82921378  — VMX KEYSTONE (honest stub)
    //
    // The X360 body is a multi-stage hand-vectorised VMX/AltiVec pipeline with NO
    // scalar lowering that can be reproduced without fabricating a per-lane formula:
    //
    //   for lane-vector in {mLane0, mLane1, mLane2, mLane3}:
    //       lvx128;  vspltw lane 0/1/2;  vcmpeqfp.   ; per-lane self-compare == finiteness
    //                                                ; (x==x is false only for NaN)
    //       extract CR6 bit; early-out to "invalid" on the first non-finite component
    //   then for mLane0 and mLane1:
    //       vspltisw v0,-1 ; vslw v0,v0,v0           ; build the 0x80000000 sign mask
    //       lvlx v13,[unk_821016C0] ; vspltw          ; load a rodata magnitude threshold
    //       vandc v* , mask                          ; clear sign bit  -> |component|
    //       vrlimi128 ; vcmpgtfp.                     ; |component| > threshold ?
    //       early-out to "invalid" on the first over-threshold component
    //   return true only if all checks pass.
    //
    // The per-lane finiteness intent is clear, BUT the magnitude branch depends on the
    // rodata constant at unk_821016C0 (an absolute-value threshold whose value is NOT
    // recoverable from the export here) and on the exact vrlimi128 lane-rotation /
    // vandc masking semantics. Per the VMX-keystone rule this must NOT be paraphrased
    // into an invented scalar formula and the threshold must NOT be fabricated.
    //
    // Honest placeholder: the boot-trace never executes IsValid (it is called only
    // from the ContactGeneratorJob narrow-phase, off the boot path). Returns true so
    // dependents compile and link; FLAGGED in still_unbodied as a VMX keystone with a
    // documented floor (unrecoverable rodata threshold @ unk_821016C0).
    // -------------------------------------------------------------------------
    bool PrimitiveTestResult::IsValid() const
    {
        // VMX keystone — body intentionally not reconstructed (see comment above).
        return true;
    }
}
}
