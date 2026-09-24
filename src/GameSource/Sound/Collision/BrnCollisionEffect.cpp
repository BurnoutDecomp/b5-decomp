#include "GameSource/Sound/Collision/BrnCollisionEffect.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameSource/Sound/Collision/BrnCollisionControl.h"
#include "GameSource/Sound/Collision/BrnCollisionState.h"
#include "GameSource/Sound/Collision/BrnCollisionStateManager.h"
#include "GameSource/AttribSys/Generated/classes/crashbin.h"
#include "GameSource/AttribSys/Generated/classes/propscrashbin.h"
#include "GameSource/Sound/Global/BrnGlobalStateManager.h"
#include "GameSource/World/BrnEntityTypes.h"   // BrnWorld::E_ENTITYTYPE_* (CalculateIntensity)

#include <algorithm>
#include <cstdio>
#include <cstdlib>

// =============================================================================
// BrnSound::Logic::Collision::CollisionEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnCollisionEffect.h for the
// base-reuse rationale and the X360-32-bit-vs-host-64-bit offset note.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

// ---------------------------------------------------------------------------
// CollisionEffect  @ 0x826AFD20  (field-init ctor)
//
//   stfs f0(=0.0), 0x20(this)            ; base mfRunningTime/mfDeltaTime region
//   stfs f0,       0x1C(this)            ; base field
//   stw  off_820AE954, 4(this)           ; (transient) IResourceRequester sub-vptr
//   sth  0, 0x10 ; stw 0, 0xC ; stw 0, 0x34 ; stw 0, 8 ; stb 0, 0x30 ; stw 0,0x28
//   stw  0, 0x24 ; sth 0, 0x12           ; base + leaf zero-init region
//   stw  off_820B1390, 0(this)           ; primary vptr (CollisionEffect vtable)
//   stw  off_820B135C, 4(this)           ; IResourceRequester sub-object vptr (final)
//   stw  0, 0x38 ; stw 0, 0x3C           ; mePrepareState / mbFirstUpdate+mbUseAzimuth
//   stw  &off_820B0E20, 0x44(this)       ; mCrashVoice vtable (Voice vptr @ +0x44)
//   stw  0, 0x48 ; stw 0, 0x4C           ; mCrashVoice handle/owner words
//   stfs f0(=0.0),  0x50(this)           ; mfIntensity = 0.0f
//   stw  5,         0x54(this)           ; meNicotineVolumeSlider = 5
//   stw  5,         0x58(this)           ; meNicotinePitchSlider  = 5
//   stfs f13(=1.0), 0x5C(this)           ; mSizeSettings.mfVolume = 1.0f
//   stfs f13,       0x60(this)           ; mSizeSettings.mfPitch  = 1.0f
//
// The leading vptr stores (+0/+4) and the base-region zero stores are the
// compiler-emitted base sub-object construction (BrnEffectObject's own ctor chain),
// produced implicitly here by the base default ctor. This body sets the LEAF
// members the X360 explicitly initialises. The EffectBase sub-object sits at +4 (the
// off_820B135C vtable holds its virtuals), so the +0x34 store is EffectBase::mpDynamicMixIo
// (EffectBase+0x30, base region) and mpCollisionControl is the +0x38 store (EffectBase+0x34,
// AttachController @0x8268812C) -- FX-VOICEPOOL 2026-09-24; see GetGain.
// ---------------------------------------------------------------------------
CollisionEffect::CollisionEffect()
    : mpCollisionControl(nullptr)         // stw 0, 0x38
    , mePrepareState(E_PREPARE_STATE_CONSTRUCT_VOICE) // stw 0, 0x3C
    , mbFirstUpdate(false)                // +0x40: the X360 ctor leaves it; Attach sets it
    , mbUseAzimuth(false)                 // +0x41: the X360 ctor leaves it; Attach sets it
    , mfIntensity(0.0f)                   // stfs 0.0, 0x50
    , meNicotineVolumeSlider(5)           // stw 5, 0x54
    , meNicotinePitchSlider(5)            // stw 5, 0x58
    // mCrashVoice: default-constructed (Voice vptr @ +0x44 + nulled handle/owner)
    // mSizeSettings: default-constructed (mfVolume/mfPitch = 1.0f -> +0x5C/+0x60)
{
}

