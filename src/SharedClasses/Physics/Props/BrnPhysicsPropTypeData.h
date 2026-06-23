#pragma once

#include "types.hpp"

// Minimal owning home for the prop-physics type-data element types that
// BrnPropPhysicsDataHeader.h stores by pointer. The element type names (PropTypeData /
// PropPartTypeData) and the KU_MAX_PROP_TYPES / KU_MAX_PROP_PART_TYPES vocabulary come
// from the DWARF .h hints; the layout/constants below are pinned by the X360 ARTIST asm.
//
// AUTHORITATIVE FACTS (X360 asm):
//   KU_MAX_PROP_TYPES == 500 (0x1F4) -- proven by the bounds assert in
//   PropPhysicsDataHeader::GetType @ 0x82277C50 ("cmplwi r30, 0x1F4").
//
// FLAG (un-homed surface, partial layout): PropTypeData and PropPartTypeData are the
// per-prop / per-part physics descriptor records. Their FULL member layout is NOT yet
// reconstructed. PropTypeData is grown here only as far as the X360 asm exercises it:
//   - PropTypeData::IsLamppost @ 0x822A1A00 reads a 32-bit prop-graphics/name-id field at
//     console byte offset 0x58 (88) and compares it against a fixed set of lamppost
//     prop-ids. That field is modelled by name (muGraphicsId) at the observed offset via a
//     leading reserved span; the rest of the record stays opaque. GROW additively as
//     further PropTypeData recon passes land -- never reorder/retype existing members.
// PropPartTypeData has no bodied function in this pass and stays an opaque forward decl.
// They are stored BY POINTER in BrnPropPhysicsDataHeader.h, so a definition here is safe
// (pointer storage imposes no offset constraint downstream). KU_MAX_PROP_PART_TYPES is a
// best-effort placeholder (referenced but not asm-pinned here) -- it only sizes the
// mapPropPartTypes[] array length in the header, which GetType does not exercise.

namespace BrnPhysics
{
namespace Props
{
    // FLAG: confirmed by GetType's bounds assert (0x1F4).
    static const uint32_t KU_MAX_PROP_TYPES = 500;

    // FLAG: placeholder array bound (not pinned by asm in this pass).
    static const uint32_t KU_MAX_PROP_PART_TYPES = 500;

    // FLAG: opaque descriptor record -- no bodied function reached it in this pass.
    class PropPartTypeData;

    // Per-prop-type physics descriptor. Only the leading reserved span + the
    // graphics/name-id field at console offset 0x58 are modelled (the only member any
    // bodied function reads). Members accessed BY NAME; raw offsets are X360-console
    // facts, not host layout asserts (host has no wider members before muGraphicsId so
    // the offset happens to coincide, but we do not assert it).
    class PropTypeData
    {
    public:
        // PropTypeData::IsLamppost @ 0x822A1A00. True when this prop type is one of the
        // street-furniture lamppost variants. The compared values are the prop-graphics
        // ids baked into the asm (decimal in the pseudocode; verified as the literal
        // `lis/ori` constants in the disasm).
        bool IsLamppost() const;

    private:
        // Opaque leading record preceding the id field at console +0x58 (88).
        u8  maReserved0[0x58];

        // console +0x58 -- 32-bit prop-graphics / name id used by IsLamppost.
        u32 muGraphicsId;
    };
}
}
