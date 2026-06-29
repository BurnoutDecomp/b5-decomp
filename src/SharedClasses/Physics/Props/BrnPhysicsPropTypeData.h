#pragma once

#include "types.hpp"

namespace rw { namespace collision { class Volume; } }

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
        u8 maReserved0[0x20];   // 0x00..0x1F -- opaque (per-part transform fields)
    public:
        // ---- per-part draw fields the prop debug overlay reads (RenderProps broken path,
        //      0x822DCBF8). Console offsets pinned by that body (provenance; this record is
        //      reached via the mpaPartVolumeGroups array pointer, so host pointer widening
        //      shifts the absolute offsets -- the names, not the byte offsets, are load-bearing):
        //   +0x20 (32) -- per-part rigid-body mass        ("Mass: " stat; `lfs f1,0x20(r29)`).
        //   +0x24 (36) -- per-part collision-volume[] base (volume i = base + 96*i;
        //                 `lwz r,0x24(r29)` + `add r,r,96*i`).
        //   +0x28 (40) -- per-part bounding-sphere radius (trailing DrawHollowSphere;
        //                 `lfs f1,0x28(r29)`).
        //   +0x2C (44) -- collision-volume count (existing; UpdateInstance part-loop bound).
        f32 mfMass;                                   // console +0x20
        ::rw::collision::Volume* mpaCollisionVolumes; // console +0x24 (per-part volume array)
        f32 mfBoundingRadius;                          // console +0x28
        u8  mu8NumberOfVolumes;                        // console +0x2C
    private:
        u8 maReserved1[3];      // -> 48-byte (0x30) console stride (advisory on host)
    public:
        u8  GetNumberOfVolumes() const { return mu8NumberOfVolumes; }
        f32 GetMass() const { return mfMass; }
        f32 GetBoundingRadius() const { return mfBoundingRadius; }
        // The luIndex-th per-part collision volume (96-byte console stride, like the whole-prop
        // array; RenderProps broken path advances the base ptr by 0x60 per volume).
        ::rw::collision::Volume* GetCollisionVolume(u32 luIndex) const
        {
            return reinterpret_cast<::rw::collision::Volume*>(
                reinterpret_cast<u8*>(mpaCollisionVolumes) + 96u * luIndex);
        }
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

        // ---- accessors the prop debug overlay reaches. These read whole-prop fields the
        //      X360 prop debug renderers (RenderProps 0x822DCBF8 / RenderPropStats 0x822DDAE8
        //      / RenderTrafficLights 0x822DD868) pull off the PropTypeData record. The console
        //      offsets are provenance (this record is stored BY POINTER, so host pointer
        //      widening makes the absolute offsets advisory -- the NAMES are load-bearing):
        //   GetMass          -- console +0x38: rigid-body mass ("Mass: " stat; `lfs f1,0x38(r21)`).
        //   GetCollisionVolume(i) -- console +0x3C: whole-prop collision-volume[] base; volume i
        //                      = base + 96*i (`lwz r,0x3C(r21)` + `add r,r,96*i`).
        //   GetBoundingRadius-- console +0x44: bounding-sphere radius (trailing
        //                      DrawHollowSphere; `lfs f1,0x44(r21)`).
        //   GetLeanCosAngle  -- console +0x48: cos(lean limit); acos'd into the "Lean angle: "
        //                      stat (`lfs f1,0x48(r21)`; bl acos).
        //   GetNumberOfParts -- console +0x5D: part count (RenderProps/RenderPropStats broken-
        //                      path loop bound; `lbz r,0x5D(r21)`).
        //   GetLeanState     -- console +0x5F: lean-animation state; the "Lean angle:" stat is
        //                      emitted only when it is 1 or 2 (`lbz r,0x5F(r21)`).
        //   IsTrafficLight   -- console +0x60: == 1 selects the props drawn as traffic lights.
        f32 GetMass() const            { return mfMass; }
        f32 GetBoundingRadius() const  { return mfBoundingRadius; }
        f32 GetLeanCosAngle() const    { return mfLeanCosAngle; }
        u32 GetNumberOfParts() const   { return mu8NumberOfParts; }
        u8  GetLeanState() const       { return mu8LeanState; }
        bool IsTrafficLight() const    { return mu8TrafficLight == 1; }
        ::rw::collision::Volume* GetCollisionVolume(u32 luIndex) const
        {
            return reinterpret_cast<::rw::collision::Volume*>(
                reinterpret_cast<u8*>(mpaCollisionVolumes) + 96u * luIndex);
        }

    private:
        // ---- console layout (offsets are provenance; host pointer widening shifts the
        //      absolute positions, but the per-frame/debug bodies reach these BY NAME only).
        //      Members are declared in console-offset order; the reserved gaps are advisory. ----
        u8  maReserved0[0x38];                          // 0x00..0x37 -- opaque leading record
        f32 mfMass;                                     // console +0x38
        ::rw::collision::Volume* mpaCollisionVolumes;   // console +0x3C -- whole-prop volume array
        f32 mfBoundingRadius;                           // console +0x44
        f32 mfLeanCosAngle;                             // console +0x48
        const PropPartVolumeGroup* mpaPartVolumeGroups; // console +0x40 (after host widening)
        u8  maReserved1[16];                            // span up to the graphics id (advisory)
        u32 muGraphicsId;                               // console +0x58 -- used by IsLamppost
        u8  maReserved2[4];                             // console +0x5C span (advisory)
        u8  mu8NumberOfParts;                           // console +0x5D
        u8  mu8NumberOfVolumes;                         // console +0x5E -- whole-prop volume count
        u8  mu8LeanState;                               // console +0x5F
        u8  mu8TrafficLight;                            // console +0x60
    };
}
}
