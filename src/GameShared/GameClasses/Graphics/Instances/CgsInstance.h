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
//
// *** ON-DISC LAYOUT (PVS wave 2026-07-27) ***
// These two structs are NOT host objects -- they ARE the streamed resource body
// (CgsGraphics::InstanceListResourceType relocates them in place, and the world
// data porter emits them as a byte-exact endian flip of the console blob). So
// every pointer field is a 32-BIT SLOT, resolved through the project's low-4 GB
// PointerFromU32 convention (the engine's whole root allocation is reserved
// below 4 GB -- CgsMemory::LowMemory). Declaring them with host-width pointers
// made sizeof(Instance) 96 instead of the console's 80 and put muArraySize at
// byte +8 instead of +4, so the InstanceList header read back as
// mpaInstances == (arraySize << 32 | instanceOffset) -- an immediate access
// violation in InstanceListResourceType::PostFixUp on the first TRK_UNIT load.
// Ptr32<T> keeps every `instance->mpModel->...` call site source-compatible
// while pinning the slot to 4 bytes; the static_asserts below are the
// regression tripwires.
// =============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // Matrix44Affine
#include "GameShared/GameClasses/Graphics/CgsSerialisedPtr.h"   // Ptr32<T> (the 32-bit slot)

#include <cstddef>
#include <cstdint>

namespace CgsGraphics
{
    struct Model;

    struct Instance
    {
        Ptr32<Model>   mpModel;               // +0x00 (fixed-up model pointer, 4-byte slot)
        u32            muBackdropZoneNumber;  // +0x04 (0xFFFF = not a backdrop link)
        u32            muZoneNumber;          // +0x08
        f32            mfMaxDrawDistanceSq;   // +0x0C
        Matrix44Affine mTransform;            // +0x10 (position = .Pos())
    };

    struct InstanceList
    {
        u32       muaInstances;    // 0x00 Instance* (base of the instance buffer, 4-byte slot)
        u32       muArraySize;     // 0x04 total Instance entries
        u32       muNumInstances;  // 0x08 complete Instance entries
        u32       muVersionNumber; // 0x0C

        // X360 0x827F9318 / 0x827F9478 -- the load-base relocation pair for the instance
        // buffer slot (FixUp also tripwires muVersionNumber == 1).
        InstanceList* FixUp(int delta);
        InstanceList* FixDown(int delta);
        Instance*     GetInstance(u32 luIndex) const;
    };

    // The console stride/offsets this whole subsystem is walked with.
    static_assert(sizeof(Instance) == 80, "Instance must be the console's 80-byte on-disc element");
    static_assert(sizeof(InstanceList) == 16, "InstanceList must be the console's 16-byte on-disc header");
    static_assert(offsetof(Instance, mTransform) == 0x10, "Instance::mTransform must be at +0x10");
}

#endif // CGS_GRAPHICS_INSTANCES_CGS_INSTANCE_H
