#ifndef BRN_SOUND_LOGIC_BRN_HUD_EFFECT_H
#define BRN_SOUND_LOGIC_BRN_HUD_EFFECT_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                        // Vector3 (GetSlipstreamAmount params)
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)

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

// BrnHUDEffect.h:106 (DWARF). HUDEffect : public BrnSound::Logic::BrnEffectObject.
// Grown from the previous member-less shell to the DWARF base + the two ledger
// functions that fit this slice (ctor + GetSlipstreamAmount) plus the dtor anchor that
// drives the compiler-synthesised vector deleting destructor + adjustor{4} thunks. The
// full member surface (maVoices[3] VoiceWrapper, maGameModeVoices[4] CustomHudVoice,
// mMusicStream, mHudMessageData presentationcomponent, + the effect vtable) uses
// un-homed types and is DEFERRED / declaration-only per the ExplosionEffect/
// StreamingEffect minimal-home convention -- NOT fabricated here. Only mGameModeData
// (the committed nested GameModeData) is materialised as a real member.
struct HUDEffect : public BrnSound::Logic::BrnEffectObject
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

    // @ 0x826E1940 -- the anchor ctor. Bodied in BrnHUDEffect.cpp; base + members
    // default-construct (matching the ExplosionEffect minimal-home ctor shape).
    HUDEffect();

    // @ 0x826E1E00 vector deleting destructor + @ 0x826E1BC0 adjustor{4} forward to
    // this virtual dtor (compiler-synthesised; empty leaf anchor in the .cpp).
    virtual ~HUDEffect();

    CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const override;
    const char* GetTypeName() const override;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectObject* CreateObject(u32 auType);

    // @ 0x82686BA8. Slipstream-audio proximity/alignment weight for the car in front.
    // The three Vector3 args arrive in vector registers (v1/v2/v3); `this` (r3) is
    // unused by the body. DWARF renders the return as `double`.
    f64 GetSlipstreamAmount( Vector3 lCarInFrontVelocity,
                             Vector3 lOwnWeightedVelocity,
                             Vector3 lNegatedDirection );

    // DWARF :338 (@ this+0x430). The per-game-mode HUD-audio data block, default-
    // constructed by the ctor. Materialised BY NAME (its generics live in the committed
    // CgsSoundUtils home).
    GameModeData mGameModeData;

    // FLAG: the DWARF also lists maVoices[3] (VoiceWrapper), maGameModeVoices[4]
    // (CustomHudVoice), mMusicStream (BrnSound::Logic::MusicStream), mHudMessageData
    // (Attrib::Gen::presentationcomponent) + trailing scalars, using types with NO
    // committed home -> DECLARATION-ONLY / DEFERRED (NOT emitted as concrete members;
    // same treatment as StreamingEffect::streamsettings). UpdateSlipStreaming
    // @ 0x82701BA8 is BLOCKED (raw-offset seams into un-homed module/interface state).
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_HUD_EFFECT_H
