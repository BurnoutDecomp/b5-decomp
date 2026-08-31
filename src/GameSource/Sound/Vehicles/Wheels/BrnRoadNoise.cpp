#include "GameSource/Sound/Vehicles/Wheels/BrnRoadNoise.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Logic/CgsState.h"
#include "GameShared/GameClasses/Sound/Logic/CgsStateManager.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameSource/AttribSys/Generated/classes/audiosurface.h"
#include "GameSource/AttribSys/Generated/classes/surface.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Vehicles/Wheels/BrnWheelControl.h"
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"

#include <algorithm>

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

namespace
{
const u32 KAU_ROADNOISE_PARAMETERS[9] =
{
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_pitch")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_volume")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_volume_railtrack")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_car_speed")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_surface_type_front")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_surface_type_back")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_azimuth")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_surface_type_previous")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_volume_transition")),
};

const u32 KU_SEND01 = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));
const u32 KU_REVERB_SEND = static_cast<u32>(CgsSound::Playback::Name::MakeHash("ReverbSend"));
const u64 KU_SURFACE_LIST_COLLECTION = Attrib::StringToKey("340654");

Attrib::Gen::audiosurface GetAudioSurface(const Attrib::Gen::surfacelist& arSurfaceList,
                                          u8 auSurface)
{
    const void* lpSurfaceRefData = arSurfaceList.Surfaces(auSurface);
    if (!lpSurfaceRefData)
        lpSurfaceRefData = Attrib::DefaultDataArea(sizeof(Attrib::RefSpec));

    const Attrib::RefSpec& lrSurfaceRef =
        *static_cast<const Attrib::RefSpec*>(lpSurfaceRefData);
    const Attrib::Gen::surface lSurface(lrSurfaceRef);
    return Attrib::Gen::audiosurface(lSurface.AudioSurface());
}
}

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

const char* RoadnoiseEffect::GetTypeName() const
{
    return "RoadnoiseEffect";
}

s32 RoadnoiseEffect::GetController(s32 aiSlot)
{
    if (aiSlot == 0)
        return 1;
    if (aiSlot == 1)
        return 0;
    return -1;
}

void RoadnoiseEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    switch (apController->GetEffectID())
    {
    case 0:
        mpPhysicsControl =
            static_cast<BrnSound::Vehicles::Engines::PhysicsControl*>(apController);
        break;
    case 1:
        mpWheelControl = static_cast<WheelControl*>(apController);
        break;
    default:
        break;
    }
}

bool RoadnoiseEffect::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;

    mSurfaceList.ChangeWithDefault(KU_SURFACE_LIST_COLLECTION);

    CgsSound::Logic::Content* lpSurfaceContent = nullptr;
    if (GetStateBase() && GetStateBase()->GetStateManager())
    {
        const CgsSound::Playback::Name lContentName("surface_patch_bank.abi");
        lpSurfaceContent = GetStateBase()->GetStateManager()->GetContent(lContentName);
    }
    CGS_ASSERT(lpSurfaceContent != nullptr,
               "lpPlayerStateManager->GetContent( K_SURFACES_BANK_NAME )");

    CgsSound::Logic::VoiceWrapper::CreateParams lParams;
    lParams.mpLogicModule = GetLogicModule();
    lParams.mFactoryName =
        static_cast<u32>(CgsSound::Playback::AemsFactorySkName().GetValue());
    lParams.mVoiceSpecName =
        static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_surface_class"));
    lParams.mpContent = lpSurfaceContent;
    lParams.mSlotName =
        static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_Slot"));
    lParams.mSendName = KU_SEND01;
    lParams.mSubMixVoiceID = 1;
    lParams.mReverbSendName = KU_REVERB_SEND;
    lParams.mReverbSubMixVoiceID = 2;
    lParams.miSendIndex = 0;

    for (s32 liSide = 0; liSide < E_MAX_SIDES; ++liSide)
    {
        mRoadNoiseVoice[liSide].Create(lParams);
        mRoadNoiseVoice[liSide].Play(0);
    }
    return true;
}

