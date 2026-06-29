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
//     prop-ids (also re-used at +0x58 by UpdateInstance's overhead-sign test). Modelled
//     by name (muGraphicsId).
//   - PropZoneManager::UpdateInstance @ 0x822F0920 reads TWO collision-volume counts:
//       * the whole-prop count at console +0x5E (94)  -> mu8NumberOfVolumes;
//       * the per-part count via the array pointer at +0x40 (mpaPartVolumeGroups),
//         indexed by muPartId at a 48-byte stride, count at element +0x2C (44)
//         -> PropPartVolumeGroup::mu8NumberOfVolumes.
//     These are the bounds of the per-frame SetVolumeInstanceTransform scene pushes.
//     GROW additively as further PropTypeData recon passes land -- never reorder/retype
//     existing members.
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

    // Per-part-volume-group descriptor. PropZoneManager::UpdateInstance @ 0x822F0920
    // (part path) walks an array of these, reached via PropTypeData::mpaPartVolumeGroups
    // (console +0x40), indexed by the part's muPartId, with a 48-byte (0x30) stride
    // (asm: `add r37, *(type+0x40), 48*partId`). The only member the asm reads from each
    // element is a u8 collision-volume count at element +0x2C (44): the bound of the
    // per-part SetVolumeInstanceTransform loop (`lbz r,0x2C(elem); cmplw idx,r; blt`).
    // The rest of the 48-byte record is unmodelled.
    struct PropPartVolumeGroup
    {
    private:
        u8 maReserved0[0x2C];   // 0x00..0x2B -- opaque (transform / volume-list fields)
    public:
        u8 mu8NumberOfVolumes;  // +0x2C (44) -- collision volumes pushed for this part
    private:
        u8 maReserved1[0x30 - 0x2C - 1]; // -> 48-byte (0x30) console stride
    public:
        u8 GetNumberOfVolumes() const { return mu8NumberOfVolumes; }
    };

    // Per-prop-type physics descriptor. Grown only as far as the X360 asm exercises it:
    // the graphics/name-id at console +0x58 (IsLamppost / overhead-sign check), the
    // part-volume-group array pointer at +0x40, and the prop-level collision-volume count
    // at +0x5E. Members accessed BY NAME; the offsets are X360-console facts (provenance),
    // NOT host layout asserts -- this record is stored BY POINTER in
    // BrnPropPhysicsDataHeader, so host layout imposes no downstream constraint and the
    // console offsets need not byte-match on the PC compile.
    class PropTypeData
    {
    public:
        // PropTypeData::IsLamppost @ 0x822A1A00. True when this prop type is one of the
        // street-furniture lamppost variants. The compared values are the prop-graphics
        // ids baked into the asm (decimal in the pseudocode; verified as the literal
        // `lis/ori` constants in the disasm).
        bool IsLamppost() const;

        // console +0x58 -- prop-graphics / name id (lamppost + overhead-sign checks).
        u32 GetGraphicsId() const { return muGraphicsId; }

        // console +0x5E -- number of collision volumes the whole-prop path pushes to the
        // scene each frame (UpdateInstance prop path: `lbz r,0x5E(type)` loop bound).
        u8 GetNumberOfVolumes() const { return mu8NumberOfVolumes; }

        // console +0x40 -- base of the per-part volume-group array (UpdateInstance part
        // path indexes this by muPartId with a 48-byte stride).
        const PropPartVolumeGroup* GetPartVolumeGroups() const { return mpaPartVolumeGroups; }

    private:
        // Opaque leading record up to the part-volume-group array pointer at console +0x40.
        u8  maReserved0[0x40];

        // console +0x40 -- pointer to the PropPartVolumeGroup[] array (one entry per part).
        const PropPartVolumeGroup* mpaPartVolumeGroups;

        // Opaque span between the +0x40 pointer and the graphics id at console +0x58.
        // (console: 0x58 - 0x44 == 20 bytes after a 32-bit console pointer; host pointer
        // widening makes this span advisory, not asserted.)
        u8  maReserved1[0x58 - 0x40 - sizeof(const PropPartVolumeGroup*)];

        // console +0x58 -- 32-bit prop-graphics / name id used by IsLamppost.
        u32 muGraphicsId;

        // console +0x5C..+0x5D -- opaque (the count at +0x5E is byte-addressed `lbz 0x5E`).
        u8  maReserved2[0x5E - 0x58 - sizeof(u32)];

        // console +0x5E -- prop-level collision-volume count (whole-prop scene push bound).
        u8  mu8NumberOfVolumes;
    };
}
}
