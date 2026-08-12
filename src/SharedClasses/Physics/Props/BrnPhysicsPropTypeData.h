#pragma once

#include <cstddef>   // offsetof (the _AssertLayout pins)

#include "types.hpp"
#include "BrnCommonTypes.h"                                       // Vector3 (alignas(16) {x,y,z,w})
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h" // CgsResource::ID (one u64)
#include "SDKs/EATech/rwcollision/volume_debug_access.h"          // rw::collision::Volume (96-byte record)

// BrnPhysics::Props::PropTypeData / PropPartTypeData -- the per-prop-type and per-part
// physics descriptor records held by BrnPropPhysicsDataHeader.h.
//
// THESE ARE EXTERNAL SERIALISED RECORDS, NOT FREE-FLOATING C++ OBJECTS.
// PropPhysicsDataHeader::mapPropTypes[] / mapPropPartTypes[] hold offsets INTO the
// serialised PROPS/PROPPHYSICS.BUNDLE blob, which FixUp turns into pointers by adding the
// resource load base -- the records live inside the resource, so their field offsets are a
// WIRE CONTRACT with the bundle converter and the _AssertLayout() blocks below pin them.
// (An earlier revision of this header claimed the opposite -- "stored BY POINTER ... host
// layout imposes no downstream constraint" -- and reordered mpaPartVolumeGroups behind
// mfLeanCosAngle on that basis. That was wrong: pointer STORAGE says nothing about where
// the pointed-at bytes come from, and these come out of the file.)
//
// SOURCES.
//  * Member set, declaration order, names and types are DecFIGS DWARF ground truth
//    (references/DecFIGS/dwarfdump/SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h and
//    BrnPhysicsPropPartTypeData.h). DWARF spells both `struct`; they are declared `class`
//    here to agree with the forward declarations already committed elsewhere in the tree
//    (BrnPropCellManager.h, BrnPropEntityInstance.h) -- MSVC C4099.
//  * The console offsets those members imply are attested by the X360 ARTIST asm:
//      PropPhysicsDataHeader::FixUp @0x8267F570 relocates PropTypeData +0x3C and +0x40
//      (== maCollisionVolumes, maParts) and PropPartTypeData +0x24 (== maCollisionVolumes);
//      PropTypeData::IsLamppost @0x822A1A00 reads the id at +0x58 (== muSceneUriId);
//      PropZoneManager::UpdateInstance @0x822F0920 reads the whole-prop volume count at
//      +0x5E and walks the part array at a 48-byte console stride with its count at +0x2C;
//      the prop debug renderers (0x822DCBF8 / 0x822DDAE8) read +0x38 mass, +0x3C volumes,
//      +0x44 radius, +0x48 cos, +0x5D parts, +0x5F joint type, +0x60 extra info.
//  * MEASURED against the shipped resource (0xD75C5932, 219 prop types / 131 part types /
//    389 volumes): with alignas(16) Vector3 and a u64 CgsResource::ID every DWARF member
//    lands exactly on its asm-attested console offset, and the three object kinds tile the
//    blob's arena at strides 112 / 48 / 96 with no bytes left over.
//
// CONSOLE -> HOST.
//    PropTypeData     112 -> 112  (unchanged: widening mfMass's tail pad and the two
//                                  pointers exactly consumes the console record's 15-byte
//                                  trailing pad; maCollisionVolumes/maParts move
//                                  +0x3C/+0x40 -> +0x40/+0x48)
//    PropPartTypeData  48 ->  64  (maCollisionVolumes +0x24 -> +0x28)
//    rw::collision::Volume  96 -> 96 (opaque serialised record, no widening)
// tools/assets/bundles/world_type_transcode.py::transcode_propphysics emits exactly this.

namespace rw { class Resource; }

