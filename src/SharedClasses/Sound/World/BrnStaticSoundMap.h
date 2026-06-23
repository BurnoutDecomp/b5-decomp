#ifndef BRN_STATIC_SOUND_MAP_H
#define BRN_STATIC_SOUND_MAP_H

#include "types.hpp"          // s32, u32, f32
#include "BrnCommonTypes.h"   // Vector2 (= rw::math::vpu::Vector2, 16-byte SIMD register)

namespace BrnSound
{
namespace World
{
// StaticSoundEntity stays pointer-only here; its full layout (and its own FixUp)
// is deferred to its own TU. Forward-declaration is the correct choice — the map
// holds it only via a const pointer.
// FLAG: StaticSoundEntity element type is DEFERRED.
struct StaticSoundEntity;

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
    u32 muData;   // opaque 4-byte cell (stride proven by 0x8267AF10 `slwi …,2`)
};

// The serialised StaticSoundMap DATA type. Shape recovered from the DecFIGS DWARF
// (struct BrnSound::World::StaticSoundMap, BrnStaticSoundMap.h:365-380). Minimal
// data home: only the fields, giving the resource-type FixUp a real, named layout
// to relocate. The data type's many methods (Construct/GetEntity/...) are deferred
// to the StaticSoundMap data TU.
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
