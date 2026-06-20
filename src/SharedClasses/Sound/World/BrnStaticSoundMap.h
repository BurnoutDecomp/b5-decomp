#ifndef BRN_STATIC_SOUND_MAP_H
#define BRN_STATIC_SOUND_MAP_H

#include "types.hpp"          // s32, u32, f32
#include "BrnCommonTypes.h"   // Vector2 (= rw::math::vpu::Vector2, 16-byte SIMD register)

namespace BrnSound
{
namespace World
{
// The two element types are pointer-only here; their full layouts (and their own
// FixUp methods) are deferred to their own TUs. Forward-declaration is the correct
// choice — StaticSoundMap holds them only via const pointers.
// FLAG: SubRegionDescriptor / StaticSoundEntity element types are DEFERRED.
struct SubRegionDescriptor;
struct StaticSoundEntity;

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