void RoadnoiseEffect::UpdateParams(f32 afTimeStep)
{
    SetMixerInputValue(0, 0);

    CGS_ASSERT(mpWheelControl != nullptr, "mpWheelControl");
    if (!mpWheelControl)
        return;

    for (s32 liSide = 0; liSide < E_MAX_SIDES; ++liSide)
    {
        const s32 liWheelNumber = liSide == E_LEFT_HAND_SIDE ? 0 : 1;
        const WheelControl::SingleWheelStatus lWheelStatus =
            mpWheelControl->GetSingleWheelStatus(liWheelNumber);
        const u8 luCurrentSurface = lWheelStatus.mSurfaceType.GetCurrent();
        const u8 luPreviousSurface = lWheelStatus.mSurfaceType.GetPrevious();

        if (luCurrentSurface != luPreviousSurface)
        {
            const Attrib::Gen::audiosurface lAudioSurface =
                GetAudioSurface(mSurfaceList, luCurrentSurface);
            const s32 liRoadnoiseLoop = lAudioSurface.mRoadnoiseLoop();
            muRoadnoiseLoop[liSide].Update(liRoadnoiseLoop);
            mpWheelControl->SetRoadnoiseLoop(liRoadnoiseLoop,
                                             static_cast<u8>(liSide));
            maLoopEnvelope[liSide].Setup(lAudioSurface.EnvelopeAttackTime(),
                                         lAudioSurface.EnvelopeDecayTime(),
                                         lAudioSurface.EnvelopeVolume());
            mafLoopBaseVolume[liSide] = lAudioSurface.SurfaceLoopVolume();

            // The shipped build constructs the old surface's audiosurface instance
            // here even though transition one-shots were compiled out. Preserve that
            // lifetime/resolve side effect and the current-surface latch.
            const Attrib::Gen::audiosurface lPreviousAudioSurface =
                GetAudioSurface(mSurfaceList, luPreviousSurface);
            (void)lPreviousAudioSurface;
            muSurfaceID[liSide] = luCurrentSurface;
            SetMixerInputValue(0, 0x7FFF);
        }

        mRoadNoiseVoice[liSide].Update();
        maLoopEnvelope[liSide].mManilla.Update(afTimeStep);
        maLoopEnvelope[liSide].mVolume.Update(
            maLoopEnvelope[liSide].mManilla.mfCurrentValue);
    }

    for (s32 liVoice = 0; liVoice < KI_NUMBER_OF_TRANSITION_VOICES; ++liVoice)
        mTransitionsSounds[liVoice].mTransitionVoice.Update();
}

void RoadnoiseEffect::ProcessUpdate()
{
    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    if (!mpPhysicsControl)
        return;

    const f32 lfReverbSend =
        GetMixerOutputValue(8, Nicotine::DMixIO::DMX_VOL) / 32767.0f;
    const f32 lfOverallVolume = GetMixerOutputValue(0, Nicotine::DMixIO::DMX_VOL);

    for (s32 liSide = 0; liSide < E_MAX_SIDES; ++liSide)
    {
        const f32 lfAzimuth =
            GetMixerOutputValue(liSide + 3, Nicotine::DMixIO::DMX_AZIM);
        const f32 lfLoopVolume = (std::min)(32767.0f, (std::max)(0.0f,
            mafLoopBaseVolume[liSide] * maLoopEnvelope[liSide].mVolume.GetCurrent() *
            lfOverallVolume * 0.70710677f));
        const f32 lfTransitionVolume =
            GetMixerOutputValue(6, Nicotine::DMixIO::DMX_VOL);
        const f32 lfPitch =
            GetMixerOutputValue(liSide + 1, Nicotine::DMixIO::DMX_PITCH);
        const f32 lfSpeed =
            mpPhysicsControl->GetPhysicsData().mSpeedMPH.GetCurrent();
        const f32 lfSurface =
            static_cast<f32>(muRoadnoiseLoop[liSide].GetCurrent());
        const f32 lfPreviousSurface =
            static_cast<f32>(muRoadnoiseLoop[liSide].GetPrevious());

        CgsSound::Logic::VoiceWrapper& lrVoice = mRoadNoiseVoice[liSide];
        lrVoice.SetParameter(6, lfAzimuth, &KAU_ROADNOISE_PARAMETERS[6]);
        lrVoice.SetParameter(1, lfLoopVolume, &KAU_ROADNOISE_PARAMETERS[1]);
        lrVoice.SetParameter(8, lfTransitionVolume, &KAU_ROADNOISE_PARAMETERS[8]);
        lrVoice.SetParameter(0, lfPitch, &KAU_ROADNOISE_PARAMETERS[0]);
        lrVoice.SetParameter(3, lfSpeed, &KAU_ROADNOISE_PARAMETERS[3]);
        lrVoice.SetParameter(4, lfSurface, &KAU_ROADNOISE_PARAMETERS[4]);
        lrVoice.SetParameter(5, lfSurface, &KAU_ROADNOISE_PARAMETERS[5]);
        lrVoice.SetParameter(7, lfPreviousSurface, &KAU_ROADNOISE_PARAMETERS[7]);
        lrVoice.SetGain(1, lfReverbSend, &KU_REVERB_SEND);
        lrVoice.SetGain(0, 2.0f, &KU_SEND01);
    }
}

bool RoadnoiseEffect::Detach()
{
    for (s32 liVoice = 0; liVoice < KI_NUMBER_OF_TRANSITION_VOICES; ++liVoice)
        mTransitionsSounds[liVoice].mTransitionVoice.Release();
    for (s32 liSide = 0; liSide < E_MAX_SIDES; ++liSide)
        mRoadNoiseVoice[liSide].Release();
    return BrnSound::Logic::BrnEffectObject::Detach();
}

void RoadnoiseEffect::PlayTransitionSound(EWheelSide aeSide,
                                          u8,
                                          u8)
{
    miLastVoiceUsed = static_cast<u8>((miLastVoiceUsed + 1u) %
                                      KI_NUMBER_OF_TRANSITION_VOICES);
    TransitionSound& lrTransition = mTransitionsSounds[miLastVoiceUsed];
    const s32 liState = lrTransition.mTransitionVoice.GetState();
    if (liState >= CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_CREATE &&
        liState <= CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_PLAYING)
        lrTransition.mTransitionVoice.Release();
    lrTransition.meSide = aeSide;
}

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound
