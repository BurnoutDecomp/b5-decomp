#include "GameSource/Sound/Vehicles/Wheels/BrnRoadNoise.h"

// =============================================================================
// BrnSound::Vehicles::Wheels::RoadnoiseEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX + DWARF (BrnRoadNoise.h,
// CgsSoundUtils.h for PathLine<2>/Curve::ECurveType).
//
// Bodied here:
//   RoadnoiseEffect::RoadnoiseEffect               @ 0x826E56C0  (full-object ctor)
//   RoadnoiseEffect::TransitionEnvelope::Setup     @ 0x826B8738  (real)
//   RoadnoiseEffect::`vector deleting destructor'  @ 0x826E5AC8  (-> ~RoadnoiseEffect anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels
{

// ---------------------------------------------------------------------------
// RoadnoiseEffect::RoadnoiseEffect  @ 0x826E56C0   (MSVC inlined full-object ctor)
//
// The dual-vptr install + base-region zero stores (0x826E56D4..0x826E5748) are the
// compiler-emitted BrnEffectObject base sub-object construction, reproduced by the base
// default ctor (reused BY NAME). The body then, store-for-store with the X360:
//
//  * loops the two TransitionEnvelopes @ this+0x48 stride 0x40 (0x826E574C..0x826E5800):
//      each PathLine<2> mManilla ctor-inits to zero (the two inner stage-zeroing loops),
//      seeds mVolume = DataPoint<f32>(1.0f) (stfs f30 @ +0x38/+0x3C, f30 == 1.0f),
//      marks mManilla.mbComplete = true (stb 1 @ +0x34), calls
//      mManilla.AddStage(1.0f, 1.0f, 1.0f, E_LINEAR) (f1=f2=f3=f30=1.0, r7=0), then
//      sets mManilla.mfCurrentValue = 1.0f (stfs f30 @ +0x30).
//  * loops the two mRoadNoiseVoice[2] VoiceWrappers @ this+0xC8 stride 0x50.
//  * zeroes muRoadnoiseLoop[2] (DataPoint<s32>) @ this+0x16C..0x178 (four words).
//  * loops the three mTransitionsSounds[3] embedded VoiceWrappers @ this+0x17C stride 0x54.
//  * miLastVoiceUsed = 0 (stb @ +0x278); mSurfaceList = surfacelist(nullptr, nullptr)
//    (bl surfacelist::surfacelist(this+0x27C, 0, 0)).
//  * muSurfaceID[2] = {0,0} (stb @ +0x168/+0x169).
//
// flt_82001CC0 == 0.0f (f31, every zero store), flt_82001C98 == 1.0f (f30). The embedded
// member ctors above are delegated to the initializer list; the body reproduces the
// per-element seed stores the X360 inlines.
// ---------------------------------------------------------------------------
RoadnoiseEffect::RoadnoiseEffect()
    : BrnEffectObject()          // installs the base vptrs + zero-inits the base members (BY NAME)
    , mpWheelControl( 0 )
    , mpPhysicsControl( 0 )
    // maLoopEnvelope[2] / mRoadNoiseVoice[2] / muRoadnoiseLoop[2] / mTransitionsSounds[3]
    // / mSurfaceList are default-constructed here (the bl VoiceWrapper / PathLine ctor /
    // surfacelist ctor the X360 inlines per element).
    , miLastVoiceUsed( 0 )       // stb r31, 0x278(this)
    , mSurfaceList( 0, 0 )       // bl Attrib::Gen::surfacelist::surfacelist(this+0x27C, 0, 0)
{
    mafLoopBaseVolume[0] = 0.0f; // stfs f31, 0x1C(this)   (f31 == 0.0f)
    mafLoopBaseVolume[1] = 0.0f; // stfs f31, 0x20(this)

    // ----- the two cross-fade envelopes (this+0x48, stride 0x40) --------------------
    for ( s32 li = 0; li < 2; ++li )
    {
        TransitionEnvelope& lrEnvelope = maLoopEnvelope[li];

        // mVolume seeded to 1.0f (both DataPoint samples) -- stfs f30 @ +0x38/+0x3C.
        lrEnvelope.mVolume.Flush( 1.0f );

        lrEnvelope.mManilla.mbComplete = true;                                   // stb 1, 0x34
        lrEnvelope.mManilla.AddStage( 1.0f, 1.0f, 1.0f,
                                      CgsSound::Utils::Curve::E_LINEAR );        // r7 == 0
        lrEnvelope.mManilla.mfCurrentValue = 1.0f;                              // stfs f30, 0x30
    }

    // ----- the surface loop-index trackers zero (this+0x16C..0x178) -----------------
    for ( s32 li = 0; li < 2; ++li )
    {
        muRoadnoiseLoop[li].Flush( 0 ); // stw r31 @ +0x16C/+0x170 (cur) / +0x174/+0x178 (prev)
    }

    // ----- the two per-side surface IDs zero (this+0x168/0x169) ---------------------
    muSurfaceID[0] = 0; // stb r31, 0x168(this)
    muSurfaceID[1] = 0; // stb r31, 0x169(this)
}

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
