#ifndef BRN_SOUND_VEHICLES_WHEELS_ROAD_NOISE_H
#define BRN_SOUND_VEHICLES_WHEELS_ROAD_NOISE_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"            // CgsSound::Utils::PathLine<2> / DataPoint<f32> / Curve (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Wheels::RoadnoiseEffect  (+ nested TransitionEnvelope)
//   GameSource/Sound/Vehicles/Wheels/BrnRoadNoise.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// RoadnoiseEffect is the per-car road/surface-loop noise EFFECT OBJECT (DWARF
// BrnRoadNoise.h:36: RoadnoiseEffect : public BrnEffectObject). It cross-fades
// per-surface tyre loops through TransitionEnvelope stage machines.
//
// This slice bodies the two TU functions that DON'T need un-homed types:
//   RoadnoiseEffect::TransitionEnvelope::Setup   @ 0x826B8738  (real -- PathLine BY NAME)
//   RoadnoiseEffect::`vector deleting destructor' @ 0x826E5AC8 (-> ~RoadnoiseEffect anchor)
//
// BLOCKED (see .cpp): RoadnoiseEffect::RoadnoiseEffect @ 0x826E56C0. Its inlined
// full-object ctor constructs an array of un-homed sub-objects (mRoadNoiseVoice[2]
// VoiceWrapper, mTransitionsSounds[3] TransitionSound, mSurfaceList Attrib::Gen::
// surfacelist) whose types are not homed anywhere in src; per the anti-fabrication
// rule that one function is blocked pending those homes.
//
// FLAG (MINIMAL home): only the nested TransitionEnvelope (for Setup) + the base (BY
// NAME) are materialised. The full RoadnoiseEffect member surface (mafLoopBaseVolume,
// maLoopEnvelope[2], mRoadNoiseVoice[2], muSurfaceID[2], muRoadnoiseLoop[2],
// mTransitionsSounds[3], miLastVoiceUsed, mSurfaceList, mpWheelControl,
// mpPhysicsControl) is DECLARATION-DEFERRED with the blocked ctor.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// absolute offsets are NOT static_asserted across pointer members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels
{

// BrnRoadNoise.h:36 (DWARF). Reuses the committed BrnEffectObject dual base BY NAME.
struct RoadnoiseEffect : public BrnSound::Logic::BrnEffectObject
{
    // BrnRoadNoise.h:27 (DWARF). One cross-fade stage machine: a two-stage PathLine
    // (mManilla) plus the tracked output volume (mVolume). Setup builds a 2-stage
    // attack/release envelope on mManilla seeded from mVolume's current value.
    struct TransitionEnvelope
    {
        TransitionEnvelope() {}

        // @ 0x826B8738 (DWARF BrnRoadNoise.h:148). void Setup(attack, release, peak).
        void Setup( f32 lfAttackTime, f32 lfReleaseTime, f32 lfPeakLevelOffset );

        CgsSound::Utils::PathLine<2u>       mManilla;   // BrnRoadNoise.h:126
        CgsSound::Utils::DataPoint<f32>     mVolume;    // BrnRoadNoise.h:127
    };

    RoadnoiseEffect();               // @ 0x826E56C0 (BLOCKED -- see .cpp)
    virtual ~RoadnoiseEffect();      // anchor for the vector deleting destructor @ 0x826E5AC8
};

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_WHEELS_ROAD_NOISE_H
