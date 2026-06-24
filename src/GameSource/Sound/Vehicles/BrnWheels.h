#ifndef BRN_SOUND_VEHICLES_WHEELS_H
#define BRN_SOUND_VEHICLES_WHEELS_H

#include "types.hpp"

// =============================================================================
// BrnSound::Vehicles::Wheels
//   GameSource/Sound/Vehicles/BrnWheels.h +
//   GameSource/Sound/Vehicles/BrnWheels.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The Wheels sound object drives the per-wheel road/air noise. InAirEffec is a
// thin forwarder: it pushes the sum of two stored wheel parameters through a
// stored callback (the active "in-air" effect handler).
//
// This TU bodies exactly ONE ledger function:
//   InAirEffec  @ 0x8268DC00
//
// LAYOUT (recovered store-for-store from the asm; all members reached BY NAME):
//   lwz r9, 8(r3)    -> mpfnInAirEffect  (callable member @ +0x08, NOT a vtable:
//                        the value at +8 is loaded straight into ctr -- one
//                        indirection -- so it is a function-pointer member)
//   lwz r10, 0xC(r3) -> miParamA         (s32 @ +0x0C)
//   lwz r11, 0x10(r3)-> miParamB         (s32 @ +0x10)
//   add r3, r11, r10 -> arg = miParamB + miParamA
//   mtctr r9 ; bctr  -> tail-call mpfnInAirEffect(arg)
//
// FLAG (un-DWARF'd member names/types): no DecFIGS DWARF hint exists, so the three
// members are modelled as HONEST named fields at the asm-observed offsets/widths.
// The callback's exact signature is taken from the asm: ONE s32-class arg (r3) in,
// an s32-class result (r3) out -- `s32 (*)(s32)`. The two operand members are read
// with lwz (32-bit) and summed as integers; their precise semantics (which wheel
// parameter) are UNVERIFIED. Gaps (0x00..0x07) are other Wheels state not touched
// by this function.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels
{

// The stored "in-air effect" callback signature, taken from the InAirEffec asm:
// one s32 arg in r3, s32 result in r3.
typedef s32 (*InAirEffectFn)( s32 liParam );

// BrnWheels.h. Per-wheel sound object operated on by the Wheels::* functions.
struct WheelsState
{
    u8            maReserved0x00[0x08]; // @0x00..0x07: state not touched by InAirEffec

    InAirEffectFn mpfnInAirEffect;      // @0x08: callable forwarded to by InAirEffec
    s32           miParamA;             // @0x0C: first summed wheel parameter
    s32           miParamB;             // @0x10: second summed wheel parameter
};

// BrnWheels.h. Forward (miParamB + miParamA) through the stored in-air effect
// callback and return its result. @ 0x8268DC00.
s32 InAirEffec( WheelsState* lpState );

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_WHEELS_H
