#pragma once

#include <cstddef>   // offsetof (the _AssertLayout pins)

#include "types.hpp"
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"   // PropTypeData, PropPartTypeData, KU_MAX_PROP_TYPES, KU_MAX_PROP_PART_TYPES
#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CgsDev::Assert (GetType bounds asserts)
#include "rw/rwcore_structs.h"                                      // rw::Resource (FixUp/FixDown declarations)

// BrnPhysics::Props::PropPhysicsDataHeader — the prop-physics resource header, and the whole
// content of PROPS/PROPPHYSICS.BUNDLE (resource 0xD75C5932, type 0x1000F). Four leading
// uint32_t counts/size, three FIXED-SIZE pointer arrays (prop types / part types / collision
// volumes), then a build timestamp; the records those arrays point at live in an arena
// immediately after the header, inside the same resource.
//
// Member set and array bounds are DecFIGS DWARF ground truth
// (references/DecFIGS/dwarfdump/.../BrnPropPhysicsDataHeader.h: `PropTypeData *[500]`,
// `PropPartTypeData *[300]`, `VolRef::Volume *[2048]`, trailing `uint32_t muTimeStamp`), and
// every one of those bounds is independently pinned by the array base offsets in the X360
// FixUp @0x8267F570: +0x10, +0x7E0, +0xC90 -- i.e. 500 and 300 four-byte slots between them.
// (An earlier revision sized mapPropPartTypes at 500 from a placeholder constant, which put
// mapVolumeTypes 800 bytes past where the data has it.)
//
// LOADED IN PLACE: the native resource blob is this object. FixUp only adds
// the resource load base to each serialised offset slot. _AssertLayout pins
// the ARTIST table offsets and native 0x2C94-byte header.
//
// GetType @ 0x82277C50 is emitted OUT OF LINE by the shipped X360 build and its body overrides
// the leak's inline version: TWO bounds asserts -- liTypeId < KU_MAX_PROP_TYPES (0x1F4 == 500)
// and liTypeId < muNumberOfPropTypes -- then `return mapPropTypes[liTypeId];` unconditionally
// (no null-on-overflow guard). Construct/Prepare/Release/Destruct/GetSizeInBytes/Refix are
// declared per the DWARF and bodied by their own recon passes.

namespace BrnPhysics
{
namespace Props
{

static const uint32_t KU_MAX_PROP_PHYSICS_VOLUMES = 2 * 1024;

class PropPhysicsDataHeader
{
public:
    void Construct();
    bool Prepare();
    bool Release();
    void Destruct();

    inline uint32_t GetSizeInBytes() const;

    void FixUp(const rw::Resource& lBaseResource);
    void FixDown(const rw::Resource& lBaseResource);
    void Refix(const void* lpSrc, void* lpDest);

    // 0x82277C50 - out-of-line. Asserts liTypeId is in [0, KU_MAX_PROP_TYPES) and
    // [0, muNumberOfPropTypes), then returns the type pointer at that slot.
    const PropTypeData* GetType(uint32_t liTypeId) const;

    inline uint32_t GetNumberOfPropTypes() const;

    inline const PropPartTypeData* GetPartType(uint32_t liPartTypeId) const;

    // DWARF BrnPropPhysicsDataHeader.h:102. The data compiler's build stamp; the shipped
    // resource carries 0x47AAE3EA == 2008-02-07, the ARTIST build window.
    inline uint32_t GetTimeStamp() const;

    // Compile-time-only layout pin (see the block at the bottom of this header).
    static void _AssertLayout();

private:
    uint32_t muNumberOfPropTypes;        // +0x00
    uint32_t muNumberOfVolumeTypes;      // +0x04
    uint32_t muNumberOfPartTypes;        // +0x08

    uint32_t muSizeInBytes;              // +0x0C

    PropTypeData* mapPropTypes[KU_MAX_PROP_TYPES];                    // +0x10

    PropPartTypeData* mapPropPartTypes[KU_MAX_PROP_PART_TYPES];       // native +0x7E0

    rw::collision::Volume* mapVolumeTypes[KU_MAX_PROP_PHYSICS_VOLUMES];  // native +0xC90

    uint32_t muTimeStamp;                // native +0x2C90
};

// 0x82277C50 - reconstructed store-for-store from the X360 asm (OVERRIDES the leak inline).
inline const PropTypeData*
PropPhysicsDataHeader::GetType(uint32_t liTypeId) const
{
    CGS_ASSERT(liTypeId < KU_MAX_PROP_TYPES, "liTypeId < KU_MAX_PROP_TYPES");
    CGS_ASSERT(liTypeId < muNumberOfPropTypes, "liTypeId < muNumberOfPropTypes");
    return mapPropTypes[liTypeId];
}

inline uint32_t
PropPhysicsDataHeader::GetNumberOfPropTypes() const
{
    return muNumberOfPropTypes;
}

// Declared per the leak layout; left to its own recon pass to body. GetPartType's leak inline used
// CgsDev::Assert::PrintStringed directly -- replaced by the project CGS_ASSERT front-end.
inline const PropPartTypeData*
PropPhysicsDataHeader::GetPartType(uint32_t liPartTypeId) const
{
    CGS_ASSERT(liPartTypeId < muNumberOfPartTypes, "liPartTypeId < muNumberOfPartTypes");
    return mapPropPartTypes[liPartTypeId];
}

inline uint32_t
PropPhysicsDataHeader::GetTimeStamp() const
{
    return muTimeStamp;
}

// DWARF BrnPropPhysicsDataHeader.h:75. The serialised total (header + record arena), which
// is what PropPhysicsResourceType::GetSerialisedResourceDescriptor reports as entry0's size.
//
// DATA NOTE: on the X360 disk this word is written LITTLE-endian while the three counts
// beside it are big-endian (an ARTIST bake-tool anomaly, same class as the PropGraphicsList
// one) -- so the console would read 0xE0360100 for a 0x136E0-byte resource and this accessor
// is effectively dead there. The converter recomputes it correctly for the widened blob, so
// on PC it really is the resource size.
inline uint32_t
PropPhysicsDataHeader::GetSizeInBytes() const
{
    return muSizeInBytes;
}

// Native table bases are used directly by ARTIST FixUp/FixDown @0x8267F570/658.
inline void PropPhysicsDataHeader::_AssertLayout()
{
    PropTypeData::_AssertLayout();
    PropPartTypeData::_AssertLayout();
    CGS_STATIC_ASSERT((KU_MAX_PROP_TYPES == 500), "mapPropTypes[500]");
    CGS_STATIC_ASSERT((KU_MAX_PROP_PART_TYPES == 300), "mapPropPartTypes[300]");
    CGS_STATIC_ASSERT((KU_MAX_PROP_PHYSICS_VOLUMES == 2048), "mapVolumeTypes[2048]");
    CGS_STATIC_ASSERT((offsetof(PropPhysicsDataHeader, muNumberOfPropTypes) == 0x0), "PropPhysicsDataHeader::muNumberOfPropTypes @0x0");
    CGS_STATIC_ASSERT((offsetof(PropPhysicsDataHeader, muNumberOfVolumeTypes) == 0x4), "PropPhysicsDataHeader::muNumberOfVolumeTypes @0x4");
    CGS_STATIC_ASSERT((offsetof(PropPhysicsDataHeader, muNumberOfPartTypes) == 0x8), "PropPhysicsDataHeader::muNumberOfPartTypes @0x8");
    CGS_STATIC_ASSERT((offsetof(PropPhysicsDataHeader, muSizeInBytes) == 0xC), "PropPhysicsDataHeader::muSizeInBytes @0xC");
    CGS_STATIC_ASSERT((offsetof(PropPhysicsDataHeader, mapPropTypes) == 0x10), "PropPhysicsDataHeader::mapPropTypes @0x10");
    CGS_STATIC_ASSERT((offsetof(PropPhysicsDataHeader, mapPropPartTypes) == 0x7E0), "PropPhysicsDataHeader::mapPropPartTypes @0x7E0");
    CGS_STATIC_ASSERT((offsetof(PropPhysicsDataHeader, mapVolumeTypes) == 0xC90), "PropPhysicsDataHeader::mapVolumeTypes @0xC90");
    CGS_STATIC_ASSERT((offsetof(PropPhysicsDataHeader, muTimeStamp) == 0x2C90), "PropPhysicsDataHeader::muTimeStamp @0x2C90");
    CGS_STATIC_ASSERT((sizeof(PropPhysicsDataHeader) == 0x2C94), "native prop physics header size");
    CGS_STATIC_ASSERT((__alignof(PropPhysicsDataHeader) == 4), "native prop physics header alignment");
}

}
}