namespace BrnPhysics
{
namespace Props
{
    // Fixed table bounds of PropPhysicsDataHeader's three arrays. All three are pinned by
    // the array base offsets in PropPhysicsDataHeader::FixUp @0x8267F570 (+0x10, +0x7E0,
    // +0xC90 => 500 and 300 four-byte slots between them) and agree with the DWARF
    // declarations `PropTypeData *[500]` / `PropPartTypeData *[300]`. KU_MAX_PROP_TYPES is
    // independently confirmed by GetType's bounds assert @0x82277C50 (`cmplwi r30, 0x1F4`).
    static const uint32_t KU_MAX_PROP_TYPES      = 500;
    static const uint32_t KU_MAX_PROP_PART_TYPES = 300;

    // Per-part physics descriptor. DWARF BrnPhysicsPropPartTypeData.h:52.
    // An earlier recon pass called this PropPartVolumeGroup, having reached it only through
    // PropTypeData's part-array pointer; it is the same record, and the alias below keeps
    // that spelling compiling.
    class PropPartTypeData
    {
    public:
        const Vector3& GetOffset() const           { return mOffset; }
        const Vector3& GetInertia() const          { return mInertia; }
        f32            GetMass() const             { return mfMass; }
        f32            GetBoundingRadius() const   { return mfSphereRadius; }   // mfSphereRadius
        u8             GetNumberOfVolumes() const  { return muNumberOfVolumes; }

        // The luIndex-th collision volume of this part. The serialised volume records are a
        // contiguous run from maCollisionVolumes (proven over every part in the shipped
        // resource), so indexing the array is the whole of the console's `base + 96*i`.
        // Returns a NON-const Volume* (the const applies to the member pointer, not the
        // pointee): the volume is mutable scene state -- FixableVolume::FixUp rewrites its
        // type slot -- and the committed debug/scene callers rely on that.
        ::rw::collision::Volume* GetCollisionVolume(u32 luIndex) const
        {
            return maCollisionVolumes + luIndex;
        }

        // DWARF BrnPhysicsPropPartTypeData.h:72/:75. The X360 folded both bodies into
        // PropPhysicsDataHeader::FixUp/FixDown @0x8267F570/0x8267F658, which relocate this
        // record's single pointer slot (console +0x24) inline; they are outlined back here.
        void FixUp(const ::rw::Resource& lrBaseResource);
        void FixDown(const ::rw::Resource& lrBaseResource);

        static void _AssertLayout();

    private:
        Vector3                  mOffset;              // console +0x00
        Vector3                  mInertia;             // console +0x10
        f32                      mfMass;               // console +0x20
        ::rw::collision::Volume* maCollisionVolumes;   // console +0x24 -> host +0x28 (FixUp slot)
        f32                      mfSphereRadius;       // console +0x28 -> host +0x30
        u8                       muNumberOfVolumes;    // console +0x2C -> host +0x34
    };

    // The name the first recon pass gave PropPartTypeData, reached only via
    // PropTypeData::GetPartVolumeGroups(). Kept so the committed prop debug/zone code that
    // already spells it that way keeps compiling.
    typedef PropPartTypeData PropPartVolumeGroup;

    // Per-prop-type physics descriptor. DWARF BrnPhysicsPropTypeData.h:51.
    class PropTypeData
    {
    public:
        // -- DWARF-named accessors -------------------------------------------------
        const Vector3&    GetJointLocator() const   { return mJointLocator; }
        const Vector3&    GetCOMOffset() const      { return mCOMOffset; }
        const Vector3&    GetInertia() const        { return mInertia; }
        CgsResource::ID   GetResourceId() const     { return mResourceId; }
        f32               GetMass() const           { return mfMass; }
        f32               GetLeanThreshold() const  { return mfLeanThreshold; }
        f32               GetMoveThreshold() const  { return mfMoveThreshold; }
        f32               GetSmashThreshold() const { return mfSmashThreshold; }
        u8                GetMaxState() const       { return muMaxState; }
        u8                GetJointType() const      { return mu8JointType; }
        u8                GetExtraTypeInfo() const  { return mu8ExtraTypeInfo; }