// ---------------------------------------------------------------------------
// ~CollisionEffect  @ 0x826C90D8  (the X360 `vector deleting destructor')
//
//   bl   CollisionEffect::~CollisionEffect          ; chain to the real dtor
//   if (a2 & 1) {                                   ; the `delete' half
//       memset(&local[1], 0, 16); local[0] = this;
//       (*(*off_82FFB954 + 0x14))(off_82FFB954, local) ; free via MemBase allocator
//   }
//   return this
//
// The leaf destructor adds no teardown of its own: the X360 inner ~CollisionEffect
// is the inherited BrnEffectObject dual-base settle (both vptr stores + the
// attach/detach/resources-ready member clears), identical store-for-store to the
// committed BrnEffectObject dtor @ 0x826AF4C8 and the sibling SingleGinsuEffect
// dtor. In reconstructed C++ the dual-base settle and the deleting-destructor thunk
// are compiler-synthesised from the virtual dtor declared in the header, so the
// hand-written body is empty.
// FLAG: the (a2 & 1) tail frees the object through the global sound MemBase
// allocator (off_82FFB954); that allocator's vtable call is not homed here, so the
// `delete' half of the vector deleting destructor is left to the host toolchain
// rather than reproducing the raw allocator vtable dispatch.
// ---------------------------------------------------------------------------
CollisionEffect::~CollisionEffect()
{
}

CgsSound::Logic::EffectObject* CollisionEffect::CreateObject(u32 /*auAllocator*/)
{
    return new CollisionEffect();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>*
CollisionEffect::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo(
        0x50000, "CollisionEffect",
        CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
        &CollisionEffect::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>*
CollisionEffect::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* CollisionEffect::GetTypeName() const
{
    return "CollisionEffect";
}

s32 CollisionEffect::GetController(s32 aiIndex)
{
    return aiIndex == 0 ? 0 : -1;
}

void CollisionEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    CGS_ASSERT(apController != nullptr, "lpController");
    if (!apController)
        return;
    if (apController->GetEffectID() == 0)
        mpCollisionControl = static_cast<CollisionControl*>(apController);
    else
        CGS_ASSERT(false, "Cound't attach controller");
}

bool CollisionEffect::Prepare(CgsSound::Logic::State* apState)
{
    switch (mePrepareState)
    {
    case E_PREPARE_STATE_CONSTRUCT_VOICE:
        if (!CgsSound::Logic::EffectBase::Prepare(apState))
            return false;

        mCrashVoice.Construct(
            GetLogicModule(), GetLogicModule()->GetUniqueId(),
            static_cast<u32>(CgsSound::Playback::Name::MakeHash(
                "~SplicerFactory::SK_NAME~")),
            static_cast<u32>(CgsSound::Playback::Name::MakeHash(
                "SplicerVoiceSpec")));
        mePrepareState = E_PREPARE_STATE_CONNECT_VOICE;
        // ARTIST falls through: a voice created synchronously can be connected on
        // this pass; an asynchronously created voice returns false until ready.
    case E_PREPARE_STATE_CONNECT_VOICE:
    {
        if (!mCrashVoice.IsReady())
            return false;

        BrnSound::Logic::GlobalStateManager* lpGlobalStateManager =
            static_cast<BrnSound::Logic::GlobalStateManager*>(
                GetLogicModule()->GetEnvironment().GetStateManager(0));
        CGS_ASSERT(lpGlobalStateManager != nullptr, "lpGlobalStateManager");
        if (!lpGlobalStateManager)
            return false;

        const s32 liCollisionSubmixIdent =
            lpGlobalStateManager->GetSubmixVoice(
                BrnSound::Logic::GlobalStateManager::E_SUBMIX_VOICE_COLLISION)
                .GetIdent();
        mCrashVoice.Connect(
            static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01")),
            static_cast<u32>(liCollisionSubmixIdent));
        return true;
    }
    default:
        return true;
    }
}

// ---------------------------------------------------------------------------
// GetSizeSpecificSettings<T>  crashbin @0x826AABC8 / propscrashbin @0x826AAC88 (identical code)
//
//   lwz r11,0x38(r4)   ; meSize
//   0 Large  -> layout+0x08 / +0x18 (Volumes().z / Pitch().z)
//   1 Medium -> layout+0x04 / +0x14 (.y)
//   2 Small  -> layout+0x00 / +0x10 (.x)
//   >= 3     -> assert "Bad Size" (cpp:611), settings left as they were
// ---------------------------------------------------------------------------
template <typename T>
void CollisionEffect::GetSizeSpecificSettings(const OutputCollision& arCollision, const T& arBin,
                                              SizeSpecificSettings& arSettings) const
{
    switch (arCollision.meSize)
    {
    case E_SIZE_LARGE:
        arSettings.mfVolume = arBin.Volumes().z;
        arSettings.mfPitch = arBin.Pitch().z;
        break;
    case E_SIZE_MEDIUM:
        arSettings.mfVolume = arBin.Volumes().y;
        arSettings.mfPitch = arBin.Pitch().y;
        break;
    case E_SIZE_SMALL:
        arSettings.mfVolume = arBin.Volumes().x;
        arSettings.mfPitch = arBin.Pitch().x;
        break;
    default:
        CGS_ASSERT(false, "Bad Size");
        break;
    }
}

// ---------------------------------------------------------------------------
// InitWork<T>  crashbin @0x826EB240 / propscrashbin @0x826EB368 (DWARF BrnCollisionEffect.h:126)
//
//   T lBin(arCollision.mBinKey, 0)                      ; ld r4,0x90(r29) -> the bin ctor
//   lpStateManager = the state's manager (+0x24)        ; assert "lpStateManager" (cpp:541)
//   mCrashVoice.Attach("~SplicerPlayerVoice::Slot~", the manager's splicer bank [meBankType])
//   mCrashVoice.SetGain(0, 0.0, "Send01")               ; 0x826EB2D4..0x826EB2F4: the voice starts
//                                                       ;   silent -- ProcessUpdate sets its gain
//   mCrashVoice.Play(arCollision.miSampleID)            ; lwz r4,0xD8(r29)
//   meNicotinePitchSlider = 1 ; meNicotineVolumeSlider = the bin's MixerSlider (layout +0x16C)
//   assert meNicotineVolumeSlider > eCollisionMixerSliders::CollisionCutoff (cpp:561)
//   GetSizeSpecificSettings<T>(arCollision, lBin, mSizeSettings)
// ---------------------------------------------------------------------------
template <typename T>
void CollisionEffect::InitWork(CollisionState* apState, const OutputCollision& arCollision)
{
    const T lBin(arCollision.mBinKey, nullptr);
    CollisionStateManager* lpManager =
        static_cast<CollisionStateManager*>(apState->GetStateManager());
    CGS_ASSERT(lpManager != nullptr, "lpStateManager");

    mCrashVoice.Attach(
        static_cast<s32>(CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Slot~")),
        lpManager->GetSplicerBank(static_cast<ECollisionSpliceBankType>(arCollision.meBankType)));
    const u32 luSend01 = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));
    mCrashVoice.SetGain(0, 0.0f, &luSend01);
    mCrashVoice.Play(arCollision.miSampleID);

    meNicotinePitchSlider = 1;
    meNicotineVolumeSlider = lBin.MixerSlider();
    CGS_ASSERT(meNicotineVolumeSlider > 2,
               "meNicotineVolumeSlider > AttribSys::Enums::eCollisionMixerSliders::CollisionCutoff");

    GetSizeSpecificSettings<T>(arCollision, lBin, mSizeSettings);
}

// ---------------------------------------------------------------------------
// CalculateIntensity  @ 0x82688240  (DWARF BrnCollisionEffect.cpp:434)
//
//   x = arCollision.mNormalizedImpulse.x                ; lfs f31,0x80(r31)
//   meAction (+0x30) != Collision                        -> 0.0 (flt_82001CC0)
//   mePipeline (+4) E_PROP                               -> flt_82F2CEC8 (12.0) * x
//   anything but E_REGULAR / E_PROP                      -> assert "Bad pipeline" (cpp:453), then regular
//   regular: the owner bytes of maEntityID[0] / [1] (lbz +0x18 / +0x1C, BrnWorld entity types)
//     A race car: B world -> flt_82F2CED0, B race car -> flt_82F2CECC, B traffic -> flt_82F2CED4
//                 (all 100.0), any other B -> 0.0
//     A traffic  -> flt_82F2CED4 (100.0)
//     A world, A >= 3 -> 0.0
//     result = (K - flt_82F2CEC4 (25.0)) * x + 25.0      ; fsubs / fmadds 0x82688418..0x8268841C
//   The "ducking: ..." TTY lines are gated on dword_82FFB8E0, a debug switch with no writer in the
//   image -- not reproduced.
// ---------------------------------------------------------------------------
f32 CollisionEffect::CalculateIntensity(const OutputCollision& arCollision)
{
    const f32 KF_MIN_INTENSITY = 25.0f;           // flt_82F2CEC4
    const f32 KF_PROP_INTENSITY_SCALE = 12.0f;    // flt_82F2CEC8
    const f32 KF_RACECAR_VS_RACECAR = 100.0f;     // flt_82F2CECC
    const f32 KF_RACECAR_VS_WORLD = 100.0f;       // flt_82F2CED0
    const f32 KF_VERSUS_TRAFFIC = 100.0f;         // flt_82F2CED4

    const f32 lfImpulse = arCollision.mNormalizedImpulse.x;
    if (arCollision.meAction != AttribSys::Enums::eAction::Collision)
        return 0.0f;

    if (arCollision.mePipeline == InputCollision::E_PROP)
        return KF_PROP_INTENSITY_SCALE * lfImpulse;
    CGS_ASSERT(arCollision.mePipeline == InputCollision::E_REGULAR, "Bad pipeline");

    const u32 luTypeA = arCollision.maEntityID[0].muValue >> 24;   // lbz +0x18: the owner byte
    const u32 luTypeB = arCollision.maEntityID[1].muValue >> 24;   // lbz +0x1C
    f32 lfFullIntensity;
    if (luTypeA == BrnWorld::E_ENTITYTYPE_RACECAR)
    {
        if (luTypeB == BrnWorld::E_ENTITYTYPE_WORLD)
            lfFullIntensity = KF_RACECAR_VS_WORLD;
        else if (luTypeB == BrnWorld::E_ENTITYTYPE_RACECAR)
            lfFullIntensity = KF_RACECAR_VS_RACECAR;
        else if (luTypeB == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE)
            lfFullIntensity = KF_VERSUS_TRAFFIC;
        else
            return 0.0f;
    }
    else if (luTypeA == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE)
    {
        lfFullIntensity = KF_VERSUS_TRAFFIC;
    }
    else
    {
        return 0.0f;   // the world, or any other owner
    }

    return (lfFullIntensity - KF_MIN_INTENSITY) * lfImpulse + KF_MIN_INTENSITY;
}

// ---------------------------------------------------------------------------
// Attach  @ 0x826F8218  (runs on the EffectBase sub-object, vtable off_820B135C)
//
//   mbUseAzimuth = 1 ; EffectBase::Attach() (+0x24 = 0, ++attach count)
//   control finished (+0x34 -> +0xA0)          -> return 1
//   assert mCrashVoice.IsCreated() (cpp:383)
//   state = EffectBase::mpState (+8); collision = the state's output collision (+0x60)
//   mePipeline 0 -> InitWork<crashbin>, 1 -> InitWork<propscrashbin>,
//   else assert "Bad pipeline" (cpp:390) and InitWork<crashbin>
//   i = max(CalculateIntensity(collision) * flt_820B78E0 (327.67), 0)      ; fneg / fsel
//   SetMixerInputValue(0, 0x7FFF)
//   SetMixerInputValue(1, fctiwz(min(i, flt_820AD310 (32767.0))))        ; fsubs / fsel / fctiwz
//   collision.meFatality (+0x3C) == E_FATAL_START -> mbUseAzimuth = 0     ; 0x826F8334..0x826F8340
//   mbFirstUpdate = 1 ; return 1
//
// FX-VOICEPOOL 2026-09-24: the PC sent 32767 - min(32767, max(0, x) * 327.67) as the ducking
// input (inverted, and without CalculateIntensity's 25..100 shaping -- every hit near full
// scale), started the voice at its previous gain (no SetGain(0) before Play), dropped the
// azimuth on meAction == Detach instead of meFatality == E_FATAL_START, and stored an
// mfIntensity the console's Attach never writes.
// ---------------------------------------------------------------------------
bool CollisionEffect::Attach()
{
    const f32 KF_INTENSITY_TO_MIXER = 327.67001f;   // flt_820B78E0 (0x43A3D5C3)
    const f32 KF_MIXER_INPUT_MAX = 32767.0f;        // flt_820AD310

    mbUseAzimuth = true;
    CgsSound::Logic::EffectBase::Attach();
    if (mpCollisionControl->GetCollisionFinished())
        return true;

    CGS_ASSERT(mCrashVoice.GetVoiceObject() != nullptr, "mCrashVoice.IsCreated()");
    CollisionState* lpState = static_cast<CollisionState*>(GetStateBase());
    const OutputCollision& lrCollision = lpState->GetOutputCollision();
    switch (lrCollision.mePipeline)
    {
    case InputCollision::E_REGULAR:
        InitWork<Attrib::Gen::crashbin>(lpState, lrCollision);
        break;
    case InputCollision::E_PROP:
        InitWork<Attrib::Gen::propscrashbin>(lpState, lrCollision);
        break;
    default:
        CGS_ASSERT(false, "Bad pipeline");
        InitWork<Attrib::Gen::crashbin>(lpState, lrCollision);
        break;
    }

    // fneg / fsel: a negative intensity takes 0 (a NaN keeps its value).
    f32 lfIntensity = CalculateIntensity(lrCollision) * KF_INTENSITY_TO_MIXER;
    if (-lfIntensity >= 0.0f)
        lfIntensity = 0.0f;
    SetMixerInputValue(0, 0x7FFF);
    // fsubs / fsel: above 32767 takes 32767; fctiwz truncates toward zero.
    const f32 lfDucking = (KF_MIXER_INPUT_MAX - lfIntensity >= 0.0f) ? lfIntensity : KF_MIXER_INPUT_MAX;
    SetMixerInputValue(1, static_cast<s32>(lfDucking));

    if (lrCollision.meFatality == E_FATAL_START)
        mbUseAzimuth = false;
    mbFirstUpdate = true;

    // [DIAG] NOT IN THE X360 BINARY (BRN_COLLISION_AUDIO_DIAG): the voice start -- the inputs
    // CalculateIntensity reads (pipeline, action, the two owner bytes, the impulse) and what it
    // sends the mixer (duck = input 1), so a log line alone re-derives the console's value.
    if (std::getenv("BRN_COLLISION_AUDIO_DIAG") != nullptr && CgsDev::Log::gpDebugPrint)
    {
        static u32 suPrintCount = 0;
        if (suPrintCount++ < 64u)
        {
            char lacLine[256];
            std::snprintf(lacLine, sizeof(lacLine),
                          "[collision-audio] voice attached sample=%d pipeline=%d action=%d owners=%u/%u "
                          "impulse=%.9g fatality=%d mixer=%d volume=%g pitch=%g duck=%d azimuth=%d\n",
                          lrCollision.miSampleID, static_cast<s32>(lrCollision.mePipeline),
                          static_cast<s32>(lrCollision.meAction),
                          static_cast<u32>(lrCollision.maEntityID[0].muValue >> 24),
                          static_cast<u32>(lrCollision.maEntityID[1].muValue >> 24),
                          static_cast<double>(lrCollision.mNormalizedImpulse.x),
                          static_cast<s32>(lrCollision.meFatality),
                          static_cast<s32>(meNicotineVolumeSlider),
                          static_cast<double>(mSizeSettings.mfVolume),
                          static_cast<double>(mSizeSettings.mfPitch),
                          static_cast<s32>(lfDucking), mbUseAzimuth ? 1 : 0);
            *CgsDev::Log::gpDebugPrint << lacLine;
        }
    }
    return true;
}

void CollisionEffect::UpdateParams(f32 /*afDeltaTime*/)
{
}

