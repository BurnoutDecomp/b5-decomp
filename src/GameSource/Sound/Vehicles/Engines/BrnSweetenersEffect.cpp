#include "GameSource/Sound/Vehicles/Engines/BrnSweetenersEffect.h"

#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnShiftControl.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Module/LogicModule/BrnMessageData.h"
#include "GameSource/Sound/Module/SharedIO/BrnPreUpdateSharedIo.h"
#include "GameSource/Sound/Global/BrnGlobalStateManager.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/Global/BrnHudSoundDiag.h"   // the opt-in [sweeteners-attach] witness only

#include <cmath>

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

namespace
{
u32 guDamagePopRoundRobin = 0;
u32 guDamageBangRoundRobin = 0;
u32 guSweetenersDiagAttach = 0;   // [sweeteners-attach] witness budget (NOT X360; see Attach)

f32 VectorElement(const Vector4& arValue, s32 aiIndex)
{
    switch (aiIndex)
    {
    case 0: return arValue.x;
    case 1: return arValue.y;
    case 2: return arValue.z;
    default: return arValue.w;
    }
}
}

SweetenersEffect::SweetenersEffect()
    : BrnEffectObject()
    , mbEnableSweetners(false)
    , mExhaustPopsVoice()
    , mSweetenerVoice()
    , mRandomGenerator()
    , mpPhysicsControl(nullptr)
    , mpShiftingControl(nullptr)
    , mSweetenersBank()
    , mfTimeRemainingToPlaySputters(0.0f)
    , mPopsSquareWave()
    , mi16SweetenerSlot(0)
    , mi8UpdateState(E_SWEETENER_STATE_NONE)
    , mi8CurrentBankElement(E_SWEETENER_BANK_ELEMENT_COUNT)
    , meRaceCarEngineState(
          BrnWorld::RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_OFF)
    , miRaceCarIndex(0)
    , mpFXBank(nullptr)
    , mDamageBangVoice()
    , mfDamageBangVolume(0.0f)
    , mDamagePopVoice()
    , mfDamagePopVolume(0.0f)
    , mfDelayToBang(-1.0f)
    , mfDeleayToVFXFire(-1.0f)
    , mCarStarting()
    , mFadeOutEngine()
    , mfTimeIntoStart(0.0f)
    , meCarStartingState(E_CARSTARTINGSTATE_NONE)
{
    mRandomGenerator.Construct();
}

SweetenersEffect::~SweetenersEffect()
{
}

BrnSound::Logic::IResourceRequester* SweetenersEffect::CreateObjec(u32 auFlavour)
{
    (void)auFlavour;
    return static_cast<BrnSound::Logic::IResourceRequester*>(new SweetenersEffect());
}

s32 SweetenersEffect::GetController(s32 aiSlot)
{
    if (aiSlot == 0)
        return 0;
    if (aiSlot == 1)
        return 2;
    return -1;
}

void SweetenersEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    CGS_ASSERT(apController != nullptr, "apController");
    if (!apController)
        return;

    switch (apController->GetEffectID())
    {
    case 0:
        mpPhysicsControl = static_cast<PhysicsControl*>(apController);
        break;
    case 2:
        mpShiftingControl = static_cast<ShiftControl*>(apController);
        break;
    default:
        CGS_ASSERT(false, "Unexpected control.");
        break;
    }
}

CgsSound::Logic::VoiceWrapper::CreateParams SweetenersEffect::MakeSplicerParams(
    const CgsSound::Logic::Content* apContent) const
{
    CgsSound::Logic::VoiceWrapper::CreateParams lParams;
    lParams.mpLogicModule = GetLogicModule();
    lParams.mFactoryName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("~SplicerFactory::SK_NAME~"));
    lParams.mVoiceSpecName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("SplicerVoiceSpec"));
    lParams.mpContent = apContent;
    lParams.mContentSpecName = 0;
    lParams.mSlotName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Slot~"));
    lParams.mSendName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("Send01"));
    lParams.mSubMixVoiceID = 1;
    lParams.mReverbSendName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("ReverbSend"));
    lParams.mReverbSubMixVoiceID = 2;
    lParams.miSendIndex = 0;
    return lParams;
}

