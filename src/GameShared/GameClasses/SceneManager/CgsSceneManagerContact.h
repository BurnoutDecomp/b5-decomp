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
    //   +0x30  (reserved 8 bytes)
    //   +0x38  muFieldA    (stw r11, 0x38(r30) <- source[+4])
    //   +0x3C  muFieldB    (stw r11, 0x3C(r30) <- source[+0xC])
    // -------------------------------------------------------------------------
    struct alignas(16) Contact
    {
        Vector4 mPosition;             // +0x00
        Vector4 mImpulse;              // +0x10
        Vector4 mNormal;               // +0x20  (negated source normal)
        u8      maReserved30[8];       // +0x30
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