        // -- accessors the committed prop code already spells this way --------------
        // (the earlier recon named these from what the asm did with them, before the DWARF
        //  member names were matched up; both spellings are kept so nothing breaks)
        f32  GetBoundingRadius() const  { return mfSphereRadius; }      // mfSphereRadius
        f32  GetLeanCosAngle() const    { return mfMaxJointAngleCos; }  // mfMaxJointAngleCos
        u32  GetGraphicsId() const      { return muSceneUriId; }        // muSceneUriId
        u32  GetNumberOfParts() const   { return muNumberOfParts; }
        u8   GetNumberOfVolumes() const { return muNumberOfVolumes; }
        u8   GetLeanState() const       { return mu8JointType; }        // mu8JointType

        // DWARF BrnPhysicsPropTypeData.h:104. mu8ExtraTypeInfo == 1 selects the prop types
        // the debug overlay draws as traffic lights (RenderTrafficLights @0x822DD868).
        bool IsTrafficLight() const     { return mu8ExtraTypeInfo == 1; }

        // DWARF BrnPhysicsPropTypeData.h:101. A prop is smashable iff it has parts to shed.
        // Attested by the X360 at the inlined call site in
        // PropEntityInstance::InitialiseFromData @0x822B80E8:
        //   `if ( *(type + 93) != 0 ) *(this + 74) |= 8u;`   (93 == 0x5D == muNumberOfParts)
        bool IsSmashable() const        { return muNumberOfParts != 0; }

        // DWARF BrnPhysicsPropTypeData.h:107, bodied @0x822A1A00 in the sibling .cpp.
        bool IsLamppost() const;

        // The luIndex-th whole-prop collision volume / part descriptor. Both tables are
        // contiguous runs from the stored base (proven over every record in the shipped
        // resource), which is what the console's `base + 96*i` / `base + 48*i` amounts to.
        ::rw::collision::Volume* GetCollisionVolume(u32 luIndex) const
        {
            return maCollisionVolumes + luIndex;
        }
        const PropPartTypeData* GetPartVolumeGroups() const { return maParts; }
        const PropPartTypeData* GetParts() const            { return maParts; }

        // DWARF BrnPhysicsPropTypeData.h:85/:88. Folded inline into
        // PropPhysicsDataHeader::FixUp/FixDown by the X360 compiler (the two relocations at
        // console +0x3C/+0x40 in that body); outlined back here. NOTE they relocate this
        // record's own slots ONLY -- the header separately walks mapVolumeTypes and calls
        // FixableVolume::FixUp, so the volumes are not double-relocated through here.
        void FixUp(const ::rw::Resource& lrBaseResource);
        void FixDown(const ::rw::Resource& lrBaseResource);

        static void _AssertLayout();

    private:
        Vector3                  mJointLocator;        // console +0x00
        Vector3                  mCOMOffset;           // console +0x10
        Vector3                  mInertia;             // console +0x20
        CgsResource::ID          mResourceId;          // console +0x30 (u64)
        f32                      mfMass;               // console +0x38
        ::rw::collision::Volume* maCollisionVolumes;   // console +0x3C -> host +0x40 (FixUp slot 0)
        PropPartTypeData*        maParts;              // console +0x40 -> host +0x48 (FixUp slot 1)
        f32                      mfSphereRadius;       // console +0x44 -> host +0x50
        f32                      mfMaxJointAngleCos;   // console +0x48 -> host +0x54
        f32                      mfLeanThreshold;      // console +0x4C -> host +0x58
        f32                      mfMoveThreshold;      // console +0x50 -> host +0x5C
        f32                      mfSmashThreshold;     // console +0x54 -> host +0x60
        u32                      muSceneUriId;         // console +0x58 -> host +0x64
        u8                       muMaxState;           // console +0x5C -> host +0x68
        u8                       muNumberOfParts;      // console +0x5D -> host +0x69
        u8                       muNumberOfVolumes;    // console +0x5E -> host +0x6A
        u8                       mu8JointType;         // console +0x5F -> host +0x6B
        u8                       mu8ExtraTypeInfo;     // console +0x60 -> host +0x6C
    };

