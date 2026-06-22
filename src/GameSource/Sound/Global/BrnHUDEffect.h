#ifndef BRN_SOUND_LOGIC_BRN_HUD_EFFECT_H
#define BRN_SOUND_LOGIC_BRN_HUD_EFFECT_H

#include "types.hpp"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"

// =============================================================================
// BrnSound::Logic::HUDEffect::GameModeData
//   GameSource/Sound/Global/BrnHUDEffect.h (DWARF home) +
//   GameSource/Sound/Global/BrnHUDEffect.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. GameModeData is the nested
// per-game-mode HUD-audio data block carried by HUDEffect: the event score/time/
// boost data points the HUD-audio logic reads to drive its voices (score ticks,
// stunt combo, showtime).
//
// DWARF (BrnHUDEffect.h:36, nested in HUDEffect at :106) gives the full layout:
//   struct GameModeData {
//       Average<10u, f32>   mEventScoreDelta;       // :312
//       DataPoint<f32>      mEventTimeRemaining;     // :313
//       DataPoint<f32>      mEventBoostAmount;       // :314
//       f32                 mfTimeSinceScoreTick;    // :315
//       DataPoint<s32>      mStuntScore;             // :318
//       DataPoint<s32>      mStuntComboMultiplier;   // :319
//       DataPoint<s32>      mStuntResultScore;       // :320
//       DataPoint<f32>      mShowtimeScore;          // :323
//       DataPoint<f32>      mShowtimeBoostDelta;     // :324
//       bool                mbTimeExtended;          // :327
//   };
// This is the OWNING home for the nested GameModeData (no committed home existed);
// the layout is modelled BY NAME reusing the (additively-grown)
// CgsSound::Utils::Average<10,f32> and CgsSound::Utils::DataPoint<T> generics.
//
// This TU's recon'd function set is exactly TWO entries:
//   GameModeData::GameModeData (Construct)  @ 0x826AFE88
//   GameModeData::Reset                     @ 0x826977D8
//
// FLAG (shape vs full surface): only the nested GameModeData (Construct + Reset)
// is materialised here. The enclosing HUDEffect (its BrnEffectObject base, the
// CustomHudVoice members, UpdateGameModeHud and the RTTI/effect surface) is
// DEFERRED to its own TU(s); HUDEffect is declared minimally so GameModeData can
// be its nested type and so the `mGameModeData` member relationship is expressed.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 ctor/Reset write members by
// absolute byte offset (mEventScoreDelta @ +0..47, mfTimeSinceScoreTick @ +64,
// the DataPoints in sequence, mbTimeExtended @ +108). Members are pinned BY NAME
// and SEQUENCE; absolute offsets are NOT static_asserted.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// BrnHUDEffect.h:106 (DWARF). Minimal declaration so the nested GameModeData has
// its enclosing scope. The full HUDEffect surface (its BrnEffectObject base,
// CustomHudVoice members, UpdateGameModeHud, RTTI hooks) is DEFERRED.
struct HUDEffect
{
    // BrnHUDEffect.h:36 (DWARF). Per-game-mode HUD-audio data block.
    struct GameModeData
    {
        // BrnHUDEffect.h:298 (DWARF). Zero-initialise the block. @ 0x826AFE88.
        GameModeData();

        // BrnHUDEffect.h:305 (DWARF). Logical reset between events (combo
        // multiplier defaults to 1, time-extended flag set). @ 0x826977D8.
        void Reset();

        // ORDER mirrors the DWARF / X360 access sequence.
        CgsSound::Utils::Average<10, f32> mEventScoreDelta;     // :312  (+0..47)
        CgsSound::Utils::DataPoint<f32>   mEventTimeRemaining;  // :313  (+48,+52)
        CgsSound::Utils::DataPoint<f32>   mEventBoostAmount;    // :314  (+56,+60)
        f32                               mfTimeSinceScoreTick; // :315  (+64)
        CgsSound::Utils::DataPoint<s32>   mStuntScore;          // :318  (+68,+72)
        CgsSound::Utils::DataPoint<s32>   mStuntComboMultiplier;// :319  (+76,+80)
        CgsSound::Utils::DataPoint<s32>   mStuntResultScore;    // :320  (+84,+88)
        CgsSound::Utils::DataPoint<f32>   mShowtimeScore;       // :323  (+92,+96)
        CgsSound::Utils::DataPoint<f32>   mShowtimeBoostDelta;  // :324  (+100,+104)
        bool                              mbTimeExtended;       // :327  (+108)
    };
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_HUD_EFFECT_H