void CollisionEffect::ProcessUpdate()
{
    if (GetDMixIOPtr())
        GetDMixIOPtr()->SetDMixInput(0, 0);
    if (!mpCollisionControl || mpCollisionControl->GetCollisionFinished())
        return;

    const f32 lfGain = GetGain();
    const f32 lfPitch = GetPitch();
    f32 lfAzimuth = 0.0f;
    if (mbUseAzimuth && GetDMixIOPtr())
    {
        lfAzimuth = static_cast<f32>(
            GetDMixIOPtr()->GetDMixOutput(0, 3)) *
            0.0054932479f;
    }
    mbFirstUpdate = false;

    if (!mCrashVoice.IsPlaying())
    {
        // [DIAG] NOT IN THE X360 BINARY (BRN_COLLISION_AUDIO_DIAG): the crash voice ended (its
        // splice ran every sample to the end of its envelope) -- the control releases the state
        // on its next UpdateParams. pitch / gain are this frame's GetPitch() / GetGain().
        if (std::getenv("BRN_COLLISION_AUDIO_DIAG") != nullptr && CgsDev::Log::gpDebugPrint)
        {
            static u32 suFinishedPrintCount = 0;
            const CollisionState* lpState = mpCollisionControl->GetCollisionState();
            if (lpState && suFinishedPrintCount++ < 256u)
            {
                char lacLine[192];
                std::snprintf(lacLine, sizeof(lacLine),
                              "[collision-audio] voice finished sample=%d bin=%d attached=%.3f now=%.3f "
                              "pitch=%g gain=%g\n",
                              lpState->GetOutputCollision().miSampleID,
                              static_cast<s32>(lpState->GetOutputCollision().miBinIndex),
                              static_cast<double>(lpState->GetTimeWeAttached()),
                              static_cast<double>(lpState->GetCurrentTime()),
                              static_cast<double>(lfPitch), static_cast<double>(lfGain));
                *CgsDev::Log::gpDebugPrint << lacLine;
            }
        }
        mpCollisionControl->SetCollisionFinished(true);
        return;
    }

    const u32 luSend01 = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("Send01"));
    const u32 luPitch = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Pitch~"));
    const u32 luAzimuth = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Azimuth~"));
    mCrashVoice.SetGain(0, lfGain, &luSend01);
    mCrashVoice.SetParameter(1, lfAzimuth, &luAzimuth);
    mCrashVoice.SetParameter(0, lfPitch, &luPitch);
}

