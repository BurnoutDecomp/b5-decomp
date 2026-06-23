#ifndef BRN_GLOBAL_IRRADIANCE_MANAGER_H
#define BRN_GLOBAL_IRRADIANCE_MANAGER_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Matrix44 (== rw::math::vpu::Matrix44, 64 bytes)

// ============================================================================
// GameSource/World/EnvironmentManager/BrnGlobalIrradianceManager.h
//
// BrnWorld::GlobalIrradianceManager holds, per colour channel, a 4x4 irradiance
// matrix used to build the lighting shader constants. GetIrradianceMatrix copies
// one channel's matrix out for the renderer.
//
// LAYOUT (from GetIrradianceMatrix @ 0x827B0500): the matrices live as a packed
// array of three 64-byte (16-aligned) Matrix44s at offset 0 of `this`; the getter
// indexes it by colour channel (channel << 6 == channel * sizeof(Matrix44)).
//
// FLAG: only the leading three-matrix array (the storage GetIrradianceMatrix
// reads) is modelled. Any further members of the manager are deferred -- this
// type is grown additively if a later TU attests more fields.
// ============================================================================

namespace BrnWorld
{
class GlobalIrradianceManager
{
public:
    // The three colour channels (R, G, B) the irradiance matrices are indexed by.
    enum { E_NUM_COLOURS = 3 };

    // GetIrradianceMatrix @ 0x827B0500. Copies the irradiance matrix for colour
    // channel lu8Colour (0..2) into lrOutMatrix and returns it. The X360 sret form
    // (r3 = out, r4 = this, r5 = colour) returns the out pointer; modelled here as
    // a by-value getter that fills the caller-provided out matrix and returns a
    // reference to it (semantic parity).
    //
    // X360-faithful: asserts lu8Colour < 3 (baked BrnGlobalIrradianceManager.h:100,
    // expr "lu8Colour < 3"; the baked file/line is not reproduced -- CGS_ASSERT
    // injects __FILE__/__LINE__), then copies the 64-byte Matrix44 at
    // maIrradianceMatrices[lu8Colour] via the four lvx128/stvx128 (vector) loads
    // and stores -- a plain 4x16-byte copy, NOT a VMX compute pipeline.
    Matrix44& GetIrradianceMatrix(Matrix44& lrOutMatrix, u8 lu8Colour) const;

private:
    // +0x00: three 64-byte irradiance matrices, one per colour channel.
    Matrix44 maIrradianceMatrices[E_NUM_COLOURS];
};
}

#endif // BRN_GLOBAL_IRRADIANCE_MANAGER_H
