#pragma once

// CgsSceneManager::CgsCollision collision-query RESULT containers.
//
//   CollisionResultList  — the broad-phase result list a tester job fills; carries a
//                          16-bit live-result count (munNumResults) the job sets.
//   PrimitiveTestResult  — one narrow-phase primitive-vs-primitive test outcome: four
//                          16-byte (vec4) lanes whose finiteness + magnitude the
//                          collision jobs validate before consuming the contact.
//
// Both live in the CgsSceneManager::CgsCollision namespace (DWARF assert home path
// CgsSceneManagerTypes.h, mirrored alongside the other CgsCollision primitives —
// see CgsTriangleList.h). Layouts are taken store-for-store from the X360 asm; only
// the fields the recovered functions observe are named, the rest of each (large)
// aggregate is preserved as reserved span so sizeof/offsets stay console-faithful.

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector4 (rw::math::vpu, 16-byte / 16-aligned lane)

namespace CgsSceneManager
{
namespace CgsCollision
{
    // -------------------------------------------------------------------------
    // CollisionResult
    //
    // One entry of a CollisionResultList: the per-hit collision-query record a
    // broad-phase tester job produces (line/sphere/volume test outcome). The X360
    // asm pins the ELEMENT STRIDE only — CollisionResultList::GetResult indexes the
    // results buffer with `112 * index` (mulli rN, index, 0x70) — so the record is
    // 0x70 = 112 bytes. Its individual fields are not observed by any recovered
    // function here, so the body is modelled as a size-only reserved span (the
    // stride is asm-authoritative; the field breakdown lands when a consumer that
    // reads them is worked). This is the singular type the header file is named for.
    // -------------------------------------------------------------------------
    struct CollisionResult
    {
        u8 maReserved[0x70];  // 112-byte record (stride from GetResult: mulli x0x70)
    };

    // -------------------------------------------------------------------------
    // CollisionResultList
    //
    // The result list a broad-phase tester job (PolygonSoupTesterJob) writes its
    // hits into. Two accessors are recovered:
    //
    //   SetNumResults @ 0x8280FFE8:  sth r4, 0xC(r3) ; blr
    //       → `this->munNumResults = (u16)liNumResults; return this;`. A halfword
    //         store → the count field is 16-bit at offset 0xC.
    //   GetResult     @ 0x828A9EF8:  lwz r10, 0(r31) ; mulli r11, r30, 0x70
    //                                add r3, r11, r10
    //       → `return &mpResults[index];`. Offset 0 is therefore the results-buffer
    //         base pointer (mpResults); the 8 bytes between it and the count are the
    //         list cursor/header the job populates elsewhere (reserved span so
    //         munNumResults keeps the asm-observed console offset 0xC). Note: host
    //         pointer width differs from the X360's 4-byte pointer, so the PC layout
    //         widens mpResults — semantic parity by named member, not byte offsets.
    // -------------------------------------------------------------------------
    struct CollisionResultList
    {
        CollisionResult* mpResults;         // +0x00  results-buffer base (GetResult base)
        u8               maCursorReserved[8]; // +0x04  list cursor/header (X360 offsets)
        u16              munNumResults;      // +0x0C  live result count (sth target)

        // SetNumResults @ 0x8280FFE8
        CollisionResultList* SetNumResults(s32 liNumResults);

        // GetResult @ 0x828A9EF8 — the lu16Index'th result record (asserts in range).
        CollisionResult* GetResult(u16 lu16Index);
    };

    // -------------------------------------------------------------------------
    // PrimitiveTestResult
    //
    // One narrow-phase primitive-vs-primitive test outcome. IsValid (the only
    // recovered function) reads four consecutive 16-byte lanes (offsets 0, 0x10,
    // 0x20, 0x30) and runs a hand-vectorised VMX validation pipeline over them
    // (per-lane NaN test via vcmpeqfp self-compare, then |component| > threshold via
    // a vandc/vrlimi128/vcmpgtfp mask against a rodata magnitude constant). The four
    // lanes are modelled as named vec4 fields at the asm offsets; the VMX body itself
    // is a keystone reconstructed (honest stub) in the .cpp — see CgsCollisionResult.cpp.
    // -------------------------------------------------------------------------
    struct alignas(16) PrimitiveTestResult
    {
        Vector4 mLane0;   // +0x00  (lvx128 v11, r0, r3)
        Vector4 mLane1;   // +0x10  (lvx128 v12, r3, 0x10)
        Vector4 mLane2;   // +0x20  (lvx128 v0,  r3, 0x20)
        Vector4 mLane3;   // +0x30  (lvx128 v0,  r3, 0x30)

        // IsValid @ 0x82921378 — returns true when every lane is finite and within
        // the magnitude threshold. VMX keystone (see .cpp).
        bool IsValid() const;
    };
}
}
