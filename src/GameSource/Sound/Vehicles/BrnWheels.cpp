#include "GameSource/Sound/Vehicles/BrnWheels.h"

// =============================================================================
// BrnSound::Vehicles::Wheels -- out-of-line body for the single ledger function
// owned by this TU:
//   InAirEffec  @ 0x8268DC00
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// See BrnWheels.h for the layout map and the un-DWARF'd-member FLAG. All members
// reached BY NAME.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels
{

// ---------------------------------------------------------------------------
// InAirEffec  @ 0x8268DC00
//
//   return mpfnInAirEffect( miParamB + miParamA );
//
// Store-for-store with the X360: load the callback @ +0x08, the two operands @
// +0x0C and +0x10, sum them (add r3, r11(=+0x10), r10(=+0x0C)) and tail-call the
// callback with the sum (mtctr ; bctr). The result of the callback is this
// function's result (the tail call forwards r3). The whole prologue-less body is
// these three loads, one add, and the indirect tail call.
// ---------------------------------------------------------------------------
s32 InAirEffec( WheelsState* lpState )
{
    return lpState->mpfnInAirEffect( lpState->miParamB + lpState->miParamA );
}

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound
