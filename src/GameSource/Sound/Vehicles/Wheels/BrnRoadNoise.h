#ifndef BRN_SOUND_VEHICLES_WHEELS_ROAD_NOISE_H
#define BRN_SOUND_VEHICLES_WHEELS_ROAD_NOISE_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"            // CgsSound::Utils::PathLine<2> / DataPoint<T> / Curve (BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"    // CgsSound::Logic::VoiceWrapper member (BY NAME)
#include "GameSource/AttribSys/Generated/classes/surfacelist.h"    // Attrib::Gen::surfacelist member (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Wheels::RoadnoiseEffect
//   GameSource/Sound/Vehicles/Wheels/BrnRoadNoise.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX + DWARF (BrnRoadNoise.h). RoadnoiseEffect
// is the per-car road/surface-loop noise EFFECT OBJECT (DWARF BrnRoadNoise.h:36:
// RoadnoiseEffect : public BrnEffectObject). It cross-fades per-surface tyre loops
// through TransitionEnvelope stage machines and plays surface-transition one-shots
// through a pool of transition voices.
//
// FULL MEMBER SURFACE (DWARF BrnRoadNoise.h:157..171, in declared order):
//   mpWheelControl, mpPhysicsControl, mafLoopBaseVolume[2], maLoopEnvelope[2]
//   (TransitionEnvelope), mRoadNoiseVoice[2] (VoiceWrapper), muSurfaceID[2] (u8),
//   muRoadnoiseLoop[2] (DataPoint<s32>), mTransitionsSounds[3] (TransitionSound),
//   miLastVoiceUsed (u8), mSurfaceList (Attrib::Gen::surfacelist).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// absolute offsets are NOT static_asserted across pointer members on the 64-bit host.
// The X360 array strides (VoiceWrapper 0x50, TransitionSound 0x54, TransitionEnvelope
// 0x40) are documented in the .cpp; on the host the embedded types keep their own
// homed sizes, so the strides are not reproduced byte-for-byte.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels
{

// Forward decls for the two control back-pointers (never dereferenced by this TU).
struct WheelControl;
} // namespace Wheels
namespace Engines
{
struct PhysicsControl;
} // namespace Engines
namespace Wheels
{

// BrnRoadNoise.h:36 (DWARF). Reuses the committed BrnEffectObject dual base BY NAME.
struct RoadnoiseEffect : public BrnSound::Logic::BrnEffectObject
{
    // DWARF BrnWheelControl.h:53 -- WheelControl::EWheelSide. Mirrored here so
    // TransitionSound::meSide has a home without editing the committed WheelControl
    // (which does not yet materialise this enum).
    enum EWheelSide
    {
        E_LEFT_HAND_SIDE  = 0,
        E_RIGHT_HAND_SIDE = 1,
        E_MAX_SIDES       = 2,
    };

    // BrnRoadNoise.h:90 (DWARF). One surface-transition one-shot slot: a wrapped voice
    // plus the wheel side it plays on. Ctor default-constructs only the embedded voice.
    struct TransitionSound
    {
        TransitionSound() : meSide( E_LEFT_HAND_SIDE ) {}

        CgsSound::Logic::VoiceWrapper mTransitionVoice; // BrnRoadNoise.h:101
        EWheelSide                    meSide;           // BrnRoadNoise.h:102
    };

    // BrnRoadNoise.h:125 (DWARF). One cross-fade stage machine: a two-stage PathLine
    // (mManilla) plus the tracked output volume (mVolume). Setup builds a 2-stage
    // attack/release envelope on mManilla seeded from mVolume's current value.
    struct TransitionEnvelope
    {
        TransitionEnvelope() {}

        // @ 0x826B8738 (DWARF BrnRoadNoise.h:148). void Setup(attack, release, peak).
        void Setup( f32 lfAttackTime, f32 lfReleaseTime, f32 lfPeakLevelOffset );

        CgsSound::Utils::PathLine<2u>   mManilla;   // BrnRoadNoise.h:126
        CgsSound::Utils::DataPoint<f32> mVolume;    // BrnRoadNoise.h:127
    };

    static const u8 KI_NUMBER_OF_TRANSITION_VOICES = 3; // BrnRoadNoise.h:160

    RoadnoiseEffect();               // @ 0x826E56C0
    virtual ~RoadnoiseEffect();      // anchor for the vector deleting destructor @ 0x826E5AC8

    // ---- members (DWARF-attested; declared order == DWARF BrnRoadNoise.h) ----
    WheelControl*                    mpWheelControl;                       // :157
    BrnSound::Vehicles::Engines::PhysicsControl* mpPhysicsControl;         // :158
    f32                              mafLoopBaseVolume[2];                 // :162
    TransitionEnvelope               maLoopEnvelope[2];                    // :163
    CgsSound::Logic::VoiceWrapper    mRoadNoiseVoice[2];                   // :164
    u8                               muSurfaceID[2];                       // :165
    CgsSound::Utils::DataPoint<s32>  muRoadnoiseLoop[2];                   // :166
    TransitionSound                  mTransitionsSounds[KI_NUMBER_OF_TRANSITION_VOICES]; // :168
    u8                               miLastVoiceUsed;                      // :169
    Attrib::Gen::surfacelist         mSurfaceList;                         // :171
};

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_WHEELS_ROAD_NOISE_H
