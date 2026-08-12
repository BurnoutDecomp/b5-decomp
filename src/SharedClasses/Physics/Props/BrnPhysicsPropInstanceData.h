#ifndef BRN_PHYSICS_PROPS_PROP_INSTANCE_DATA_H
#define BRN_PHYSICS_PROPS_PROP_INSTANCE_DATA_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Matrix44Affine (embedded by value at +0)

#include <cstddef>            // offsetof (porter-contract pins)

// =============================================================================
// BrnPhysics::Props::PropInstanceData
//   DWARF home: SharedClasses/Physics/Props/BrnPhysicsPropInstanceData.{h,cpp}
//               (references/DecFIGS/dwarfdump/... -- same path, so this file is
//               the canonical home, next to BrnPhysicsPropZoneData.h).
//
// One serialised prop placement inside a PropZoneData (resource 0x10011). The zone
// header points at a flat array of these (PropZoneData::maInstances, 16-aligned at
// +0x30 of the resource); PropZoneManager::LoadProp walks it and hands each record
// to PropEntityInstance::InitialiseFromData.
//
// The record is POINTER-FREE, so its 80-byte stride does NOT widen on the x64 host --
// the host layout IS the on-disk platform-4 form. The porter
// (tools/assets/bundles/world_type_transcode.py `_flip_prop_instance`) byte-swaps it
// in place at that same stride; the static_asserts at the bottom pin that contract.
//
// LAYOUT EVIDENCE (X360 ARTIST asm; every offset has a witness):
//   +0x00  Matrix44Affine mWorldTransform   4x16B rows.
//            - InitialiseFromData 0x822B8130..0x822B8164: `lvx128 v0,r0,r30` /
//              `lvx128 v0,r30,{0x10,0x20,0x30}` -> copied row-for-row into the
//              PropEntityInstance transform.
//            - LoadProp 0x822F2EF0: `_R24 = v38 + 48` then GetCellId(x, z) -- the
//              translation row (wAxis) is the prop's world position.
//            - porter: `_u32_flip_range(src, src+64)` == 16 floats.
//   +0x40  u32 muTypeIdAndFlags   packed { flags:6 | typeId:26 }.
//            - LoadProp: `PropPhysicsDataHeader::GetType(a6, *(v38 + 64) & 0x3FFFFFF)`
//              == GetTypeID() with KI_PROP_TYPEID_MASK.
//            - InitialiseFromData 0x822B8108/0x822B8114: `lwz r11,0x40(r30)` then
//              `sth r11,0x44(r31)` (low half -> PropEntityInstance::muTypeId) and
//              `rlwinm r11,r11,0,5,5` (bit 0x04000000 == flag 0 ==
//              KI_PROP_FLAG_DISABLEPHYSICS, tested against zero).
//   +0x44  u32 muInstanceID
//            - InitialiseFromData 0x822B8198: `lwz r11,0x44(r30)`; `stw r11,0x40(r31)`.
//            - LoadProp: `*(v222 + 68)` streamed as "Instance id: ".
//   +0x48  u16 muAlternativeType   the "respawn changed" replacement prop type.
//            - LoadProp: `PropPhysicsDataHeader::GetType(v115, *(v222 + 72))`, then
//              `*(v221 + 68) = *(v222 + 72)` (a 16-bit store into muTypeId).
//   +0x4A  s8  mn8RotSpeed         packed { axis:2 (top bits) | speed }.
//            - InitialiseFromData 0x822B8268: `lbz r11,0x4A(r30)`; `clrrwi r11,r11,6`
//              then compared against 0 / 0x40 / 0x80 == IsAnimated().
//            - LoadProp: `v91[840314] = *(v38 + 74)` -> PropEntityRotationParams+2
//              (BrnWorld::PropEntityRotationParams::mnRotSpeed).
//   +0x4B  u8  mn8MaxAngle         packed 0..255 == 0..360 deg.
//            - LoadProp: `v91[840316] = *(v38 + 75)` -> PropEntityRotationParams+4
//              (muMaxAngle; UpdateConstraints 0x822A9C68 reads `lbz r11,4(r28)`).
//   +0x4C  u8  mn8MinAngle
//            - LoadProp: `v91[840315] = *(v38 + 76)` -> PropEntityRotationParams+3
//              (muMinAngle; UpdateConstraints 0x822A9C44 reads `lbz r11,3(r28)`).
//   +0x4D  u8  mau8Padding[3]      -> 80-byte, 16-aligned record.
//
// Member NAMES/TYPES and the constant block are the DecFIGS DWARF
// (BrnPhysicsPropInstanceData.h:37..:173), gated on X360 attestation: the X360 build
// emits NO out-of-line PropInstanceData function, so only the accessors an X360 body
// actually folds inline are declared here (the authoring-side setters / Construct /
// FixUp / FixDown / AngleToFixed the DWARF also lists are tool-side and are left out).
// =============================================================================

