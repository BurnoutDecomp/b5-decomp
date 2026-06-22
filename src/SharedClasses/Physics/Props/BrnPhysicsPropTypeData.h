#pragma once

#include "types.hpp"

// Minimal owning home for the prop-physics type-data element types that
// BrnPropPhysicsDataHeader.h stores by pointer. Reconstructed from the Feb-2007 leak
// (which #includes "Physics/Props/BrnPhysicsPropTypeData.h" and names PropTypeData /
// PropPartTypeData / KU_MAX_PROP_TYPES / KU_MAX_PROP_PART_TYPES) + the X360 ARTIST asm.
//
// AUTHORITATIVE FACTS (X360 asm):
//   KU_MAX_PROP_TYPES == 500 (0x1F4) -- proven by the bounds assert in
//   PropPhysicsDataHeader::GetType @ 0x82277C50 ("cmplwi r30, 0x1F4").
//
// FLAG (un-homed surface, HONEST placeholder): PropTypeData and PropPartTypeData are the
// per-prop / per-part physics descriptor records. Their full member layout is NOT yet
// reconstructed (no asm/DWARF in this recon pass beyond the pointer-array storage in the
// header). They are forward-declared here so the header that stores them BY POINTER compiles
// with correct pointer-array layout; the owning bodies must GROW these types additively when a
// recon pass reaches PropTypeData / PropPartTypeData. KU_MAX_PROP_PART_TYPES is likewise a
// best-effort placeholder (the leak references it but no asm pins it here) -- it only affects the
// mapPropPartTypes[] array length in the header, which is not exercised by GetType.

namespace BrnPhysics
{
namespace Props
{
    // FLAG: confirmed by GetType's bounds assert (0x1F4).
    static const uint32_t KU_MAX_PROP_TYPES = 500;

    // FLAG: placeholder array bound (not pinned by asm in this pass).
    static const uint32_t KU_MAX_PROP_PART_TYPES = 500;

    // FLAG: opaque descriptor records -- grow additively when their layout is reconstructed.
    class PropTypeData;
    class PropPartTypeData;
}
}
