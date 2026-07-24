#ifndef CGS_GRAPHICS_INSTANCES_CGS_INSTANCE_H
#define CGS_GRAPHICS_INSTANCES_CGS_INSTANCE_H

// =============================================================================
// CgsGraphics::Instance / InstanceList
//   GameShared/GameClasses/Graphics/Instances/CgsInstance.h
//
// PROMOTED 2026-07-24 from the CgsInstance.cpp-local declarations (the
// WorldEntityModule TU needs the element layout by name). Reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// Instance is the 80-byte (X360 on-disk) world-instance element. Field map
// attested by the WorldEntityModule consumers (RenderInstance @cpp:134 inline,
// LoadBackdropForZone @0x822EDBC8, UpdateBackdropSceneEntities @0x822D8730,
// GenerateDispatchLists @0x822D5AB0):
//   +0x00 model handle -> fixed up to the Model* at load (FixDown clears it)
//   +0x04 muBackdropZoneNumber (0xFFFF = none) -- the zone this backdrop
//         stand-in represents
//   +0x08 muZoneNumber
//   +0x0C mfMaxDrawDistanceSq (compared against the scaled camera distance^2)
//   +0x10 mTransform (Matrix44Affine; world position = .Pos())
//
// The instance lists stream inside the TRK_UNIT%d_GR bundles; instances
// [0, muNumInstances) are the complete/real entries, and the tail
// [muNumInstances, muArraySize) holds the backdrop stand-in entries the world
// module swaps in for unloaded neighbour zones.
// =============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // Matrix44Affine

#include <cstdint>

namespace CgsGraphics
{
    struct Model;

    struct Instance
    {
        Model*         mpModel;               // +0x00 (fixed-up model pointer)
        u32            muBackdropZoneNumber;  // +0x04 (0xFFFF = not a backdrop link)
        u32            muZoneNumber;          // +0x08
        f32            mfMaxDrawDistanceSq;   // +0x0C
        Matrix44Affine mTransform;            // +0x10 (position = .Pos())
    };

    struct InstanceList
    {
        uintptr_t mpaInstances;    // 0x00 Instance* (base of the instance buffer)
        u32       muArraySize;     // 0x04 total Instance entries
        u32       muNumInstances;  // 0x08 complete Instance entries
        u32       muVersionNumber; // 0x0C

        InstanceList* FixDown(int delta);
        Instance*     GetInstance(u32 luIndex) const;
    };
}

#endif // CGS_GRAPHICS_INSTANCES_CGS_INSTANCE_H
