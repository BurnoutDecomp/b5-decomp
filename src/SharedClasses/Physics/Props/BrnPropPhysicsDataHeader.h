#pragma once

#include "types.hpp"
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"   // PropTypeData, PropPartTypeData, KU_MAX_PROP_TYPES, KU_MAX_PROP_PART_TYPES
#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CgsDev::Assert (GetType bounds asserts)
#include "rw/rwcore_structs.h"                                      // rw::Resource (FixUp/FixDown declarations)

// BrnPhysics::Props::PropPhysicsDataHeader — the prop-physics resource header. Layout is GROUND
// TRUTH from the Feb-2007 leak (SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h): four leading
// uint32_t counts/size, then three pointer arrays (prop types / part types / collision volumes).
//
// This recon pass owns ONE function: GetType @ 0x82277C50. The shipped X360 build emits GetType
// OUT OF LINE, and its body OVERRIDES the leak's inline version:
//   * It performs TWO bounds asserts -- liTypeId < KU_MAX_PROP_TYPES (0x1F4 == 500) and
//     liTypeId < muNumberOfPropTypes -- (the leak inline only checked muNumberOfPropTypes and
//     returned 0 on out-of-range).
//   * It then returns mapPropTypes[liTypeId] unconditionally (asm: result = a1[liTypeId + 4],
//     i.e. the (liTypeId)th pointer past the four leading words). No null-on-overflow guard.
// The remaining member functions (Construct/Prepare/Release/Destruct/GetSizeInBytes/FixUp/FixDown/
// Refix/GetPartType/GetNumberOfPropTypes) are declared per the leak layout but bodied by their own
// recon passes; only GetType is defined here.

namespace rw { namespace collision { class Volume; } }

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

    uint32_t GetSizeInBytes() const;

    void FixUp(const rw::Resource& lBaseResource);
    void FixDown(const rw::Resource& lBaseResource);
    void Refix(const void* lpSrc, void* lpDest);

    // 0x82277C50 - out-of-line. Asserts liTypeId is in [0, KU_MAX_PROP_TYPES) and
    // [0, muNumberOfPropTypes), then returns the type pointer at that slot.
    const PropTypeData* GetType(uint32_t liTypeId) const;

    inline uint32_t GetNumberOfPropTypes() const;

    inline const PropPartTypeData* GetPartType(uint32_t liPartTypeId) const;

private:
    uint32_t muNumberOfPropTypes;
    uint32_t muNumberOfVolumeTypes;
    uint32_t muNumberOfPartTypes;

    uint32_t muSizeInBytes;

    PropTypeData* mapPropTypes[KU_MAX_PROP_TYPES];

    PropPartTypeData* mapPropPartTypes[KU_MAX_PROP_PART_TYPES];

    rw::collision::Volume* mapVolumeTypes[KU_MAX_PROP_PHYSICS_VOLUMES];
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

}
}
