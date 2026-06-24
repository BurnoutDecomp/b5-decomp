#include "GameSource/Sound/Vehicles/BrnVehicleStateManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (index-range tripwire)

// =============================================================================
// BrnSound::Vehicles::VehicleStateManager -- out-of-line body for the single
// ledger function owned by this TU:
//   GetAIEngineAssignment  @ 0x82682050
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace VehicleStateManager
{

// The AI engine-voice assignment table the X360 indexes as byte_82FFB838[index]
// (lbzx r3, r11, r31). It lives in the writable data segment (0x82FFxxxx), i.e. it
// is RUNTIME-POPULATED per-car state that other VehicleStateManager paths fill --
// NOT a static const lookup. Its contents are therefore NOT a fact recoverable
// from this getter, and are modelled as zero-initialised storage (the honest
// default state the read observes before any assignment write). One byte per
// active race-car slot.
static u8 gaAIEngineAssignment[E_ACTIVE_RACE_CAR_INDEX_COUNT] = { 0 };

// ---------------------------------------------------------------------------
// GetAIEngineAssignment  @ 0x82682050
//
//   if ( !(liVehicleIndex >= 0 && liVehicleIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT) )
//       assert("liVehicleIndex >= 0 && liVehicleIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT",
//              BrnVehicleStateManager.h:143)               ; non-gating tripwire
//   return gaAIEngineAssignment[liVehicleIndex]            ; lbzx r3, byte_82FFB838[idx]
//
// Store-for-store with the X360: the bounds check is the signed pair
// (cmpwi r31,0 ; blt) and (cmpwi r31,8 ; blt ok), i.e. 0 <= index < 8; the read is
// a single zero-extended byte load (lbzx). The Hex-Rays `unsigned int a1` is the
// register width; the asm compares it SIGNED for the >=0 half of the assert, so
// the index is conceptually a signed in-range value -- the assert text spells out
// both halves.
// ---------------------------------------------------------------------------
u8 GetAIEngineAssignment( u32 luVehicleIndex )
{
    CGS_ASSERT( static_cast<s32>(luVehicleIndex) >= 0
                && luVehicleIndex < static_cast<u32>(E_ACTIVE_RACE_CAR_INDEX_COUNT),
                "liVehicleIndex >= 0 && liVehicleIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT" );

    return gaAIEngineAssignment[luVehicleIndex];
}

} // namespace VehicleStateManager
} // namespace Vehicles
} // namespace BrnSound
