#pragma once

// CgsSceneManager::Contact — one resolved contact point produced by the scene
// manager's overlap-culling narrow phase, plus the source collision-pair result it
// is populated from.
//
// DWARF assert home: CgsSceneManagerTypes.h (the two Construct asserts fire there at
// lines 157/158). Reconstructed store-for-store from the X360 asm
// (Contact::Construct @ 0x828A9E30). Only the fields the recovered Construct touches
// are named; the (large) source result aggregate's other bytes are preserved as
// reserved spans so the asm-observed field offsets stay console-faithful — this is a
// faithful named-field layout, not a raw-offset cast.

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector4 (16-byte / 16-aligned lane)

namespace CgsSceneManager
{
    // -------------------------------------------------------------------------
    // Contact — the 64-byte contact record Construct() fills (16-aligned: carries
    // vec4 lanes). Offsets from the Contact::Construct stores into r30:
    //   +0x00  mPosition   (stvx128 v0, r0,  r30)   — point copied from source[idx]
    //   +0x10  mImpulse    (stvx128 v0, r30, r8=0x10)— second per-point vec4
    //   +0x20  mNormal     (stvx128 v0, r30, r9=0x20)— source normal lane, sign-flipped
    //   +0x30  muVolumeInstanceA  (NOT written by Construct -- see below)
    //   +0x34  muVolumeInstanceB  (NOT written by Construct -- see below)
    //   +0x38  muFieldA    (stw r11, 0x38(r30) <- source[+4])
    //   +0x3C  muFieldB    (stw r11, 0x3C(r30) <- source[+0xC])
    //
    // ⭐ THE +0x30 / +0x34 PAIR WAS NAMED 2026-08-19 (wave Q5, cluster E3a). It used to
    // be `u8 maReserved30[8]` because Contact::Construct never touches it. Its
    // producer is the culler's DoPairQuery @0x828C1A18, which stamps the two
    // volume-instance indices into the freshly Constructed contact immediately BEFORE
    // pushing it -- twice, once per arm:
    //     prim x prim  0x828C1B20 `stw r17, var_7F0` / 0x828C1B28 `stw r16, var_7EC`
    //     prim x agg   0x828C1C7C `stw r17, var_7F0` / 0x828C1C84 `stw r16, var_7EC`
    // with var_7F0 == var_820+0x30 and var_7EC == var_820+0x34 (var_820 being the
    // stack Contact), and r17/r16 the DoPairQuery arguments the caller took straight
    // out of OverlappingPair::muVolumeInstanceA / ::muVolumeInstanceB.
    // DWARF confirms both names and their position in the member list:
    //     references/DecFIGS/dwarfdump/.../CgsSceneManagerTypes.h:231
    //         uint32_t muVolumeInstanceA;  // CgsSceneManagerTypes.h:107
    //         uint32_t muVolumeInstanceB;  // CgsSceneManagerTypes.h:108
    // No layout change, no symbol change: eight reserved bytes become two named u32s
    // at the same offsets. (Edited out of cluster E3a's file grant -- DoPairQuery
    // cannot stamp these fields without them; reported in
    // scratchpad/waveQ5/e3a.owner.md.)
    //
    // ⚠️ THREE MORE DWARF DELTAS IN THIS FILE, REPORTED NOT APPLIED (they change
    // names/signatures other TUs bind to, so they belong to this file's owner):
    //   * `mPosition` / `mImpulse` are really `mPointOnA` / `mPointOnB` (DWARF
    //     CgsSceneManagerTypes.h:104/:105) -- Construct copies them from the source
    //     result's pointsOn1[i] / pointsOn2[i], which are two CONTACT POINTS, one per
    //     side. Neither is an impulse; nothing in the scene manager computes an
    //     impulse. A wrong name here is a live trap for the world/physics bridges that
    //     will read these next (they need pointOnA/pointOnB, not a "position").
    //   * `muFieldA` / `muFieldB` are `muPolyTagA` / `muPolyTagB` (DWARF :109/:110) --
    //     the source's tag1/tag2 user tags (PrimitivePairIntersectResult +0x04/+0x0C).
    //   * `Contact::SourceResult` below is a FORK of rw::collision::
    //     PrimitivePairIntersectResult, which now has a real home at
    //     vendor/renderware/collision/GPInstance.hpp:320 (same 0x750 stride, same
    //     normal/pointsOn1/pointsOn2/numPoints offsets). DWARF declares
    //     `void Construct(PrimitivePairIntersectResult*, uint32_t)`
    //     (CgsSceneManagerTypes.h:102). Retyping the parameter would change
    //     Contact::Construct's mangled name and pull the vendor narrow-phase header
    //     into every includer of this file, so it is left for a coordinated step;
    //     DoPairQuery casts between the two spellings at its two call sites with the
    //     equivalence static_asserted in that TU.
    // -------------------------------------------------------------------------
    struct alignas(16) Contact
    {
        Vector4 mPosition;             // +0x00
        Vector4 mImpulse;              // +0x10
        Vector4 mNormal;               // +0x20  (negated source normal)
        u32     muVolumeInstanceA;     // +0x30  DWARF :107 (stamped by DoPairQuery)
        u32     muVolumeInstanceB;     // +0x34  DWARF :108 (stamped by DoPairQuery)
        u32     muFieldA;              // +0x38  (<- source +0x04)
        u32     muFieldB;              // +0x3C  (<- source +0x0C)

        // Source collision-pair result Construct() reads from. It is the large
        // per-pair aggregate OverlapCullingModule::DoPairQuery builds; only the
        // fields Construct observes are named, the rest is reserved so the named
        // fields land at their asm offsets. Two parallel per-contact-point vec4
        // arrays sit at base 0x500 (slot index 80) and 0x600 (slot index 96), each
        // indexed by the contact-point index; muNumPoints (offset 0x740) bounds them.
        struct alignas(16) SourceResult
        {
            u8      maReserved00[4];       // +0x000
            u32     muFieldA;              // +0x004  -> Contact::muFieldA
            u8      maReserved08[4];       // +0x008
            u32     muFieldB;              // +0x00C  -> Contact::muFieldB
            u8      maReserved10[0x4B0];   // +0x010
            Vector4 mNormal;               // +0x4C0  (negated into Contact::mNormal)
            u8      maReserved4D0[0x30];   // +0x4D0
            Vector4 maPositions[16];       // +0x500  (slot 80*16): per-point position
            Vector4 maImpulses[16];        // +0x600  (slot 96*16): per-point impulse
            u8      maReserved700[0x40];   // +0x700
            u32     muNumPoints;           // +0x740  contact-point count (assert bound)
        };

        // Construct @ 0x828A9E30 — populate this contact from luContactPointIndex of
        // lpResult: copies the per-point position/impulse lanes, the sign-flipped
        // pair normal, and the two u32 fields. Asserts lpResult != NULL (157) and
        // luContactPointIndex < lpResult->muNumPoints (158). Returns this.
        Contact* Construct(const SourceResult* lpResult, u32 luContactPointIndex);
    };
}