void SweetenersEffect::CreateVoices()
{
    const CgsSound::Logic::VoiceWrapper::CreateParams lParams =
        MakeSplicerParams(&mSweetenersBank);
    mExhaustPopsVoice.Create(lParams);
    mSweetenerVoice.Create(lParams);
    mCarStarting.Create(lParams);
}

// ---------------------------------------------------------------------------
// SweetenersEffect::Attach @ 0x826FD7A0 (runs on this+4, the EffectBase subobject: r31)
//
// Re-verified against the ARTIST words 2026-09-24 (crash parity FOLLOWUPS 12, FX-XLANE):
//   0x826FD7C4..DC  the inlined EffectBase::Attach (++mu16AttachCount, meDetachState = 0)
//   0x826FD7E8..810 miRaceCarIndex = GetAttachInfo().muVehicleIndex
//   0x826FD814..81C mfDelayToBang: previous = current, current = -1.0 (flt_820037C8) -- an
//                   Update, not a Flush; 0x826FD82C/30 mfDeleayToVFXFire = (-1.0, -1.0) -- a Flush
//   0x826FD820..834 mfDamagePopVolume / mfDamageBangVolume / mfTimeIntoStart = 0.0,
//                   meCarStartingState = NONE.  NO store to meRaceCarEngineState (+0x1C0/+0x1C4).
//   0x826FD838      mFadeOutEngine.Initialize(1.0, 1.0, 0.0, linear)
//   0x826FD8B0..8C4 module RNG (module+0x13590) draw 1: hi32(seed) kept as the HIGH word, seed
//                   = seed * 0x5851F42D4C957F2D + 1 (mulld 0x826FD8B4)
//   0x826FD8D0..8E4 draw 2 (mulld 0x826FD8D4): its hi32 is the LOW word
//   0x826FD8E8      SqaureWave::Construct(this+0x16C, seed, r5 = up, r6 = down), where
//                   up   = (flt_82001CC0 0.0, flt_82002138 0.01)   (var_50)
//                   down = (flt_82F2CD50 0.025, flt_82F2CD4C 0.15)  (var_60; initialised .data,
//                   no dyn-init writer) -- the callee stores r5 -> mUpTimeWindow (+0, 0x826AB840)
//                   and r6 -> mDownTimeWindow (+8, 0x826AB848).
//   0x826FD8EC..8F8 mpFXBank = the global state manager + 0xB0 (the "mpFXBank" assert)
// and nothing else before the attribute lookup: the module RNG is stepped exactly TWICE and the
// effect's own mRandomGenerator is NOT reseeded here. The PC used to draw a THIRD value into
// mRandomGenerator.SetSeed (shifting every later module-RNG draw and giving each instance its own
// pop / sample sequence), passed the square-wave windows as up (0.15, 0.15) / down (0.0, 0.01),
// flushed meRaceCarEngineState to OFF and flushed mfDelayToBang; and it composed the seed as
// (A() << 32) | B(), whose two calls C++ may evaluate in either order.
// ---------------------------------------------------------------------------
bool SweetenersEffect::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;

    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    if (!mpPhysicsControl)
        return false;

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    CGS_ASSERT(lpModule != nullptr, "mpLogicModule");
    if (!lpModule)
        return false;

    miRaceCarIndex = static_cast<s8>(mpPhysicsControl->GetAttachInfo().muVehicleIndex);
    mfDelayToBang.Update(-1.0f);
    mfDamagePopVolume = 0.0f;
    mfDamageBangVolume = 0.0f;
    mfTimeIntoStart = 0.0f;
    mfDeleayToVFXFire.Flush(-1.0f);
    meCarStartingState = E_CARSTARTINGSTATE_NONE;
    mFadeOutEngine.Initialize(1.0f, 1.0f, 0.0f, CgsSound::Utils::Curve::E_LINEAR);

    CgsNumeric::Random& lrModuleRandom = lpModule->GetRandomGenerator();
    const u64 luSeedHighWord = lrModuleRandom.RandomUInt();   // mulld 0x826FD8B4
    const u64 luSeedLowWord  = lrModuleRandom.RandomUInt();   // mulld 0x826FD8D4
    mPopsSquareWave.Construct((luSeedHighWord << 32) | luSeedLowWord,
        CgsSound::Utils::MinMax(0.0f, 0.01f),       // up:   flt_82001CC0, flt_82002138
        CgsSound::Utils::MinMax(0.025f, 0.15f));    // down: flt_82F2CD50, flt_82F2CD4C

    // [sweeteners-attach] NOT X360 -- opt-in dispatch witness on the sound family's latch
    // (BRN_HUD_SOUND_DIAG, capped): the seed the two module-RNG draws built and the windows the
    // square wave now holds.
    if (BrnSound::Logic::HudSoundDiagBudget(guSweetenersDiagAttach))
    {
        BrnSound::Logic::HudSoundDiagPrintf(
            "[sweeteners-attach] car %d seed %08x%08x up (%.3f, %.3f) down (%.3f, %.3f) [FLAG PC witness]\n",
            static_cast<s32>(miRaceCarIndex), static_cast<u32>(luSeedHighWord), static_cast<u32>(luSeedLowWord),
            static_cast<double>(mPopsSquareWave.mUpTimeWindow.mfMin), static_cast<double>(mPopsSquareWave.mUpTimeWindow.mfMax),
            static_cast<double>(mPopsSquareWave.mDownTimeWindow.mfMin), static_cast<double>(mPopsSquareWave.mDownTimeWindow.mfMax));
    }

    BrnSound::Logic::GlobalStateManager* lpGlobal =
        static_cast<BrnSound::Logic::GlobalStateManager*>(
            lpModule->GetEnvironment().GetStateManager(0));
    mpFXBank = lpGlobal ? &lpGlobal->GetFxSpliceBank() : nullptr;
    CGS_ASSERT(mpFXBank != nullptr, "mpFXBank");

    const Attrib::Gen::vehicleengine& lrEngineAttributes =
        mpPhysicsControl->GetVehicleEngineAttributes();
    mbEnableSweetners = lrEngineAttributes.SweetenersAsset() != 0;
    if (!mbEnableSweetners)
    {
        SetMixerInputValue(1, 0x7FFF);
        return true;
    }

    switch (GetUpdateState())
    {
    case E_SWEETENER_STATE_NONE:
    {
        UpdateSweetenerInfo(lrEngineAttributes);
        const char* lpcBankName = lrEngineAttributes.SweetenersAssetName();
        CGS_ASSERT(lpcBankName != nullptr, "lpcBankName");
        if (!lpcBankName)
            return false;
        mSweetenersBank.Construct(
            lpModule,
            static_cast<u32>(CgsSound::Playback::Name::MakeHash(
                "~SplicerFactory::SK_NAME~")),
            static_cast<u32>(CgsSound::Playback::Name::MakeHash(lpcBankName)));
        mfTimeRemainingToPlaySputters = 0.0f;
        mi16SweetenerSlot = 0;
        SetCurrentBankElement(E_SWEETENER_BANK_ELEMENT_GEAR_UP);
        SetUpdateState(E_SWEETENER_STATE_WAIT_CONTENT);
        // Continue into the loaded-content test in this attach call.
    }
    case E_SWEETENER_STATE_WAIT_CONTENT:
        if (!mSweetenersBank.IsLoaded())
            return false;
        CreateVoices();
        SetUpdateState(E_SWEETENER_STATE_UPDATING);
        return true;
    case E_SWEETENER_STATE_UPDATING:
        return true;
    default:
        CGS_ASSERT(false, "Invalid state");
        return true;
    }
}

bool SweetenersEffect::SelectSampleIndex(eSweetenerBankElement aeElement,
                                         s32& ariSampleIndex)
{
    CGS_ASSERT(aeElement < E_SWEETENER_BANK_ELEMENT_COUNT,
               "E_SWEETENER_BANK_ELEMENT_COUNT > leElement");
    if (aeElement < 0 || aeElement >= E_SWEETENER_BANK_ELEMENT_COUNT)
        return false;

    const SweetenerInfo& lrInfo = maSweetenerInfo[aeElement];
    if (lrInfo.mi16FirstIndex == -1)
        return false;

    CGS_ASSERT(lrInfo.mi16LastIndex >= lrInfo.mi16FirstIndex,
               "liMax >= liMin");
    const u32 luCount = static_cast<u32>(
        lrInfo.mi16LastIndex - lrInfo.mi16FirstIndex + 1);
    CGS_ASSERT(luCount > 0, "luMod > 0");
    if (!luCount)
        return false;

    ariSampleIndex = lrInfo.mi16FirstIndex +
        static_cast<s32>(mRandomGenerator.RandomUInt() % luCount);
    return true;
}

void SweetenersEffect::UpdateSweetenerInfo(
    const Attrib::Gen::vehicleengine& arEngineAttributes)
{
    const Vector4 lCounts0 = arEngineAttributes.SweetenerCounts0();
    const Vector4 lVolumes0 = arEngineAttributes.SweetenerVolumes0();
    const Vector4 lCounts1 = arEngineAttributes.SweetenerCounts1();
    const Vector4 lVolumes1 = arEngineAttributes.SweetenerVolumes1();

    s16 liTotalSweeteners = 0;
    for (s32 liIndex = 0; liIndex < E_SWEETENER_BANK_ELEMENT_COUNT; ++liIndex)
    {
        const s32 liLane = liIndex & 3;
        const Vector4& lrCounts = liIndex < 4 ? lCounts0 : lCounts1;
        const Vector4& lrVolumes = liIndex < 4 ? lVolumes0 : lVolumes1;
        const s16 liCount = static_cast<s16>(VectorElement(lrCounts, liLane));
        SweetenerInfo& lrInfo = maSweetenerInfo[liIndex];
        if (liCount <= 0)
        {
            lrInfo.mi16FirstIndex = -1;
            lrInfo.mi16LastIndex = -1;
            lrInfo.mfVolume = 0.0f;
        }
        else
        {
            lrInfo.mi16FirstIndex = liTotalSweeteners;
            liTotalSweeteners = static_cast<s16>(liTotalSweeteners + liCount);
            lrInfo.mi16LastIndex = static_cast<s16>(liTotalSweeteners - 1);
            lrInfo.mfVolume = VectorElement(lrVolumes, liLane);
        }
    }

    FxVolumes lVolumes;
    lVolumes.mfWhineVolume = maSweetenerInfo[E_SWEETENER_BANK_ELEMENT_WHINE].mfVolume;
    lVolumes.mfTurboVolume = maSweetenerInfo[E_SWEETENER_BANK_ELEMENT_TURBO].mfVolume;
    if (std::fabs(lVolumes.mfWhineVolume) <= 0.00000011920929f)
        lVolumes.mfWhineVolume = 1.0f;
    if (std::fabs(lVolumes.mfTurboVolume) <= 0.00000011920929f)
        lVolumes.mfTurboVolume = 1.0f;

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    if (!lpModule)
        return;

    CgsSound::Io::Message<FxVolumes> lMessage(lVolumes);
    lMessage.Construct(E_SOUNDMESSAGE_FX_VOLUMES, 1,
        static_cast<u16>(GetInstanceId()), 12,
        CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT);
    lpModule->PostMessage(lMessage);
    lMessage.Construct(E_SOUNDMESSAGE_FX_VOLUMES, 1,
        static_cast<u16>(GetInstanceId()), 11,
        CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT);
    lpModule->PostMessage(lMessage);
}

f32 SweetenersEffect::UpdatePopingDuration(f32 afTimeStep)
{
    (void)afTimeStep;
    if (!mpPhysicsControl || mpPhysicsControl->GetPhysicsData().mSpeedMPH.GetCurrent() < 5.0f)
        return -1.0f;

    if (mpShiftingControl)
    {
        const ShiftControl::EShiftStage leState = mpShiftingControl->GetShiftingState();
        if (leState == ShiftControl::E_SHFT_UP_DISENGAGE ||
            leState == ShiftControl::E_SHFT_DOWN_ENGAGING_RISE)
            return 0.15f;
    }

    const CgsSound::Utils::DataPoint<bool>& lrAccelerating =
        mpPhysicsControl->GetPhysicsData().mIsAccelerating;
    if (lrAccelerating.GetCurrent() || !lrAccelerating.GetPrevious())
        return -1.0f;
    return 0.15f;
}

void SweetenersEffect::EmitPop(f32 afIntensity)
{
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    if (!lpModule)
        return;

    BrnSound::Module::Io::PopEffectsMessage lMessage;
    lMessage.Construct(static_cast<u8>(miRaceCarIndex), afIntensity);
    lpModule->GetPreUpdateOutput().GetAudioEffectsMessageQueue().AddEvent(
        static_cast<const CgsModule::Event*>(&lMessage),
        lMessage.GetEventType(), static_cast<s32>(sizeof(lMessage)));
}

void SweetenersEffect::UpdateCarStart(f32 afTimeStep)
{
    mfTimeIntoStart -= afTimeStep;
    mFadeOutEngine.Update(afTimeStep);
    const BrnWorld::RaceCarEntityModuleIO::EActiveRaceCarEngineState leEngineState =
        meRaceCarEngineState.GetPrevious();

    switch (meCarStartingState)
    {
    case E_CARSTARTINGSTATE_NONE:
        if (leEngineState !=
            BrnWorld::RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_STARTING)
            return;
        meCarStartingState = E_CARSTARTINGSTATE_BEGIN_STARTING;
        // fall through
    case E_CARSTARTINGSTATE_BEGIN_STARTING:
    {
        meCarStartingState = E_CARSTARTINGSTATE_BEGIN_STARTING;
        mFadeOutEngine.Initialize(1.0f, 1.0f, 0.0f,
            CgsSound::Utils::Curve::E_LINEAR);
        s32 liSampleIndex = 0;
        if (SelectSampleIndex(E_SWEETENER_BANK_ELEMENT_STARTING, liSampleIndex))
        {
            const CgsSound::Logic::VoiceWrapper::CreateParams lParams =
                mCarStarting.GetCreateParams();
            mCarStarting.Release();
            mCarStarting.Create(lParams);
            mCarStarting.Play(static_cast<u32>(liSampleIndex));
            mfTimeIntoStart = 0.25f;
        }
        meCarStartingState = E_CARSTARTINGSTATE_STARTING;
        // fall through
    }
    case E_CARSTARTINGSTATE_STARTING:
        meCarStartingState = E_CARSTARTINGSTATE_STARTING;
        if (leEngineState ==
                BrnWorld::RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_OFF ||
            leEngineState ==
                BrnWorld::RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING)
        {
            meCarStartingState = E_CARSTARTINGSTATE_QUITTING;
            if (mfTimeIntoStart >= 0.0f)
            {
                mFadeOutEngine.Initialize(1.0f, 1.0f, 400.0f,
                    CgsSound::Utils::Curve::E_LINEAR);
                mFadeOutEngine.AddLinkedStage(0.0f, 250.0f,
                    CgsSound::Utils::Curve::E_LINEAR);
            }
            else
            {
                mFadeOutEngine.Initialize(1.0f, 0.0f, 250.0f,
                    CgsSound::Utils::Curve::E_LINEAR);
            }
        }
        else if (leEngineState ==
                     BrnWorld::RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_STARTING &&
                 !mCarStarting.IsAlive())
        {
            meCarStartingState = E_CARSTARTINGSTATE_BEGIN_STARTING;
        }
        return;
    case E_CARSTARTINGSTATE_QUITTING:
        meCarStartingState = E_CARSTARTINGSTATE_QUITTING;
        if (leEngineState ==
            BrnWorld::RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_STARTING)
        {
            meCarStartingState = E_CARSTARTINGSTATE_STARTING;
            mFadeOutEngine.Initialize(mFadeOutEngine.mfCurrentValue, 1.0f, 70.0f,
                CgsSound::Utils::Curve::E_LINEAR);
            return;
        }
        if (!mFadeOutEngine.mbComplete)
            return;
        mCarStarting.Release();
        meCarStartingState = E_CARSTARTINGSTATE_RESTARTING;
        // fall through
    case E_CARSTARTINGSTATE_RESTARTING:
        meCarStartingState = E_CARSTARTINGSTATE_RESTARTING;
        mFadeOutEngine.Initialize(1.0f, 1.0f, 0.0f,
            CgsSound::Utils::Curve::E_LINEAR);
        meCarStartingState = E_CARSTARTINGSTATE_NONE;
        return;
    default:
        return;
    }
}

void SweetenersEffect::Notify(const CgsSound::Io::MessageHeader* apMessage)
{
    CGS_ASSERT(apMessage != nullptr, "lpMessageHeader");
    if (!apMessage || apMessage->GetEventId() != E_SOUND_MESSAGE_PLAY_BANG)
        return;

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    if (!lpModule)
        return;

    mfDelayToBang.Update(lpModule->GetRandomGenerator().RandomFloat(0.0f, 0.1f));
    if (mDamagePopVoice.IsAlive() || !mpFXBank)
        return;

    BrnSound::Logic::BrnEffectObject::SampleTag lTag;
    lTag.Construct();
    if (!GetSampleTag(2, 5, guDamagePopRoundRobin++, lTag))
        return;

    const CgsSound::Logic::VoiceWrapper::CreateParams lParams =
        MakeSplicerParams(mpFXBank);
    mDamagePopVoice.Create(lParams);
    mDamagePopVoice.Play(static_cast<u32>(static_cast<u16>(lTag.miSampleIndex)));
    mfDamagePopVolume = lTag.mfVolume;
}

void SweetenersEffect::UpdateDamageBangs(f32 afTimeStep)
{
    mfDelayToBang.Update(mfDelayToBang.GetCurrent() - afTimeStep);
    mfDeleayToVFXFire.Update(mfDeleayToVFXFire.GetCurrent() - afTimeStep);

    if (mfDelayToBang.GetCurrent() < 0.0f &&
        mfDelayToBang.GetPrevious() > 0.0f && mpFXBank)
    {
        BrnSound::Logic::BrnEffectObject::SampleTag lTag;
        lTag.Construct();
        if (GetSampleTag(2, 4, guDamageBangRoundRobin++, lTag))
        {
            if (mDamageBangVoice.IsAlive())
                mDamageBangVoice.Release();
            const CgsSound::Logic::VoiceWrapper::CreateParams lParams =
                MakeSplicerParams(mpFXBank);
            mDamageBangVoice.Create(lParams);
            mDamageBangVoice.Play(
                static_cast<u32>(static_cast<u16>(lTag.miSampleIndex)));
            mfDamageBangVolume = lTag.mfVolume;
            mfDeleayToVFXFire.Update(0.45f);
        }
    }

    if (mfDeleayToVFXFire.GetCurrent() < 0.0f &&
        mfDeleayToVFXFire.GetPrevious() > 0.0f)
    {
        EmitPop(1.0f);
    }
}

void SweetenersEffect::UpdateParams(f32 afTimeStep)
{
    SetMixerInputValue(0, 0);
    mFadeOutEngine.Update(afTimeStep);

    if (mbEnableSweetners)
    {
        mfTimeRemainingToPlaySputters -= afTimeStep;
        s32 liSampleIndex = 0;
        if (mfTimeRemainingToPlaySputters < 0.0f)
            mfTimeRemainingToPlaySputters = UpdatePopingDuration(afTimeStep);

        if (mfTimeRemainingToPlaySputters > 0.0f)
        {
            if (mExhaustPopsVoice.GetState() ==
                    CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_PLAYING ||
                !SelectSampleIndex(E_SWEETENER_BANK_ELEMENT_EXHAUST_POP,
                                   liSampleIndex))
            {
                if (mExhaustPopsVoice.GetState() ==
                    CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_PLAYING)
                {
                    const CgsSound::Utils::DataPoint<bool> lPop =
                        mPopsSquareWave.Update(afTimeStep);
                    if (lPop.GetCurrent() && !lPop.GetPrevious())
                        EmitPop(mRandomGenerator.RandomFloat(0.0f, 1.0f));
                }
            }
            else
            {
                EmitPop(mRandomGenerator.RandomFloat(0.0f, 1.0f));
                mExhaustPopsVoice.Play(static_cast<u32>(liSampleIndex));
                SetMixerInputValue(0, 0x7FFF);
            }
        }

        if (GetStateId() == 1)
        {
            BrnSound::Module::SoundLogicModule* lpModule =
                static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
            BrnSound::Module::Io::LogicInputBuffer* lpInput =
                lpModule ? lpModule->GetBrnInputStructure() : nullptr;
            if (lpInput)
            {
                const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
                    lpVehicles = lpInput->GetVehicleInterface();
                BrnWorld::RaceCarEntityModuleIO::EActiveRaceCarEngineState leState =
                    BrnWorld::RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_COUNT;
                if (lpVehicles->GetPlayerActiveRaceCarIndex() !=
                    E_ACTIVE_RACE_CAR_INDEX_INVALID)
                    leState = lpVehicles->GetPlayerEngineState();
                meRaceCarEngineState.Update(leState);
                SetMixerInputValue(1,
                    leState == BrnWorld::RaceCarEntityModuleIO::
                        E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING ? 0x7FFF : 0);
            }
        }

        if (mpShiftingControl && mpShiftingControl->IsActive() &&
            mpShiftingControl->GetShiftingState() != ShiftControl::E_SHFT_UP_LFO)
        {
            const ShiftControl::EShiftStage leChanged =
                mpShiftingControl->GetShiftingStateChange();
            if (leChanged == ShiftControl::E_SHFT_UP_DISENGAGE &&
                SelectSampleIndex(E_SWEETENER_BANK_ELEMENT_GEAR_UP, liSampleIndex))
            {
                mSweetenerVoice.Play(static_cast<u32>(liSampleIndex));
                mi16SweetenerSlot = 0;
                SetCurrentBankElement(E_SWEETENER_BANK_ELEMENT_GEAR_UP);
            }
            else if (leChanged == ShiftControl::E_SHFT_DOWN_DISENGAGE &&
                     SelectSampleIndex(E_SWEETENER_BANK_ELEMENT_GEAR_DOWN,
                                       liSampleIndex))
            {
                mSweetenerVoice.Play(static_cast<u32>(liSampleIndex));
                mi16SweetenerSlot = 1;
                SetCurrentBankElement(E_SWEETENER_BANK_ELEMENT_GEAR_DOWN);
            }
        }

        UpdateCarStart(afTimeStep);
    }

    UpdateDamageBangs(afTimeStep);
}

void SweetenersEffect::ProcessUpdate()
{
    if (mpPhysicsControl)
        UpdateSweetenerInfo(mpPhysicsControl->GetVehicleEngineAttributes());

    mExhaustPopsVoice.Update();
    mSweetenerVoice.Update();
    mDamageBangVoice.Update();
    mDamagePopVoice.Update();
    mCarStarting.Update();

    const u32 luPitchName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Pitch~"));
    const u32 luSendName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("Send01"));
    const u32 luReverbName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("ReverbSend"));
    const f32 lfReverbGain = GetRWACMixerOutputValue(9, 0);
    const f32 lfPitch = GetRWACMixerOutputValue(6, 1);

    const auto lUpdateVoice = [&](CgsSound::Logic::VoiceWrapper& arVoice,
                                  f32 afGain)
    {
        if (!arVoice.IsAlive())
            return;
        arVoice.SetParameter(0, lfPitch, &luPitchName);
        arVoice.SetGain(1, lfReverbGain, &luReverbName);
        arVoice.SetGain(0, afGain, &luSendName);
    };

    lUpdateVoice(mExhaustPopsVoice,
        maSweetenerInfo[E_SWEETENER_BANK_ELEMENT_EXHAUST_POP].mfVolume *
        GetRWACMixerOutputValue(4, 0) * 4.0f);

    CGS_ASSERT(GetCurrentBankElement() < E_SWEETENER_BANK_ELEMENT_COUNT,
               "E_SWEETENER_BANK_ELEMENT_COUNT > GetCurrentBankElement()");
    if (GetCurrentBankElement() < E_SWEETENER_BANK_ELEMENT_COUNT)
    {
        lUpdateVoice(mSweetenerVoice,
            maSweetenerInfo[GetCurrentBankElement()].mfVolume *
            GetRWACMixerOutputValue(mi16SweetenerSlot, 0));
    }

    lUpdateVoice(mCarStarting,
        mFadeOutEngine.mfCurrentValue * GetRWACMixerOutputValue(5, 0) *
        maSweetenerInfo[E_SWEETENER_BANK_ELEMENT_STARTING].mfVolume);
    lUpdateVoice(mDamageBangVoice,
        mfDamageBangVolume * GetRWACMixerOutputValue(4, 0));
    lUpdateVoice(mDamagePopVoice,
        mfDamagePopVolume * GetRWACMixerOutputValue(4, 0));
}

bool SweetenersEffect::Detach()
{
    mExhaustPopsVoice.Release();
    mSweetenerVoice.Release();
    mCarStarting.Release();
    mDamageBangVoice.Release();
    mDamagePopVoice.Release();

    if (!BrnSound::Logic::BrnEffectObject::Detach())
        return false;
    if (mSweetenersBank.IsCreated())
        mSweetenersBank.Destruct();
    SetUpdateState(E_SWEETENER_STATE_NONE);
    return true;
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
