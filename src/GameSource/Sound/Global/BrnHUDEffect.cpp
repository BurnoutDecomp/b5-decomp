#include "GameSource/Sound/Global/BrnHUDEffect.h"

// =============================================================================
// BrnSound::Logic::HUDEffect::GameModeData -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnHUDEffect.h for the layout
// rationale and the X360-32-bit-vs-host-64-bit offset note.
//
// This TU's recon'd function set is exactly two entries:
//   GameModeData::GameModeData (Construct)  @ 0x826AFE88
//   GameModeData::Reset                     @ 0x826977D8
//
// dep_flags: none un-homed for THIS TU. Every member written is modelled BY NAME
// in the owning header (the Average<10,f32> / DataPoint<T> generics live in the
// committed CgsSoundUtils home).
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// HUDEffect::GameModeData::GameModeData  @ 0x826AFE88  (the DWARF Construct)
//
//   for ( i = 0; i < 10; ++i ) *(4*i + this) = 0.0;   ; mEventScoreDelta.maPoints
//   *(this+44) = 0.0;  *(this+40) = 0;                 ; mfAverage / muNextPoint
//   *(this+48..60) = 0.0;                              ; time-remaining / boost DPs
//   *(this+68..104) = 0;                               ; stunt + showtime DPs
//
// The X360 ctor zeroes every field it touches (it does NOT write +64
// mfTimeSinceScoreTick nor +108 mbTimeExtended -- those are left as Reset's job).
// Reconstructed as explicit member zeroing in DWARF order.
// ---------------------------------------------------------------------------
HUDEffect::GameModeData::GameModeData()
{
    // mEventScoreDelta: zero the 10 sample points, the next-point index and the
    // cached average (the i<10 loop + the +40/+44 stores).
    for (u32 lu = 0; lu < 10u; ++lu)
    {
        mEventScoreDelta.maPoints[lu] = 0.0f;
    }
    mEventScoreDelta.muNextPoint = 0;
    mEventScoreDelta.mfAverage   = 0.0f;

    mEventTimeRemaining.Flush(0.0f);   // +48,+52
    mEventBoostAmount.Flush(0.0f);     // +56,+60

    mStuntScore.Flush(0);              // +68,+72
    mStuntComboMultiplier.Flush(0);    // +76,+80
    mStuntResultScore.Flush(0);        // +84,+88

    mShowtimeScore.Flush(0.0f);        // +92,+96
    mShowtimeBoostDelta.Flush(0.0f);   // +100,+104
}

// ---------------------------------------------------------------------------
// HUDEffect::GameModeData::Reset  @ 0x826977D8
//
//   *(this+44) = 0.0;                       ; mEventScoreDelta.mfAverage
//   do { *(4*v1 + this) = 0.0; } while (v1 < 10);  ; maPoints
//   *(this+40) = 0;                          ; muNextPoint
//   *(this+48..60) = 0.0;                    ; time-remaining / boost DPs
//   *(this+64) = 0.0;                        ; mfTimeSinceScoreTick
//   *(this+68) = 0; *(this+72) = 0;          ; mStuntScore
//   *(this+76) = 1; *(this+80) = 1;          ; mStuntComboMultiplier = 1 (cur+prev)
//   *(this+84) = 0; *(this+88) = 0;          ; mStuntResultScore
//   *(this+92..104) = 0.0;                   ; showtime DPs
//   *(this+108) = 1;                         ; mbTimeExtended = true
//
// Reset differs from Construct: it ALSO clears mfTimeSinceScoreTick (+64), defaults
// the stunt combo multiplier to 1 (both current and previous words), and sets
// mbTimeExtended true.
// ---------------------------------------------------------------------------
void HUDEffect::GameModeData::Reset()
{
    mEventScoreDelta.mfAverage = 0.0f;
    for (u32 lu = 0; lu < 10u; ++lu)
    {
        mEventScoreDelta.maPoints[lu] = 0.0f;
    }
    mEventScoreDelta.muNextPoint = 0;

    mEventTimeRemaining.Flush(0.0f);   // +48,+52
    mEventBoostAmount.Flush(0.0f);     // +56,+60

    mfTimeSinceScoreTick = 0.0f;       // +64

    mStuntScore.Flush(0);              // +68,+72
    mStuntComboMultiplier.Flush(1);    // +76,+80  (combo defaults to 1)
    mStuntResultScore.Flush(0);        // +84,+88

    mShowtimeScore.Flush(0.0f);        // +92,+96
    mShowtimeBoostDelta.Flush(0.0f);   // +100,+104

    mbTimeExtended = true;             // +108
}

} // namespace Logic
} // namespace BrnSound