bool CollisionEffect::Detach()
{
    if (mCrashVoice.GetVoiceObject())
    {
        mCrashVoice.Stop();
        mCrashVoice.Detach(static_cast<s32>(
            CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Slot~")));
    }
    if (GetDMixIOPtr())
        GetDMixIOPtr()->SetDMixInput(1, 0);
    return BrnSound::Logic::BrnEffectObject::Detach();
}

// ---------------------------------------------------------------------------
// GetGain  @ 0x82688138
//
//   lwz  r3, 0x34(this)       ; r3 = EffectBase::mpDynamicMixIo -- the EFFECT's own endpoint
//   lwz  r4, 0x54(this)       ; r4 = meNicotineVolumeSlider
//   cmplwi r3, 0
//   beq  zero                 ; no endpoint -> gain contribution 0
//     li   r5, 0              ; preset = 0  (DMX_VOL)
//     bl   Nicotine::DMixIO::GetDMixOutput(r3, r4, 0)
//     extsw r11, r3           ; SIGN-extend the s32 result
//     fcfid / frsp            ; (s64)->f64->f32
//     fmuls f0, f12, flt_820AA8F8   ; * 3.0518509e-05 (0x38000100, the Q15 scale)
//     b    tail
//   zero:
//     lfs  f0, flt_82001CC0   ; f0 = 0.0
//   tail:
//   lfs  f13, 0x5C(this)      ; f13 = mSizeSettings.mfVolume
//   fmuls f1, f13, f0         ; return mfVolume * scaledMixerOutput
//   blr
//
// Reads the effect's dynamic-mixer output for the volume slider, converts the signed
// Q15 mixer value to a normalised f32, and scales it by the size-specific volume.
// When the effect has no endpoint the mixer contribution is 0.0f (so the gain is 0).
//
// WHOSE ENDPOINT (FX-VOICEPOOL, 2026-09-24). `this` here is the primary object (ProcessUpdate
// @0x826BCFE0 calls it with `addi r31,r30,-4`); CollisionEffect's EffectBase sub-object sits at
// +4 (its vtable off_820B135C, stored by the ctor at 0x826AFD8C, holds AttachController,
// Attach, ProcessUpdate and Detach), so +0x34 is EffectBase+0x30 = mpDynamicMixIo, the
// endpoint DynamicMixer::ConnectDMixIO gives the EFFECT (SetDMixIOPtr @0x826808D8
// `stw r31,0x30(this)`). The control lives one word further (EffectBase+0x34 = +0x38,
// AttachController @0x8268812C). The PC read the CONTROL's endpoint here, which the mix map
// never connects: gain and pitch were 0 on every crash voice -- silent, and at pitch 0 the
// splice clock never reached the end of a sample, so no crash voice ever finished and the
// seven collision states filled after seven impacts.
// ---------------------------------------------------------------------------
f32 CollisionEffect::GetGain() const
{
    // flt_820AA8F8 = 3.0518509e-05 (0x38000100): the Q15 -> [0,1] scale.
    const f32 KF_Q15_TO_NORMALISED = 0.000030518509f;

    f32 lfMixerOutput;
    Nicotine::DMixIO* lpDmix = GetDMixIOPtr();   // lwz r3,0x34(this): the effect's own endpoint
    if (lpDmix != nullptr) // cmplwi; beq
    {
        // GetDMixOutput(slot, preset): preset 0 == DMX_VOL. extsw -> signed s32.
        const s32 liOutput =
            lpDmix->GetDMixOutput(meNicotineVolumeSlider, Nicotine::DMixIO::DMX_VOL);
        lfMixerOutput = static_cast<f32>(liOutput) * KF_Q15_TO_NORMALISED;
    }
    else
    {
        lfMixerOutput = 0.0f; // lfs f0, flt_82001CC0
    }

    return mSizeSettings.mfVolume * lfMixerOutput; // fmuls f1, f13, f0
}

// ---------------------------------------------------------------------------
// GetPitch  @ 0x826881B0
//
//   lwz  r3, 0x34(r31)        ; EffectBase::mpDynamicMixIo -- the effect's own endpoint (see GetGain)
//   lwz  r4, 0x58(r31)        ; meNicotinePitchSlider
//   lfs  f31, flt_82001CC0    ; 0.0
//   cmplwi r3, 0 ; beq none
//     li r5, 1 ; bl Nicotine::DMixIO::GetDMixOutput(r3, r4, DMX_PITCH)
//     extsw / fcfid / frsp ; fmuls f13, f13, flt_820AA8F4   ; * 0.000244140625 (Q12)
//   none: f13 = f31 (0.0)
//   lfs  f0, 0x60(r31)        ; mSizeSettings.mfPitch
//   fcmpu f0, f31 ; bne keep  ; a size pitch of 0.0 ...
//   lfs  f0, flt_82001C98     ; ... is taken as 1.0
//   keep: fmuls f1, f0, f13
// ---------------------------------------------------------------------------
f32 CollisionEffect::GetPitch() const
{
    // flt_820AA8F4 = 0.000244140625 (0x39800000): the Q12 pitch-multiplier scale.
    const f32 KF_Q12_TO_PITCH = 0.000244140625f;

    Nicotine::DMixIO* lpDmix = GetDMixIOPtr();   // lwz r3,0x34(r31): the effect's own endpoint
    f32 lfMixerOutput = 0.0f;                    // flt_82001CC0
    if (lpDmix)
    {
        lfMixerOutput = static_cast<f32>(lpDmix->GetDMixOutput(
            meNicotinePitchSlider, Nicotine::DMixIO::DMX_PITCH)) *
            KF_Q12_TO_PITCH;
    }

    f32 lfSizePitch = mSizeSettings.mfPitch;     // lfs f0,0x60(r31)
    if (lfSizePitch == 0.0f)                     // fcmpu f0,f31 ; bne (NaN keeps its value)
        lfSizePitch = 1.0f;                      // flt_82001C98
    return lfSizePitch * lfMixerOutput;
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const
    gpCollisionEffectReg =
        CgsSound::Logic::EffectObject::AddToClassTypeInfoArray(
            CollisionEffect::GetStaticTypeInfo());

// =============================================================================
// BrnSound::Logic::Collision::Collision3DControl -- out-of-line bodies.
// (Added to the existing CollisionEffect TU home; both classes share the DWARF
//  source BrnCollisionEffect.h. See the header for the inheritance rationale.)
//
// This slice's recon'd function set is three entries:
//   Collision3DControl()           @ 0x826E88D0
//   CreateObject(u32)              @ 0x826F80E8
//   `scalar deleting destructor'   @ 0x826F8168
// =============================================================================

// ---------------------------------------------------------------------------
// Collision3DControl::Collision3DControl  @ 0x826E88D0
//
//   bl   Brn3DEffectControl::Brn3DEffectControl   ; GRANDPARENT ctor (mEngineDataAtrib etc.)
//   li   r10, 0 ; stw r10, 0xD0(r31)              ; mpTransform            = nullptr
//   stw  &off_820B6240, 0(r31)                    ; primary vptr (implicit)
//   stvx128 v0, r31, 0xE0                          ; mPositionInUserSpace   = {0,0,0}
//   stvx128 v0, r31, 0xF0                          ; mDirectionInUserSpace  = {0,0,0}
//   stvx128 v0, r31, 0x100                         ; mGeneratedPosition     = {0,0,0}
//   stvx128 v0, r31, 0x110                         ; mGeneratedDirection    = {0,0,0}
//   return r31
//
// The X360 body INLINES the Brn3DUserSpaceEffectControl construction: it calls the
// grandparent Brn3DEffectControl() directly, then nulls mpTransform (+0xD0) and
// zero-inits the four 16-byte Vector3s (+0xE0/+0xF0/+0x100/+0x110). Delegating to
// Brn3DUserSpaceEffectControl() (which chains Brn3DEffectControl() + nulls mpTransform
// + SetZero()s the four Vector3s) reproduces that sequence BY NAME; Collision3DControl
// adds NO members of its own. The +0(vptr) store is the compiler-emitted leaf
// final-overrider vptr write (the RTTI table off_820B6240 is DEFERRED to its own TU).
// ---------------------------------------------------------------------------
Collision3DControl::Collision3DControl()
    : Brn3DUserSpaceEffectControl() // chains Brn3DEffectControl() + nulls mpTransform + SetZero()s the 4 Vector3s
{
}

// ---------------------------------------------------------------------------
// Collision3DControl::CreateObject(u32)  @ 0x826F80E8   (the factory hook)
//
//   if ( a1 ) { if ( MemBase::operator new(288, "Collision3DControl", 1) ) return new'd ctor; }
//   else      { if ( MemBase::operator new(288, "Collision3DControl", 0) ) return new'd ctor; }
//   return 0;
//
// The X360 allocates a 288-byte (0x120) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "Collision3DControl" and placement-constructs a
// Collision3DControl into it. Both arms call the SAME size+ctor; the `a1` argument
// only selects the operator-new flavour (0/1). DWARF (BrnCollisionEffect.h:199)
// attests `EffectControl* CreateObject(uint32_t)` (non-virtual, non-const).
//
// FLAG (allocator gate): CgsSound::MemBase does NOT model operator new(size, tag,
// flavour) (off_82FFB954 not homed in this group), so a faithful placement-new is not
// yet expressible. This uses the host `new`; the observable result matches. Mirrors
// the committed CollisionStateManager::CreateObject @ 0x82701FA8. The 0x120 size is
// documentation only and is NOT passed to the host new.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectControl* Collision3DControl::CreateObject( u32 /*luType*/ )
{
    return new Collision3DControl();
}

// ---------------------------------------------------------------------------
// ~Collision3DControl  @ 0x826F8168  (the X360 `scalar deleting destructor')
//
//   stw  &off_820B3670, 0(this)                    ; leaf vptr @ entry
//   bl   Attrib::Instance::~Instance(this + 0xB0)  ; destroy mEngineDataAtrib (base-owned)
//   li   r9, 3 ; stw r9, 0x24(this)                ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  &off_820AA820, 0(this)                    ; final base vptr settle
//   stb  0, 0x2D(this)                             ; (un-homed byte flag) = 0
//   stw  0, 0x20(this)                             ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound MemBase allocator) }
//   return this
//
// Every store above is produced by the compiler-generated virtual-destructor chain:
// the leaf vptr install, the intermediate Brn3DUserSpaceEffectControl teardown
// (trivial Vector3 members), and Brn3DEffectControl's destructor which tears down
// mEngineDataAtrib (@ +0xB0 via the committed Attrib::Instance::~Instance) and settles
// meDetachState/meAttachState + the final vptr. So this leaf body adds nothing of its
// own and is empty -- identical treatment to the sibling Passby3DControl dtor
// (BrnPassbyEffect.cpp @ 0x826E8ED0).
//
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free
// the object; that allocator is not homed here, so the `delete` half is left to the
// host toolchain (same treatment as the committed Brn3DEffectControl / Passby3DControl
// homes).
// ---------------------------------------------------------------------------
Collision3DControl::~Collision3DControl()
{
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>*
Collision3DControl::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> sTypeInfo(
        0x50010, "Collision3DControl",
        CgsSound::Logic::EffectControl::GetStaticTypeInfo(),
        &Collision3DControl::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>*
Collision3DControl::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* Collision3DControl::GetTypeName() const
{
    return "Collision3DControl";
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const
    gpCollision3DControlReg =
        CgsSound::Logic::EffectControl::AddToClassTypeInfoArray(
            Collision3DControl::GetStaticTypeInfo());

} // namespace Collision
} // namespace Logic
} // namespace BrnSound
