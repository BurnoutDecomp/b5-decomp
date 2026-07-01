#include "GameSource/Sound/Vehicles/Wheels/BrnRoadNoise.h"

// =============================================================================
// BrnSound::Vehicles::Wheels::RoadnoiseEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX + DWARF (BrnRoadNoise.h,
// CgsSoundUtils.h for PathLine<2>/Curve::ECurveType).
//
// Bodied here:
//   RoadnoiseEffect::TransitionEnvelope::Setup    @ 0x826B8738  (real)
//   RoadnoiseEffect::`vector deleting destructor'  @ 0x826E5AC8  (-> ~RoadnoiseEffect anchor)
//
// BLOCKED (NOT bodied): RoadnoiseEffect::RoadnoiseEffect @ 0x826E56C0. Its inlined
// full-object ctor constructs an array of embedded sub-objects whose TYPES are UN-HOMED
// (mRoadNoiseVoice[2] CgsSound::Logic::VoiceWrapper array, mTransitionsSounds[3]
// RoadnoiseEffect::TransitionSound array, mSurfaceList Attrib::Gen::surfacelist), plus
// muRoadnoiseLoop[2] (DataPoint<s32>) and the mafLoopBaseVolume/muSurfaceID leaf
// scalars. Homing that full layout (VoiceWrapper is a 1-byte committed stand-in, so the
// post-array offsets are not byte-accurate; surfacelist/TransitionSound have no home)
// would require fabricating un-attested members. Per the anti-fabrication HARD RULE this
// one function is blocked pending those types' homes; its declaration remains for the
// class shape but no body is emitted.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels
{

// ---------------------------------------------------------------------------
// RoadnoiseEffect::TransitionEnvelope::Setup  @ 0x826B8738
//   DWARF BrnRoadNoise.h:148 -> void Setup(float32_t, float32_t, float32_t)
//   (args: attack-time [s], release-time [s], peak-level offset)
//
// Build a two-stage volume envelope on mManilla, seeded from the envelope's current
// volume, then linked back down to unity:
//   mManilla.ClearStages();  (twice, verbatim X360)
//   start = mVolume.GetCurrent();
//   mManilla.AddStage(start, peakOffset + 1.0f, attack * 1000.0f, E_ONE_MINUS_EQPWR);
//   mManilla.mfCurrentValue = start;
//   mManilla.AddLinkedStage(1.0f, release * 1000.0f, E_POWER);
//
// flt_82009E10 == 1000.0f (seconds->milliseconds), flt_82001C98 == 1.0f. Curve values
// per DWARF: E_ONE_MINUS_EQPWR == 3, E_POWER == 1. The X360 issues ClearStages twice
// back-to-back (second immediately after reloading the current volume) -- reproduced.
// ---------------------------------------------------------------------------
void RoadnoiseEffect::TransitionEnvelope::Setup( f32 lfAttackTime,
                                                 f32 lfReleaseTime,
                                                 f32 lfPeakLevelOffset )
{
    const f32 KF_SECONDS_TO_MS = 1000.0f; // flt_82009E10

    mManilla.ClearStages();

    const f32 lfStartLevel = mVolume.GetCurrent(); // lfs f31, 0x38(this)
    mManilla.ClearStages();                        // second clear (X360)

    mManilla.AddStage( lfStartLevel,
                       lfPeakLevelOffset + 1.0f,
                       lfAttackTime * KF_SECONDS_TO_MS,
                       CgsSound::Utils::Curve::E_ONE_MINUS_EQPWR ); // curve == 3

    mManilla.mfCurrentValue = lfStartLevel; // stfs f31, 0x30(this)

    mManilla.AddLinkedStage( 1.0f,
                             lfReleaseTime * KF_SECONDS_TO_MS,
                             CgsSound::Utils::Curve::E_POWER ); // curve == 1
}

// ---------------------------------------------------------------------------
// ~RoadnoiseEffect  @ 0x826E5AC8  (anchor for the X360 `vector deleting destructor').
// The observable teardown (BrnEffectObject base settle + embedded member dtors) is the
// inherited base destructor chain (BY NAME); this leaf anchor adds nothing of its own.
// It is the class's vtable emission point. The (a2 & 1) allocator-free tail is
// re-synthesised by the host toolchain.
// ---------------------------------------------------------------------------
RoadnoiseEffect::~RoadnoiseEffect()
{
}

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound
