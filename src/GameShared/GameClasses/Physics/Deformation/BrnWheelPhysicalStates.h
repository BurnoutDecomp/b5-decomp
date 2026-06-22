#ifndef BRN_PHYSICS_DEFORMATION_BRNWHEELPHYSICALSTATES_H
#define BRN_PHYSICS_DEFORMATION_BRNWHEELPHYSICALSTATES_H

#include "types.hpp"

// =============================================================================
// BrnWheelPhysicalStates.h  (OWNING HEADER for
//   BrnPhysics::Deformation::WheelPhysicalStates)
//
// Home for the deformation wheel physical-state block. The only attested function
// is the compiler-generated copy-assignment operator
//   BrnPhysics::Deformation::WheelPhysicalStates::operator=  @ 0x825C0A00
// (called by DeformableObject::OutputWheelData and a sibling). The X360 lowers it
// as a straight 0x188-byte (392-byte) field copy: 24 quadwords loaded/stored via
// lvx128/stvx128 over offsets 0x000..0x170 (384 bytes), followed by 8 single-byte
// lbz/stb copies over 0x180..0x187.
//
// LAYOUT (asm-authoritative, from the copy spine):
//   The block is exactly 0x188 == 392 bytes. The first 0x180 bytes are a
//   16-byte-aligned region copied as 24 quadwords (the per-wheel SIMD physical
//   state -- positions/velocities/forces); the trailing 8 bytes at 0x180 are
//   copied byte-wise (a smaller scalar tail, e.g. flags/counters). No finer
//   member breakdown is attested by this TU, so the region is modelled as the
//   two faithful sub-spans the asm copies and the copy is a store-order-faithful
//   memcpy of the whole 392 bytes. sizeof == 0x188.
//
// FLAG: MINIMAL FLAGGED HOME. Only the byte extent and the copy-assignment are
// attested here; the per-wheel field breakdown is deferred to whichever TU
// populates it (OutputWheelData et al.). Grow additively when those land.
// =============================================================================

namespace BrnPhysics
{
namespace Deformation
{

// Wheel physical-state block. 0x188 (392) bytes. See header note for the layout
// the copy-assignment spine attests.
struct WheelPhysicalStates
{
    // +0x000 .. +0x17F : the 16-byte-aligned SIMD region copied as 24 quadwords.
    u8 maQuadwordRegion[0x180];   // 384 bytes (24 * 16)

    // +0x180 .. +0x187 : the 8-byte scalar tail copied byte-wise.
    u8 maTail[8];                 // 8 bytes

    // WheelPhysicalStates::operator=  @ 0x825C0A00. Field copy of the whole
    // 0x188-byte block; returns *this (the asm returns r3 == result == this).
    WheelPhysicalStates& operator=(const WheelPhysicalStates& lkrSource);
};

} // namespace Deformation
} // namespace BrnPhysics

#endif // BRN_PHYSICS_DEFORMATION_BRNWHEELPHYSICALSTATES_H
