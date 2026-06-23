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
    // CollisionResultList
    //
    // The result list a broad-phase tester job (PolygonSoupTesterJob) writes its
    // hit count into. The only recovered accessor is SetNumResults, which stores a
    // 16-bit count at offset 0xC:
    //
    //   SetNumResults @ 0x8280FFE8:  sth r4, 0xC(r3) ; blr
    //
    // i.e. `this->munNumResults = (u16)liNumResults; return this;`. The store is a
    // halfword (sth) → the count field is 16-bit at offset 0xC. The 12 bytes ahead
    // of it are the list header/cursor the job populates elsewhere; modelled here as
    // a reserved span so munNumResults lands at the asm-observed offset 0xC. Returns
    // `this` (the Hex-Rays `return result;`).
    // -------------------------------------------------------------------------
    struct CollisionResultList
    {
        u8  maHeaderReserved[0xC];  // +0x00  list header/cursor (populated elsewhere)
        u16 munNumResults;          // +0x0C  live result count (sth target)

        // SetNumResults @ 0x8280FFE8
        CollisionResultList* SetNumResults(s32 liNumResults);
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
