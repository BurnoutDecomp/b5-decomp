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
    //   0x8280FFE8  sth  r4, 0xC(r3)   ; this->mu16NumResults = (u16)liNumResults
    //   0x8280FFEC  blr                ; return this (r3 unchanged)
    //
    // A single 16-bit store of the count into offset 0xC, returning `this`. The arg
    // arrives as a halfword (the caller PolygonSoupTesterJob::ExecuteFillTriangleCache
    // passes the triangle-cache fill count); truncated to u16 by the sth.
    // -------------------------------------------------------------------------
    CollisionResultList* CollisionResultList::SetNumResults(s32 liNumResults)
    {
        mu16NumResults = static_cast<u16>(liNumResults);
        return this;
    }

    // -------------------------------------------------------------------------
    // CollisionResultList::GetResult @ 0x828A9EF8
    //
    //   clrlwi r30, r4, 16                 ; lu16Index (zero-extended)
    //   lhz    r11, 0xC(r31)               ; mu16NumResults
    //   cmplw  cr6, r30, r11 ; blt ...     ; if (lu16Index >= mu16NumResults) assert
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
        CGS_ASSERT(lu16Index < mu16NumResults, "lu16Index < mu16NumResults");
        return &mpResults[lu16Index];
    }

    // -------------------------------------------------------------------------
    // PrimitiveTestResult::IsValid @ 0x82921378 (142) — REAL as of walls leg 2.
    //
    // The old "honest stub" here was gated on "the rodata constant at
    // unk_821016C0 is NOT recoverable from the export". The .rdata unlock
    // retired that floor: `x360rd.read(0x821016C0, 4)` == 0x34000000 ==
    // 1.1920929e-07 == 2^-23 (FLT_EPSILON). With the value in hand the whole
    // pipeline reads plainly:
    //
    //   stage 1 (all four vectors): per-component x==x self-compare on the x/y/z
    //     lanes (vspltw 0/1/2 + vcmpeqfp., w never tested) -> any NaN is invalid.
    //   stage 2 (the two NORMAL vectors only): vandc clears the sign bits, the
    //     vrlimi128(v, v, mask=1, rot=1) replaces the w lane with a copy of x
    //     (so w is excluded), and vcmpgtfp. against the 2^-23 splat sets the
    //     "all lanes false" CR6 bit. The branch keeps the record only when that
    //     bit is CLEAR, i.e. when SOME |component| EXCEEDS 2^-23:
    //     this is a non-degenerate-normal test (a zero/denormal normal is what
    //     the caller's "Invalid normal generated" dump is about), not an upper
    //     magnitude bound.
    // -------------------------------------------------------------------------
    namespace
    {
        // unk_821016C0 word 0, read from the image: 0x34000000 == 2^-23.
        const f32 KF_MIN_NORMAL_COMPONENT = 1.1920929e-07f;

        template <class TLaneVector>
        inline bool AllFiniteXYZ(const TLaneVector& lrV)
        {
            // x==x is false only for NaN (the console never tests the w lane).
            return (lrV.x == lrV.x) && (lrV.y == lrV.y) && (lrV.z == lrV.z);
        }

        inline bool NormalNonDegenerate(const Vector3& lrV)
        {
            const f32 lfAx = lrV.x < 0.0f ? -lrV.x : lrV.x;   // vandc sign clear
            const f32 lfAy = lrV.y < 0.0f ? -lrV.y : lrV.y;
            const f32 lfAz = lrV.z < 0.0f ? -lrV.z : lrV.z;
            return (lfAx > KF_MIN_NORMAL_COMPONENT)
                || (lfAy > KF_MIN_NORMAL_COMPONENT)
                || (lfAz > KF_MIN_NORMAL_COMPONENT);
        }
    }

    bool PrimitiveTestResult::IsValid() const
    {
        return AllFiniteXYZ(mPrimitive0Normal)
            && AllFiniteXYZ(mPrimitive1Normal)
            && AllFiniteXYZ(mPrimitive0Contact)
            && AllFiniteXYZ(mPrimitive1Contact)
            && NormalNonDegenerate(mPrimitive0Normal)
            && NormalNonDegenerate(mPrimitive1Normal);
    }
}
}
