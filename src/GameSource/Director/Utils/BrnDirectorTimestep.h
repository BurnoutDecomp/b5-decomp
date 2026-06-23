#ifndef GAMESOURCE_DIRECTOR_UTILS_BRN_DIRECTOR_TIMESTEP_H
#define GAMESOURCE_DIRECTOR_UTILS_BRN_DIRECTOR_TIMESTEP_H

#include "types.hpp"
#include "rw/math/vpu/types.h"                          // rw::math::vpu (the SIMD register type vocabulary)
#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT (the Get bounds assert)

// ============================================================================
// GameSource/Director/Utils/BrnDirectorTimestep.h
//
// BrnDirector::Timestep -- a tiny value object the director camera behaviours sample once
// per frame to get the frame delta in one of three flavours: the game timestep (slow-mo
// scaled), the world timestep (slow-mo scaled, the physics-facing one), and the world
// timestep WITHOUT the slow-mo scale. Each flavour is held twice: once as a scalar f32
// (Get) and once as a broadcast VecFloat register (GetVecFloat) so vector math can multiply
// by it without a scalar->register move. HOME for the Timestep class.
//
// ----------------------------------------------------------------------------
// LAYOUT (pinned from BrnDirector::Timestep::Get @0x821F2908). Get loads
//   *(this + (leType + 12) * 4) == *(this + leType*4 + 48), i.e. the scalar array
//   mafTimestep[] starts at byte +0x30, just past the three 16-byte VecFloat registers
//   maVecFloatTimestep[3] (3 * 16 == 48 == 0x30). VecFloat is one SIMD register (16 bytes),
//   modelled here as the broadcast lane register so the offsets match the console.
//
//   +0x00  VecFloat maVecFloatTimestep[3]   (3 * 16 == 48 bytes)
//   +0x30  f32      mafTimestep[3]
// ----------------------------------------------------------------------------

namespace BrnDirector
{

// FLAG: minimal slice of rw::math::VecFloat -- the SDK's broadcast scalar register (one
//   16-byte SIMD lane register holding the same scalar in all four lanes). The reconstructed
//   rw math home (rw/math/vpu) de-models VecFloat to a plain f32 for arithmetic, but Timestep
//   STORES a VecFloat register by value (its layout pins mafTimestep at +0x30), so the storage
//   type must be 16 bytes wide. Modelled here as a 16-byte, 16-aligned broadcast register
//   constructed from / convertible to a scalar f32. Replace with the real VecFloat when the
//   rw math VecFloat type TU lands; the storage width and the scalar conversions are stable.
struct alignas(16) VecFloat
{
    f32 maLanes[4];

    VecFloat() : maLanes{ 0.0f, 0.0f, 0.0f, 0.0f } {}
    VecFloat(f32 lfScalar) : maLanes{ lfScalar, lfScalar, lfScalar, lfScalar } {}

    // The scalar value the register broadcasts (lane 0).
    operator f32() const { return maLanes[0]; }
};

class Timestep
{
public:

    enum EType
    {
        E_TIMESTEP_INVALID = -1,

        E_WORLD,
        E_WORLD_NO_SLOMO,
        E_GAME,

        E_TIMESTEP_COUNT
    };

    // The scalar frame delta for the requested flavour. @0x821F2908.
    f32 Get(EType leType) const;

    // The same frame delta as a broadcast VecFloat register.
    VecFloat GetVecFloat(EType leType) const;

    // Seed all three flavours (both the scalar and the register copies). The world / no-slomo
    // / game order matches the body's stores.
    void Set(
        const VecFloat& lGameTimestep,
        const VecFloat& lWorldTimestep,
        const VecFloat& lWorldNoSlomoTimestep,
        f32 lfGameTimestep,
        f32 lfWorldTimestep,
        f32 lfWorldNoSlomoTimestep);

private:

    VecFloat maVecFloatTimestep[E_TIMESTEP_COUNT];   // +0x00
    f32      mafTimestep[E_TIMESTEP_COUNT];          // +0x30
};



// ----------------------------------------------------------------------------
// BrnDirector::Timestep::Get @0x821F2908
//   Bounds-asserts leType is in (E_TIMESTEP_INVALID, E_TIMESTEP_COUNT) then returns the
//   scalar flavour. The asm compares leType > -1 and leType < 3 (the > E_GAME early-out in
//   the pseudocode is the same compare). The index load is mafTimestep[leType].
// ----------------------------------------------------------------------------
inline f32
Timestep::Get(EType leType) const
{
    CGS_ASSERT(leType > E_TIMESTEP_INVALID && leType < E_TIMESTEP_COUNT,
               "leType > E_TIMESTEP_INVALID && leType < E_TIMESTEP_COUNT");
    return mafTimestep[leType];
}



inline VecFloat
Timestep::GetVecFloat(EType leType) const
{
    CGS_ASSERT(leType > E_TIMESTEP_INVALID && leType < E_TIMESTEP_COUNT,
               "leType > E_TIMESTEP_INVALID && leType < E_TIMESTEP_COUNT");
    return maVecFloatTimestep[leType];
}



inline void
Timestep::Set(
    const VecFloat& lGameTimestep,
    const VecFloat& lWorldTimestep,
    const VecFloat& lWorldNoSlomoTimestep,
    f32 lfGameTimestep,
    f32 lfWorldTimestep,
    f32 lfWorldNoSlomoTimestep)
{
    mafTimestep[E_WORLD] = lfWorldTimestep;
    mafTimestep[E_WORLD_NO_SLOMO] = lfWorldNoSlomoTimestep;
    mafTimestep[E_GAME] = lfGameTimestep;
    maVecFloatTimestep[E_WORLD] = lWorldTimestep;
    maVecFloatTimestep[E_WORLD_NO_SLOMO] = lWorldNoSlomoTimestep;
    maVecFloatTimestep[E_GAME] = lGameTimestep;
}

} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_UTILS_BRN_DIRECTOR_TIMESTEP_H
