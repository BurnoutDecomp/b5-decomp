#include "GameSource/Sound/Global/BrnMusicEffect.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (Prepare lpLogicModule tripwire)

// =============================================================================
// BrnSound::Logic::MusicEffect::EaTraxData -- out-of-line body for the single
// ledger function owned by this TU:
//   EaTraxData::Prepare  @ 0x82697538
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// See BrnMusicEffect.h for the layout map and the un-DWARF'd-member-type FLAG.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace MusicEffect
{

// ---------------------------------------------------------------------------
// EaTraxData::Prepare  @ 0x82697538
//
//   if (!lpLogicModule)
//       assert("lpLogicModule", BrnMusicEffect.h:720)        ; non-gating tripwire
//   maEaTraxHandles[0..3] = 0                                ; std r11(=0) @0/8/0x10/0x18
//   miTrackIndex0 = miTrackIndex1 = miTrackIndex2 = -1       ; stw r10(=-1) @0x30/0x34/0x38
//   mpLogicModule = lpLogicModule                            ; stw r4 @0x3C
//   miState = 0                                              ; stw r11(=0) @0x40
//   mbReserved0x44 = 0                                       ; stb r11(=0) @0x44
//   return 1
//
// Store-for-store with the X360: the four 8-byte stores ARE zero (the Hex-Rays
// `0xFFFFFFFF00000000uLL` literal is a misread of `li r11,0 ; std r11`), the three
// index sentinels are -1, the logic-module pointer is captured, and the two
// trailing scalars reset to 0. Returns true (X360 `li r3, 1`). All members reached
// BY NAME -- no raw offset stores, no absolute-offset static_asserts across the
// 64-bit pointer member.
// ---------------------------------------------------------------------------
bool EaTraxData::Prepare( BrnSound::Logic::Module::SoundLogicModule* lpLogicModule )
{
    CGS_ASSERT( lpLogicModule != nullptr, "lpLogicModule" );

    maEaTraxHandles[0] = 0;
    maEaTraxHandles[1] = 0;
    maEaTraxHandles[2] = 0;
    maEaTraxHandles[3] = 0;

    miTrackIndex0 = -1;
    miTrackIndex1 = -1;
    miTrackIndex2 = -1;

    mpLogicModule = lpLogicModule;

    miState        = 0;
    mbReserved0x44 = 0;

    return true;
}

} // namespace MusicEffect
} // namespace Logic
} // namespace BrnSound
