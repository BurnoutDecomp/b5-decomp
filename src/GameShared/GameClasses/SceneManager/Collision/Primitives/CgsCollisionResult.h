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
#include "BrnCommonTypes.h"   // Vector3 / Vector3Plus / Vector4 (rw::math::vpu, 16-byte lanes)

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
    //       → `this->mu16NumResults = (u16)liNumResults; return this;`. A halfword
    //         store → the count field is 16-bit at offset 0xC.
    //   GetResult     @ 0x828A9EF8:  lwz r10, 0(r31) ; mulli r11, r30, 0x70
    //                                add r3, r11, r10
    //       → `return &mpResults[index];`. Offset 0 is therefore the results-buffer
    //         base pointer (mpResults). Note: host pointer width differs from the
    //         X360's 4-byte pointer, so the PC layout widens mpResults — semantic
    //         parity by named member, not byte offsets.
    //
    // ⭐ 2026-08-14 (walls leg 1): the "8 reserved bytes between pointer and count" are
    // RETIRED — the DecFIGS DWARF (CgsCollisionResultList.h:163-169) names every field, and
    // BaseCollisionGenerator::PrepareNewPrimitiveTestResultsList @0x82810798 writes all six
    // in one burst (stw/stw/sth/sth/sth/stb at +0/+4/+8/+A/+C/+E):
    //     mpResults(mpaResults) / mu32UserTagA / mu16UserTagB / mu16MaxNumResults /
    //     mu16NumResults / meResultType.
    // The collide-stream drivers pass the BRIDGE'S custom-queue index through UserTagA
    // (`li 5` race-world / `li 9` traffic-world / 7/8/13 car-car) and a constant 1 through
    // UserTagB — tags, not semantics the list itself interprets.
    // (mu16NumResults was spelled `munNumResults` here before the DWARF confirmed the
    // canonical spelling; SetNumResults/GetResult updated with it.)
    // -------------------------------------------------------------------------
    struct CollisionResultList
    {
        CollisionResult* mpResults;         // +0x00  results-buffer base (GetResult base; DWARF mpaResults)
        u32              mu32UserTagA;       // +0x04  caller tag A (custom-queue index on the vehicle paths)
        u16              mu16UserTagB;       // +0x08  caller tag B (the drivers pass 1)
        u16              mu16MaxNumResults;  // +0x0A  capacity the results buffer was sized for
        u16              mu16NumResults;     // +0x0C  live result count (sth target)
        u8               meResultType;       // +0x0E  result-record type id (0 == primitive-test, 80-byte stride)

        // SetNumResults @ 0x8280FFE8
        CollisionResultList* SetNumResults(s32 liNumResults);

        // GetResult @ 0x828A9EF8 — the lu16Index'th result record (asserts in range).
        CollisionResult* GetResult(u16 lu16Index);
    };

    // -------------------------------------------------------------------------
    // PrimitiveTestResult
    //
    // One narrow-phase primitive-vs-primitive test outcome -- the 80-byte record
    // the sphere-vs-triangle contact worker queues per hit, and the record type
    // meResultType == 0 lists carry (PrepareNewPrimitiveTestResultsList Mallocs
    // 80 * max; the worker copies 10 dwords at `80 * index + base`).
    //
    // ⭐ 2026-08-14 (walls leg 2): every field is now DWARF-NAMED verbatim
    // (DecFIGS dwarfdump CgsCollisionResult.h:60-70), retiring the "four
    // anonymous lanes" model. On the sphere-vs-triangle path Primitive0 is the
    // TRIANGLE and Primitive1 is the SPHERE -- proven by the kernel call-site's
    // record offsets (lTriangleNormal -> +0x00, lContactNormal -> +0x10,
    // lTriangleContactPoint -> +0x20, lSphereContactPoint -> +0x30) and the
    // worker's tail stores (surface tag -> +0x40, 0 -> +0x44, triangle index ->
    // +0x48 sth, sphere index -> +0x4A sth).
    // ⚠️ mu16TestIndex/muPad (+0x4C/+0x4E) are NOT written by the sphere worker
    // -- on the console they carry stack garbage into the queued copy. The
    // reconstruction zero-initialises them at the one build site instead
    // (deterministic; garbage is not reproducible), flagged there.
    // -------------------------------------------------------------------------
    struct alignas(16) PrimitiveTestResult
    {
        Vector3     mPrimitive0Normal;    // +0x00  DWARF :60 (triangle side's normal)
        Vector3     mPrimitive1Normal;    // +0x10  DWARF :61 (sphere side's normal; the kernel
                                          //         writes the SAME contact direction to both)
        Vector3Plus mPrimitive0Contact;   // +0x20  DWARF :62 (triangle contact point)
        Vector3Plus mPrimitive1Contact;   // +0x30  DWARF :63 (sphere contact point)
        u32     muPrimitive0Tag;      // +0x40  DWARF :64 (triangle surface tag)
        u32     muPrimitive1Tag;      // +0x44  DWARF :65 (sphere side: the worker stores 0)
        u16     muPrimitive0Index;    // +0x48  DWARF :66 (triangle index: 4*batch + lane)
        u16     muPrimitive1Index;    // +0x4A  DWARF :67 (sphere index)
        u16     mu16TestIndex;        // +0x4C  DWARF :68 (unwritten by the sphere worker)
        u16     muPad;                // +0x4E  DWARF :70

        // IsValid @ 0x82921378 — all four vectors finite in xyz, and both normal
        // vectors non-degenerate (some |component| > 2^-23). See the .cpp.
        bool IsValid() const;
    };

    static_assert(sizeof(PrimitiveTestResult) == 80,
                  "PrimitiveTestResult is the 80-byte result-list record "
                  "(pointer-free; the list stride is a contract)");
}
}