    // ---- x64 layout pins ---------------------------------------------------------
    // PROPS/PROPPHYSICS.BUNDLE is consumed in place, so these offsets are a WIRE CONTRACT
    // with tools/assets/bundles/world_type_transcode.py::transcode_propphysics (constants
    // PT_OUT_VOLS / PT_OUT_PARTS / PT_OUT_TAIL / PROP_TYPE_STRIDE / PROP_PART_OUT /
    // PP_OUT_VOLS). Anything that shifts a member here must shift the converter's emitted
    // image in the same step -- the two silently disagreeing is the STREETDATA failure mode.
    inline void PropPartTypeData::_AssertLayout()
    {
        static_assert( sizeof( Vector3 ) == 16, "Vector3 is a 16-byte SIMD lane quad" );
        static_assert( sizeof( ::rw::collision::Volume ) == 96,
                       "rwcollision Volume is a 96-byte serialised record on both targets" );
        static_assert( offsetof( PropPartTypeData, mfMass )             == 0x20, "mfMass @0x20" );
        static_assert( offsetof( PropPartTypeData, maCollisionVolumes ) == 0x28,
                       "PropPartTypeData::maCollisionVolumes @0x28 (console +0x24)" );
        static_assert( offsetof( PropPartTypeData, mfSphereRadius )     == 0x30, "mfSphereRadius @0x30" );
        static_assert( offsetof( PropPartTypeData, muNumberOfVolumes )  == 0x34, "muNumberOfVolumes @0x34" );
        static_assert( sizeof( PropPartTypeData ) == 64,
                       "PropPartTypeData stride is 64 on x64 (console 48)" );
    }

    inline void PropTypeData::_AssertLayout()
    {
        static_assert( offsetof( PropTypeData, mCOMOffset )        == 0x10, "mCOMOffset @0x10" );
        static_assert( offsetof( PropTypeData, mInertia )          == 0x20, "mInertia @0x20" );
        static_assert( offsetof( PropTypeData, mResourceId )       == 0x30, "mResourceId @0x30" );
        static_assert( offsetof( PropTypeData, mfMass )            == 0x38, "mfMass @0x38" );
        static_assert( offsetof( PropTypeData, maCollisionVolumes) == 0x40,
                       "PropTypeData::maCollisionVolumes @0x40 (console +0x3C)" );
        static_assert( offsetof( PropTypeData, maParts )           == 0x48,
                       "PropTypeData::maParts @0x48 (console +0x40)" );
        static_assert( offsetof( PropTypeData, mfSphereRadius )    == 0x50, "mfSphereRadius @0x50" );
        static_assert( offsetof( PropTypeData, mfMaxJointAngleCos )== 0x54, "mfMaxJointAngleCos @0x54" );
        static_assert( offsetof( PropTypeData, mfSmashThreshold )  == 0x60, "mfSmashThreshold @0x60" );
        static_assert( offsetof( PropTypeData, muSceneUriId )      == 0x64, "muSceneUriId @0x64" );
        static_assert( offsetof( PropTypeData, muNumberOfParts )   == 0x69, "muNumberOfParts @0x69" );
        static_assert( offsetof( PropTypeData, muNumberOfVolumes ) == 0x6A, "muNumberOfVolumes @0x6A" );
        static_assert( offsetof( PropTypeData, mu8ExtraTypeInfo )  == 0x6C, "mu8ExtraTypeInfo @0x6C" );
        static_assert( sizeof( PropTypeData ) == 112,
                       "PropTypeData stride is 112 on both targets" );
    }
}
}
