#ifndef BRN_STATIC_SOUND_MAP_H
#define BRN_STATIC_SOUND_MAP_H

#include "types.hpp"          // s32, u32, f32
#include "BrnCommonTypes.h"   // Vector2 (= rw::math::vpu::Vector2, 16-byte SIMD register)

namespace BrnSound
{
namespace World
{
// StaticSoundEntity — one static sound-map entity. DWARF (DecFIGS
// BrnStaticSoundMap.h:59): a single `Vector3Plus mPosPlus` — the position xyz with
// the radius/type payload packed into the w lane (the :136 UFloatHelper union is
// the pack/unpack view; Construct(Vector3, f32, u16) packs both). 16 bytes,
// pointer-free — the platform-4 serialised element `{f32 x,y,z, u16, u16}` (see
// the porter contract note below) is this record verbatim. COMPLETED 2026-08-25
// (audio-faithfulness wave 2; was a forward-decl that forced GetEntity into a
// hardcoded byte-stride walk). The accessor/FixUp bodies (GetPos @h:89, GetRadius
// @h:103, GetType @h:110, FixUp @h:117, Construct @h:74) stay DEFERRED to the
// entity's own TU — only the layout is load-bearing here.
struct StaticSoundEntity
{
    // BrnStaticSoundMap.h:136 (DWARF) — the w-lane pack/unpack view.
    union UFloatHelper
    {
        f32 mfPlusComponent;   // :137
        u32 mu32Bits;          // :138
    };

    const Vector3Plus& GetPosPlus() const { return mPosPlus; }
    Vector3 GetPos() const
    {
        Vector3 lPosition = { mPosPlus.x, mPosPlus.y, mPosPlus.z, 0.0f };
        return lPosition;
    }
    f32 GetRadius() const
    {
        UFloatHelper lPacked;
        lPacked.mfPlusComponent = mPosPlus.w;
        return static_cast<f32>(static_cast<u16>(lPacked.mu32Bits & 0xffffu));
    }
    u16 GetType() const
    {
        UFloatHelper lPacked;
        lPacked.mfPlusComponent = mPosPlus.w;
        return static_cast<u16>(lPacked.mu32Bits >> 16);
    }

protected:
    // BrnStaticSoundMap.h:152 (DWARF). Position + packed w payload.
    Vector3Plus mPosPlus;
};
static_assert(sizeof(StaticSoundEntity) == 16,
              "StaticSoundEntity is one 16-byte Vector3Plus (porter contract)");

// SubRegionDescriptor — the per-cell descriptor of the XZ sub-region grid.
// ADDITIVE GROW (was a deferred forward-decl): GetSubRegionDescrip @ 0x8267AF10
// returns &mpSubRegions[miNumSubRegionsX * iz + ix], and the X360 indexing
// scales the cell index by 4 (`slwi r11, r11, 2`), proving a 4-byte element
// stride. Modelled here as a single 4-byte opaque cell so the accessor can return
// a real element pointer by index without a raw-offset cast. FLAG: only the
// 4-byte stride is X360-attested; the cell's internal field layout is still
// DEFERRED to the SubRegionDescriptor TU — grow this struct additively there.
struct SubRegionDescriptor
{
    union
    {
        u32 muData;
        struct
        {
            u16 mu16FirstEntity;
            u16 mu16NumEntities;
        };
    };

    u16 GetFirstEntity() const { return mu16FirstEntity; }
    u16 GetNumEntities() const { return mu16NumEntities; }
};

// The serialised StaticSoundMap DATA type. Shape recovered from the DecFIGS DWARF
// (struct BrnSound::World::StaticSoundMap, BrnStaticSoundMap.h:365-380). Minimal
// data home: only the fields, giving the resource-type FixUp a real, named layout
// to relocate. The data type's many methods (Construct/GetEntity/...) are deferred
// to the StaticSoundMap data TU.
//
// SERIALISED (platform-4) FORM: this host layout (0x50 header; offsets pinned by
// the static_asserts in BrnStaticSoundMapResourceType.cpp) is the on-disk x64
// form; the porter (tools/assets/bundles/world_type_transcode.py
// transcode_staticsoundmap) rebuilds the 0x40-byte X360 header to it. The
// 16-byte entities {f32 x,y,z, u16, u16} and the 4-byte grid cells
// {u16 firstEntity (0xFFFF = empty), u16 count} are pointer-free and keep
// their console strides; mpSubRegions/mpEntities serialise as u64 offsets.
struct StaticSoundMap
{
    // BrnStaticSoundMap.h:380 — root-classification enum (enumerator names/values
    // are DWARF-attested). meRootType is not touched by FixUp.
    enum eRootType
    {
        E_ROOT_TYPE_PASSBY  = 0,
        E_ROOT_TYPE_EMITTER = 1,
        E_ROOT_TYPE_COUNT   = 2
    };

    // GetSubRegionDescrip @ 0x8267AF10 — XZ-plane grid lookup. Returns the
    // descriptor for the cell containing lrPosition, or nullptr when the point is
    // outside the [mMin, mMax) extents (compared per the X360 `>= max` /
    // `< min` half-open tests) or maps to an out-of-range cell. The map grid lives
    // in the world XZ plane: the X360 reads the query's X (lane 0) and Z (lane 2)
    // and compares them against mMin/mMax, whose .x/.y lanes hold the X/Z bounds.
    const SubRegionDescriptor* GetSubRegionDescrip(const Vector3& lrPosition) const;

    // GetEntity @ 0x82675578 — returns the entity at liEntityIndex. The X360 asm
    // asserts mpEntities != 0 and liEntityIndex < miNumEntities, then returns
    // &mpEntities[liEntityIndex] (16-byte stride == sizeof(StaticSoundEntity),
    // now a complete type — the size is static_asserted at its definition above).
    const StaticSoundEntity& GetEntity(s32 liEntityIndex) const;

    // IsInRange @ 0x82677570 — half-open XZ overlap test between a radius-inflated
    // query point and a packed bounds rect. lrPackedMinAndMax holds {minX, minZ,
    // maxX, maxZ} in lanes x/y/z/w. The X360 body does not dereference `this`
    // (operates purely on its arguments); kept a member to match the DWARF decl.
    bool IsInRange(const Vector3& lrPosition, f32 lfRadius,
                   const Vector4& lrPackedMinAndMax) const;

    const Vector2& GetMin() const { return mMin; }
    const Vector2& GetMax() const { return mMax; }
    f32 GetSubRegionSize() const { return mfSubRegionSize; }
    s32 GetNumEntities() const { return miNumEntities; }
    eRootType GetRootType() const { return meRootType; }

    // The resource-type handler relocates mpSubRegions/mpEntities and validates
    // mpEntities/miNumEntities during FixUp. Grant it friendship rather than
    // exposing the private data. FLAG: friend-access choice (vs public fields).
    friend struct StaticSoundMapResourceType;

private:
    // DWARF field order (BrnStaticSoundMap.h:365-380). FLAG: Vector2 resolved to
    // the committed rw::math::vpu::Vector2 via BrnCommonTypes.h.
    Vector2                     mMin;             // :365
    Vector2                     mMax;             // :366
    f32                         mfSubRegionSize;  // :368
    const SubRegionDescriptor*  mpSubRegions;     // :373
    s32                         miNumSubRegionsX; // :374
    s32                         miNumSubRegionsZ; // :375
    const StaticSoundEntity*    mpEntities;       // :377
    s32                         miNumEntities;    // :378
    eRootType                   meRootType;       // :380
};

}
}

#endif