namespace BrnPhysics
{
namespace Props
{
    // DWARF BrnPhysicsPropInstanceData.h:37-41 -- muTypeIdAndFlags is packed as
    // { flags : 6 | typeId : 26 }. Names/values verbatim from the DWARF (the original
    // spells the unsigned masks `KI_`; keep its spelling).
    static const u32 KI_PROP_FLAG_SHIFT          = 26;
    static const u32 KI_PROP_TYPEID_MASK         = 67108863u;    // 0x03FFFFFF
    static const u32 KI_PROP_FLAG_MASK           = 4227858432u;  // 0xFC000000
    static const s32 KI_PROP_FLAG_DISABLEPHYSICS = 1;            // flag bit 0 == raw 0x04000000

    // DWARF BrnPhysicsPropInstanceData.h:52-56 -- the rotation axis selector packed in
    // the top two bits of mn8RotSpeed. knXAngularRotation carries no DWARF const value
    // (it is the zero encoding); the X360 IsAnimated fold compares 0 / 0x40 / 0x80.
    static const u8 knXAngularRotation    = 0;
    static const u8 knYAngularRotation    = 64;
    static const u8 knZAngularRotation    = 128;
    static const u8 knNoAngularRotation   = 192;
    static const u8 knAngularRotationBits = 192;

    struct alignas(16) PropInstanceData
    {
    public:
        // ---- accessors the X360 folds inline at its call sites ----

        // Full placement transform (rotation rows + world position in wAxis).
        const Matrix44Affine& GetWorldTransform() const { return mWorldTransform; }

        // 26-bit prop type id -- the index PropPhysicsDataHeader::GetType() takes.
        u32 GetTypeID() const { return muTypeIdAndFlags & KI_PROP_TYPEID_MASK; }

        // 6-bit flag field, shifted down (bit 0 == KI_PROP_FLAG_DISABLEPHYSICS).
        u32 GetFlags() const { return (muTypeIdAndFlags & KI_PROP_FLAG_MASK) >> KI_PROP_FLAG_SHIFT; }

        u32 GetInstanceID() const      { return muInstanceID; }
        u32 GetAlternativeType() const { return muAlternativeType; }

        // True when this placement rotates about one of the three axes. The X360 spells
        // the test as three equality compares in exactly this order (0x822B8268..0x822B8288,
        // and again in LoadProp) rather than a single `!= knNoAngularRotation`.
        bool IsAnimated() const
        {
            const u8 lu8Bits = static_cast<u8>(mn8RotSpeed) & knAngularRotationBits;
            return (lu8Bits == knXAngularRotation)
                || (lu8Bits == knYAngularRotation)
                || (lu8Bits == knZAngularRotation);
        }

        s8 GetRotVelocity() const { return mn8RotSpeed; }
        u8 GetMaxAngle() const    { return mn8MaxAngle; }
        u8 GetMinAngle() const    { return mn8MinAngle; }

    public:
        // ---- serialised layout ----
        // The DWARF marks these private behind the accessors above; they are public here
        // so the porter-contract offsetof pins can sit at namespace scope, matching the
        // sibling records (BrnPropGraphicsList.h, BrnPropEntityInstance.h). Read them
        // through the accessors.
        Matrix44Affine mWorldTransform;   // +0x00  (64B, 16-aligned)
        u32            muTypeIdAndFlags;  // +0x40  { flags:6 | typeId:26 }
        u32            muInstanceID;      // +0x44
        u16            muAlternativeType; // +0x48
        s8             mn8RotSpeed;       // +0x4A  { axis:2 | speed }
        u8             mn8MaxAngle;       // +0x4B  packed 0..255 == 0..360 deg
        u8             mn8MinAngle;       // +0x4C  packed 0..255 == 0..360 deg
        u8             mau8Padding[3];    // +0x4D  -> sizeof 80
    };

    // Porter contract (world_type_transcode.py `_flip_prop_instance` /
    // PROP_INSTANCE_STRIDE): pointer-free record, so the host layout must reproduce the
    // on-disk platform-4 form byte for byte.
    static_assert(offsetof(PropInstanceData, mWorldTransform)   == 0x00, "PropInstanceData::mWorldTransform @ +0");
    static_assert(offsetof(PropInstanceData, muTypeIdAndFlags)  == 0x40, "PropInstanceData::muTypeIdAndFlags @ +0x40");
    static_assert(offsetof(PropInstanceData, muInstanceID)      == 0x44, "PropInstanceData::muInstanceID @ +0x44");
    static_assert(offsetof(PropInstanceData, muAlternativeType) == 0x48, "PropInstanceData::muAlternativeType @ +0x48");
    static_assert(offsetof(PropInstanceData, mn8RotSpeed)       == 0x4A, "PropInstanceData::mn8RotSpeed @ +0x4A");
    static_assert(offsetof(PropInstanceData, mn8MaxAngle)       == 0x4B, "PropInstanceData::mn8MaxAngle @ +0x4B");
    static_assert(offsetof(PropInstanceData, mn8MinAngle)       == 0x4C, "PropInstanceData::mn8MinAngle @ +0x4C");
    static_assert(sizeof(PropInstanceData) == 80, "PropInstanceData stride 80 (porter contract)");
    static_assert(alignof(PropInstanceData) == 16, "PropInstanceData 16-aligned (embedded Matrix44Affine)");
}
}

#endif // BRN_PHYSICS_PROPS_PROP_INSTANCE_DATA_H
