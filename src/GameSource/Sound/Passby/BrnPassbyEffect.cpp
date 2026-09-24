#include "GameSource/Sound/Passby/BrnPassbyEffect.h"
#include "GameSource/Sound/Passby/BrnPassbyState.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // [DIAG] sink
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"             // CgsSound::Utils::Slope (UpdateParams)
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"        // CgsSound::Playback::Name::MakeHash
#include "GameSource/Sound/Global/BrnGlobalStateManager.h"          // the pass-by submix voice (Prepare)
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"                  // Nicotine::DMixIO (ProcessUpdate)

#include <cmath>
#include <cstdio>
#include <cstdlib>

// =============================================================================
// BrnSound::Logic::Passby::Passby3DControl / PassbyEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnPassbyEffect.h for the object
// layouts and the RTTI descriptors.
//
//   PassbyEffect()                  @ 0x826AFDC8
//   ~PassbyEffect()                 @ 0x826BF6D8  (scalar deleting destructor @ 0x826C9200)
//   CreateObject                    @ 0x826BF678
//   GetTypeInfo / GetTypeName       @ 0x82689028 / 0x82689038
//   GetController                   @ 0x82685D38  (identical-code-folded with MusicEffect's)
//   AttachController                @ 0x82689048
//   Prepare                         @ 0x826F9860
//   Attach                          @ 0x826D5280
//   ChooseSampleId                  @ 0x82689108
//   UpdateParams                    @ 0x826D5068
//   ProcessUpdate                   @ 0x826F99C0
//   Detach                          @ 0x826F9F10
//   UpdatePosition                  @ 0x826BF778
//   GetRelativeVelocityMag          @ 0x826BF828
//   Passby3DControl::CreateObject   @ 0x826E8E18
//   Passby3DControl GetTypeInfo / GetTypeName  @ 0x82689008 / 0x82689018
//   ~Passby3DControl                @ 0x826E8ED0
//
// NOT REPRODUCED: the four dev-menu switches the console registers with its debug-variable
// registry (sub_8282D640 at 0x826D6D40..0x826D6DF0, all zero in the image and written by
// nothing else) -- KI_SPEW_PASSBY_INFO (dword_82FFB938, the "Culled with a velocity of" and
// "Detach." TTY lines), KI_SPEW_PASSBY_INFO_VERBOSE (dword_82FFB934, the first-update
// SampleID / Boost / Vol / Pitch line), KB_DRAW_PASSBY_INFO (byte_82FFB93C, the
// Message<Brn3DEffectControl::DrawSphere> debug sphere) and the manager's
// KI_SPEW_PASSBY_STATE_INFO. Their only side effect besides the text is ProcessUpdate's
// `mbFirstUpdate = false` inside the verbose-spew arm (0x826F9BF4), so on the console
// mbFirstUpdate is written by Attach and read by nothing that runs.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Passby
{

namespace
{
// BrnPassbyEffect.cpp:30 / :31 (DWARF `extern float32_t`): the relative-speed range the pitch
// and volume slopes map onto their curves. KF_MIN_CAR_SPEED is the .bss word 0x82FFB940 (read
// at 0x826D51C4 and written by nothing -- 0.0); KF_MAX_CAR_SPEED is the .data word 0x82F2CF08
// (0x42A00000, read at 0x826D51D0 only).
const f32 KF_MIN_CAR_SPEED = 0.0f;
const f32 KF_MAX_CAR_SPEED = 80.0f;

// BrnPassbyEffect.cpp:33 (DWARF `extern int32_t[19]`): the ducking level Attach sends on mixer
// input 1, per ePassbyTypes -- the .data words at 0x82F2CF20..0x82F2CF68.
const s32 KAI_PASSBY_DUCKING_ARRAY[AttribSys::Enums::ePassbyTypes::MaxPassbyTypes] =
{
    0,      0,      0,          // PassbyAzimuth, PassbyPitch, PassbyCutoff
    15000,  15000,  15000,      // TrafficSmall, TrafficMedium, TrafficLarge
    8000,   8000,               // LampPost, Tree
    0x7FFF, 0x7FFF,             // Bridge, Tunnel
    8000,   8000,               // Camera, Misc
    0x7FFF, 0x7FFF, 0x7FFF,     // Collision, Overpass, Warehouse
    20000,  8000,   20000,      // Alley, StaticMetal, LargeOverheadObject
    0,                          // PassbyBoostOffset
};

// The mixer-output scales ProcessUpdate applies inline (0x826F9A7C / 0x826F9ABC): the azimuth
// in degrees (flt_820AA8EC, 0x3BB400B4) and the Q12 pitch multiplier (flt_820AA8F4, 0x39800000).
const f32 KF_MIXER_AZIMUTH_TO_DEGREES = 0.0054932479f;
const f32 KF_MIXER_Q12_TO_PITCH = 0.000244140625f;

// ProcessUpdate's collision-pass-by pitch bend (0x826F9B08..0x826F9B40): the record's
// relative speed clamped to flt_820047C4 (15.0), times flt_820AA540 (0x3D888889, 1/15), then
// `fmadds` with flt_820138AC (0x3ECCCCCE -- one ulp above 0.4f) and flt_820054C8 (0.8).
const f32 KF_COLLISION_SPEED_MAX = 15.0f;
const f32 KF_COLLISION_SPEED_SCALE = 0.06666667f;
const f32 KF_COLLISION_PITCH_RANGE = 0.40000003576278687f;
const f32 KF_COLLISION_PITCH_MIN = 0.8f;

// [DIAG] NOT IN THE X360 BINARY -- BRN_PASSBY_SOUND_DIAG (any value but "0"), the same switch
// the manager's prepare witnesses use. Every line is capped by its caller.
bool PassbyEffectDiag()
{
    static int siEnabled = -1;
    if (siEnabled < 0)
    {
        const char* lpcEnv = std::getenv("BRN_PASSBY_SOUND_DIAG");
        siEnabled = (lpcEnv && lpcEnv[0] && lpcEnv[0] != '0') ? 1 : 0;
    }
    return siEnabled != 0 && CgsDev::Log::gpDebugPrint != 0;
}
} // namespace

u32 PassbyEffect::smuSampleIndex = 0;

// ---------------------------------------------------------------------------
// ~Passby3DControl  @ 0x826E8ED0  (the X360 `scalar deleting destructor')
//
//   bl   Attrib::Instance::~Instance(this + 176)  ; destroy mEngineDataAtrib
//   li   r7, 3 ; stw r7, 0x24(this)               ; meDetachState = FINISHED
//   stw  &off_820AA820, 0(this)                   ; primary vptr settle
//   stb  0, 0x2D(this)                            ; control bookkeeping flag = false
//   stw  0, 0x20(this)                            ; mfDeltaTime = 0
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound allocator) }
//   return this
//
// The Attrib::Instance member teardown (mEngineDataAtrib) is produced by the
// inherited ~Brn3DEffectControl destructor chain, which destroys the member via
// the committed Attrib::Instance::~Instance. The remaining stores settle members
// owned by the inherited bases, so this leaf destructor body adds nothing of its
// own.
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to
// free the object; that allocator is not homed here, so operator-delete dispatch
// is left to the host toolchain (the `delete` half of the X360 scalar deleting
// destructor) rather than reproducing the raw allocator vtable call.
// ---------------------------------------------------------------------------
Passby3DControl::~Passby3DControl()
{
}

// ---------------------------------------------------------------------------
// Passby3DControl::CreateObject(u32)  @ 0x826E8E18
//   MemBase::operator new(0xD0, "Passby3DControl", a1 != 0) ; Brn3DEffectControl() ;
//   vptr = off_820B451C. The host `new` stands in for the sound allocator (the
//   Collision3DControl::CreateObject treatment).
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectControl* Passby3DControl::CreateObject(u32 /*auAllocator*/)
{
    return new Passby3DControl();
}

// sTypeInfo @0x82F2F97C: {0x40000, "Passby3DControl", &Brn3DEffectControl::sTypeInfo, CreateObject}.
// FLAG: Brn3DEffectControl's own descriptor (0x82F2E81C) is not homed, so the chain starts at
// EffectControl's -- the Collision3DControl precedent. CreateEffectFromRegistry only walks the
// base chain to break a tie between two descriptors of the same state and effect id, and
// 0x40000 has no rival.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* Passby3DControl::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> sTypeInfo(
        0x40000, "Passby3DControl",
        CgsSound::Logic::EffectControl::GetStaticTypeInfo(),
        &Passby3DControl::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* Passby3DControl::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* Passby3DControl::GetTypeName() const
{
    return "Passby3DControl";
}

// The CRT registration at 0x82C633A4.
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const gpPassby3DControlReg =
    CgsSound::Logic::EffectControl::AddToClassTypeInfoArray(Passby3DControl::GetStaticTypeInfo());

// ---------------------------------------------------------------------------
// PassbyEffect::PassbyEffect  @ 0x826AFDC8
//
//   <BrnEffectObject base: both vptrs, the EffectBase zero-inits>
//   stw &off_820B0E20, 0x38 ; stw 0, 0x3C ; stw 0, 0x40     ; mVoice
//   bl  passbybin::passbybin(this + 0x44, 0, 0)            ; mAttribs
//   stw 0, 0x78                                            ; mePrepareState = CONSTRUCT_VOICE
//
// The console leaves every other leaf member to Attach / AttachController / UpdateParams; here
// they are value-defined (zero) so nothing is ever read uninitialised on the host.
// ---------------------------------------------------------------------------
PassbyEffect::PassbyEffect()
    : mVoice()
    , mAttribs(nullptr, nullptr)
    , meLifetime(E_NOT_PLAYED)
    , mpPassby3DControl(nullptr)
    , mePrepareState(E_PREPARE_STATE_CONSTRUCT_VOICE)
    , muSampleId(0)
    , mfPitchScale(0.0f)
    , mfVolumeScale(0.0f)
    , mbPlayerIsBoosting(false)
    , mbFirstUpdate(false)
{
    mPosition.SetZero();
}

// ---------------------------------------------------------------------------
// ~PassbyEffect  @ 0x826BF6D8 (the leaf body), reached from the scalar deleting destructor
// @ 0x826C9200. The console body destroys mAttribs (Attrib::Instance::~Instance on +0x44) and
// mVoice (its handle release on +0x3C), then settles the BrnEffectObject base -- all of it the
// member / base destructors the host compiler emits from this empty body. The (a2 & 1) tail
// frees through the sound allocator (off_82FFB954), left to the host operator delete.
// ---------------------------------------------------------------------------
PassbyEffect::~PassbyEffect()
{
}

// ---------------------------------------------------------------------------
// PassbyEffect::CreateObject(u32)  @ 0x826BF678
//   MemBase::operator new(0x90, "PassbyEffect", a1 != 0) ; PassbyEffect() ; return this + 4
//   (the EffectBase sub-object -- the host pointer conversion does the same adjustment).
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectObject* PassbyEffect::CreateObject(u32 /*auAllocator*/)
{
    return new PassbyEffect();
}

// sTypeInfo @0x82F2F98C: {0x40000, "PassbyEffect", &BrnEffectObject::sTypeInfo, CreateObject}.
// FLAG: BrnEffectObject's descriptor (0x82F2E7FC) is not homed, so the chain starts at
// EffectObject's -- the CollisionEffect precedent (see Passby3DControl above).
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* PassbyEffect::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo(
        0x40000, "PassbyEffect",
        CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
        &PassbyEffect::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* PassbyEffect::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* PassbyEffect::GetTypeName() const
{
    return "PassbyEffect";
}

// The CRT registration at 0x82C633B4. Without it StateManager::CreateEffectObject finds no
// ObjectID 0x40000 and every PassbyState's CreateSFXObjs fires "Failed to find Effect Object".
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpPassbyEffectReg =
    CgsSound::Logic::EffectObject::AddToClassTypeInfoArray(PassbyEffect::GetStaticTypeInfo());

// ---------------------------------------------------------------------------
// GetController  @ 0x82685D38: `subfic r11, r4, 0 ; subfe r3, r11, r11` -- controller 0 is
// effect 0 (the Passby3DControl), any other index -1.
// ---------------------------------------------------------------------------
s32 PassbyEffect::GetController(s32 aiIndex)
{
    return aiIndex == 0 ? 0 : -1;
}

// ---------------------------------------------------------------------------
// AttachController  @ 0x82689048  (DWARF cpp:135)
//   (ctrl->miObjectId & 0x7F0) != 0 -> the "Cound't attach controller " << id assert (cpp:146)
//   else mpPassby3DControl = ctrl                      ; stw r28, 0x70(r3)
// There is no null test on the console (it reads ctrl+0x14 first).
// ---------------------------------------------------------------------------
void PassbyEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    if (apController->GetEffectID() != 0)
    {
        CGS_ASSERT(false, "Cound't attach controller ");
        return;
    }
    mpPassby3DControl = static_cast<Passby3DControl*>(apController);
}

// ---------------------------------------------------------------------------
// Prepare  @ 0x826F9860  (DWARF cpp:165)
//
//   mePrepareState < 1 (CONSTRUCT_VOICE):
//     mePrepareState = CONSTRUCT_VOICE                       ; stw 0, 0x74 (0x826F9898)
//     EffectBase::Prepare(apState) inlined                   ; mpState, mpLogicModule
//     mVoice.Construct(module, module->GetUniqueId() [vt +0x48],
//                      dword_83008404 (MakeHash("~SplicerFactory::SK_NAME~"), CRT 0x82C65940),
//                      MakeHash("SplicerVoiceSpec"))
//     -- falls into
//   mePrepareState == 1 (CONNECT_VOICE):
//     mePrepareState = CONNECT_VOICE ; if (!mVoice.IsReady()) return false
//     lpPassbyState = mpState            ; assert "lpPassbyState" (cpp:190)
//     lpStateManager = its manager (+0x24) ; assert "lpStateManager" (cpp:193)
//     mVoice.Attach(MakeHash("~SplicerPlayerVoice::Slot~"), lpStateManager->mSplicerBank (+0x224))
//     mVoice.Connect(dword_8300A6E0 (MakeHash("Send01"), CRT 0x82C61A20),
//                    module+0x2954 (Environment slot 0, the GlobalStateManager)
//                        +0xA4 (maSubmixVoices[E_SUBMIX_VOICE_PASSBY]) .GetIdent())
//     return true
//   anything else: return true
// ---------------------------------------------------------------------------
bool PassbyEffect::Prepare(CgsSound::Logic::State* apState)
{
    switch (mePrepareState)
    {
    case E_PREPARE_STATE_CONSTRUCT_VOICE:
        mePrepareState = E_PREPARE_STATE_CONSTRUCT_VOICE;
        CgsSound::Logic::EffectBase::Prepare(apState);
        mVoice.Construct(
            GetLogicModule(), GetLogicModule()->GetUniqueId(),
            static_cast<u32>(CgsSound::Playback::Name::MakeHash("~SplicerFactory::SK_NAME~")),
            static_cast<u32>(CgsSound::Playback::Name::MakeHash("SplicerVoiceSpec")));
        // fall through
    case E_PREPARE_STATE_CONNECT_VOICE:
    {
        mePrepareState = E_PREPARE_STATE_CONNECT_VOICE;
        if (!mVoice.IsReady())
            return false;

        PassbyState* lpPassbyState = static_cast<PassbyState*>(GetStateBase());
        CGS_ASSERT(lpPassbyState != nullptr, "lpPassbyState");
        PassbyStateManager* lpStateManager =
            static_cast<PassbyStateManager*>(lpPassbyState->GetStateManager());
        CGS_ASSERT(lpStateManager != nullptr, "lpStateManager");

        mVoice.Attach(static_cast<s32>(CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Slot~")),
                      lpStateManager->GetSplicerBank());
        BrnSound::Logic::GlobalStateManager* lpGlobalStateManager =
            static_cast<BrnSound::Logic::GlobalStateManager*>(
                GetLogicModule()->GetEnvironment().GetStateManager(0));
        mVoice.Connect(
            static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01")),
            static_cast<u32>(lpGlobalStateManager
                                 ->GetSubmixVoice(BrnSound::Logic::GlobalStateManager::E_SUBMIX_VOICE_PASSBY)
                                 .GetIdent()));
        return true;
    }
    default:
        return true;
    }
}

// ---------------------------------------------------------------------------
// GetPassbyData  (DWARF cpp:605; inlined into Attach @0x826D52A8, UpdatePosition @0x826BF790
// and ProcessUpdate @0x826F99F0): the owning PassbyState's record, asserting "lpState"
// (cpp:616, li r5, 0x268).
// ---------------------------------------------------------------------------
const PassbyStateManager::Passby& PassbyEffect::GetPassbyData() const
{
    const PassbyState* lpState = static_cast<const PassbyState*>(GetStateBase());
    CGS_ASSERT(lpState != nullptr, "lpState");
    return lpState->GetPassbyData();
}

// ---------------------------------------------------------------------------
// Attach  @ 0x826D5280  (DWARF cpp:432; runs on the EffectBase sub-object)
//
//   EffectBase::Attach() inlined                     ; meDetachState = 0, ++mu16AttachCount
//   lPassby = GetPassbyData()
//   assert 0 <= lPassby.meType < PassbyBoostOffset    ; cpp:441 (li r5, 0x1B9)
//   mAttribs.ChangeWithDefault(module->mBurnoutGlobalData (+0x13508).mPassbyBins(meType))
//   control = lPassby.mp3dControl ; control && its meAttachState == FINISHED
//       -> mu16AttachCount = control's mu16AttachCount   ; sth 0xE (0x826D5340..0x826D5344)
//   !UpdatePosition() -> meLifetime = E_CULLED ; return true
//   mpPassby3DControl->AttachEmitterPosition(&mPosition)   ; vt +0x34
//   mbPlayerIsBoosting = IsPlayerCarActive() ? the player's BoostOutputInfo.mbIsBoosting : 0
//   mbPlayerIsBoosting = lPassby.mbSuppressBoostBys ? 0 : mbPlayerIsBoosting
//   muSampleId = ChooseSampleId(lPassby, mbPlayerIsBoosting) ; mVoice.Play(muSampleId)
//   mVoice.SetGain(0, 0.0f, dword_8300866C = MakeHash("Send01"))   ; starts silent
//   meLifetime = E_NOT_PLAYED ; mfPitchScale = mfVolumeScale = 1.0f
//   SetMixerInputValue(0, 0x7FFF) ; SetMixerInputValue(1, KAI_PASSBY_DUCKING_ARRAY[meType])
//   mbFirstUpdate = true ; return true
// ---------------------------------------------------------------------------
bool PassbyEffect::Attach()
{
    CgsSound::Logic::EffectBase::Attach();
    const PassbyStateManager::Passby& lrPassby = GetPassbyData();
    CGS_ASSERT(lrPassby.meType >= 0 && lrPassby.meType < AttribSys::Enums::ePassbyTypes::PassbyBoostOffset,
               "lPassby.meType >= 0 && lPassby.meType < AttribSys::Enums::ePassbyTypes::PassbyBoostOffset");

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    mAttribs.ChangeWithDefault(*static_cast<const Attrib::RefSpec*>(
        lpModule->GetGlobalData().mPassbyBins(static_cast<u32>(lrPassby.meType))));

    const CgsSound::Logic::Cgs3dEffectControl* lpControl = lrPassby.mp3dControl;
    if (lpControl != nullptr &&
        lpControl->GetAttachState() == CgsSound::Logic::EffectBase::E_ATTACH_STATE_FINISHED)
    {
        mu16AttachCount = lpControl->GetAttachCount();
    }

    if (!UpdatePosition())
    {
        meLifetime = E_CULLED;
        return true;
    }
    mpPassby3DControl->AttachEmitterPosition(&mPosition);

    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpVehicles =
        lpModule->GetBrnInputStructure()->GetVehicleInterface();
    if (lpVehicles->IsPlayerCarActive())
        mbPlayerIsBoosting =
            lpVehicles->GetBoostOutputInfoN(lpVehicles->GetPlayerActiveRaceCarIndex())->mbIsBoosting;
    else
        mbPlayerIsBoosting = false;
    mbPlayerIsBoosting = lrPassby.mbSuppressBoostBys ? false : mbPlayerIsBoosting;

    muSampleId = ChooseSampleId(lrPassby, mbPlayerIsBoosting);
    mVoice.Play(static_cast<s32>(muSampleId));
    const u32 luSend01 = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));
    mVoice.SetGain(0, 0.0f, &luSend01);

    meLifetime = E_NOT_PLAYED;
    mfPitchScale = 1.0f;
    mfVolumeScale = 1.0f;
    SetMixerInputValue(0, 0x7FFF);
    SetMixerInputValue(1, KAI_PASSBY_DUCKING_ARRAY[lrPassby.meType]);
    mbFirstUpdate = true;

    // [DIAG] NOT IN THE X360 BINARY (BRN_PASSBY_SOUND_DIAG): what the voice was started with.
    if (PassbyEffectDiag())
    {
        static u32 suPrintCount = 0;
        if (suPrintCount++ < 64u)
        {
            char lacLine[224];
            std::snprintf(lacLine, sizeof(lacLine),
                          "[passby-sound] effect attached type=%d sample=%u boost=%d range=%d..%d/%d..%d "
                          "control=%d ducking=%d volMod=%g relVel=%g\n",
                          static_cast<s32>(lrPassby.meType), muSampleId, mbPlayerIsBoosting ? 1 : 0,
                          mAttribs.mFirstPassBy(), mAttribs.mLastPassBy(),
                          mAttribs.mFirstBoostPassBy(), mAttribs.mLastBoostPassBy(),
                          lpControl != nullptr ? 1 : 0, KAI_PASSBY_DUCKING_ARRAY[lrPassby.meType],
                          static_cast<double>(lrPassby.mfVolumeModifier),
                          static_cast<double>(lrPassby.mfRelativeVelocityMagnitude));
            *CgsDev::Log::gpDebugPrint << lacLine;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// ChooseSampleId  @ 0x82689108  (DWARF cpp:551)
//
//   layout = mAttribs (+0x48 on the primary)
//   boost:  first = mFirstBoostPassBy (+0x68), last = mLastBoostPassBy (+0x60)
//   normal: first = mFirstPassBy (+0x64),      last = mLastPassBy (+0x5C)
//   count = last - first + 1 (u32) ; first16 = first & 0xFFFF   ; clrlwi r8, r10, 16
//   count == 0 -> return first16
//   r = smuSampleIndex % count (divwu / mullw / subf) + first16 ; ++smuSampleIndex ; return r
// The record argument is unused.
// ---------------------------------------------------------------------------
u32 PassbyEffect::ChooseSampleId(const PassbyStateManager::Passby& /*arPassby*/, bool abPlayerIsBoosting)
{
    const u32 luFirst = static_cast<u32>(abPlayerIsBoosting ? mAttribs.mFirstBoostPassBy() : mAttribs.mFirstPassBy());
    const u32 luLast = static_cast<u32>(abPlayerIsBoosting ? mAttribs.mLastBoostPassBy() : mAttribs.mLastPassBy());
    const u32 luCount = luLast - luFirst + 1u;
    const u32 luFirst16 = static_cast<u16>(luFirst);
    if (luCount == 0)
        return luFirst16;
    const u32 luSample = smuSampleIndex % luCount + luFirst16;
    ++smuSampleIndex;
    return luSample;
}

// ---------------------------------------------------------------------------
// UpdatePosition  @ 0x826BF778  (DWARF cpp:624)
//
//   lPassby = GetPassbyData() ; control = lPassby.mp3dControl (+0x10)
//   no control           -> mPosition = lPassby.mStaticPos ; true
//   control->meAttachState (+0x20) != FINISHED, or control->mu16AttachCount (+0xE) !=
//   mu16AttachCount      -> false (the control has let go of the source, or holds another)
//   else                 -> mPosition = control's current emitter position (+0x60) ; true
// ---------------------------------------------------------------------------
bool PassbyEffect::UpdatePosition()
{
    const PassbyStateManager::Passby& lrPassby = GetPassbyData();
    const CgsSound::Logic::Cgs3dEffectControl* lpControl = lrPassby.mp3dControl;
    if (lpControl == nullptr)
    {
        mPosition = lrPassby.mStaticPos;
        return true;
    }
    if (lpControl->GetAttachState() != CgsSound::Logic::EffectBase::E_ATTACH_STATE_FINISHED)
        return false;
    if (lpControl->GetAttachCount() != mu16AttachCount)
        return false;
    mPosition = lpControl->GetEmitterPosition().GetCurrent();
    return true;
}

// ---------------------------------------------------------------------------
// GetRelativeVelocityMag  @ 0x826BF828  (DWARF cpp:660)
//
//   v = (mPosition - mpPassby3DControl's current emitter position (+0x60))
//       * vrefp(mfDeltaTime) [two Newton steps]
//       - module+0x2B80 (the E_MIC_PLAYER / E_PLAYER_1 microphone's mVelocity)
//   return |v| : vmsum3fp, vrsqrtefp + two Newton steps, `vcmpeqfp / vsel` -> 0 for a zero vector
// The control's emitter position is the one ITS UpdateParams pushed from &mPosition last frame
// (the controls update before the effects), so the first term is the source's own velocity.
// ---------------------------------------------------------------------------
f32 PassbyEffect::GetRelativeVelocityMag() const
{
    const CgsSound::Logic::MicrophoneSystem::Microphone* lpListener =
        GetLogicModule()->GetEnvironment().GetMicrophoneSystem().GetMicrophone(
            CgsSound::Logic::MicrophoneSystem::E_MIC_PLAYER, CgsSound::Logic::MicrophoneSystem::E_PLAYER_1);
    const Vector3& lrListenerVelocity = lpListener->GetVelocity();
    const Vector3 lControlPosition = mpPassby3DControl->GetEmitterPosition().GetCurrent();
    const f32 lfInverseTimeStep = 1.0f / mfDeltaTime;

    const f32 lfX = (mPosition.x - lControlPosition.x) * lfInverseTimeStep - lrListenerVelocity.x;
    const f32 lfY = (mPosition.y - lControlPosition.y) * lfInverseTimeStep - lrListenerVelocity.y;
    const f32 lfZ = (mPosition.z - lControlPosition.z) * lfInverseTimeStep - lrListenerVelocity.z;
    const f32 lfSquared = lfX * lfX + lfY * lfY + lfZ * lfZ;
    return lfSquared == 0.0f ? 0.0f : std::sqrt(lfSquared);
}

// ---------------------------------------------------------------------------
// UpdateParams  @ 0x826D5068  (DWARF cpp:224)
//
//   !UpdatePosition() -> return                              ; the source is gone: hold
//   v = GetRelativeVelocityMag()
//   v < mAttribs.VelocityThreshold_Min (+0x50) && meLifetime != E_CULLED && mVoice.IsPlaying()
//       -> mVoice.SetGain(0, 0.0f, dword_8300866C "Send01") ; meLifetime = E_CULLED
//          (`fcmpu ; bge` -- a NaN speed does not cull)
//   Slope(KF_MIN_CAR_SPEED, KF_MAX_CAR_SPEED, curve.x, curve.y).GetValue(v, E_ONE_MINUS_EQPWR)
//       mfPitchScale  <- PitchCurveBoost (+0x30) / PitchCurveNormal (+0x20) by mbPlayerIsBoosting
//       mfVolumeScale <- VolumeBoost (+0x10) / VolumeNormal (+0x00)
// ---------------------------------------------------------------------------
void PassbyEffect::UpdateParams(f32 /*afDeltaTime*/)
{
    if (!UpdatePosition())
        return;

    const f32 lfRelativeVelocity = GetRelativeVelocityMag();
    if (lfRelativeVelocity < mAttribs.VelocityThreshold_Min() && meLifetime != E_CULLED && mVoice.IsPlaying())
    {
        const u32 luSend01 = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));
        mVoice.SetGain(0, 0.0f, &luSend01);
        meLifetime = E_CULLED;
    }

    const Attrib::Gen::passbybin::RwVector2& lrPitchCurve =
        mbPlayerIsBoosting ? mAttribs.PitchCurveBoost() : mAttribs.PitchCurveNormal();
    CgsSound::Utils::SlopeParams lParams(KF_MIN_CAR_SPEED, KF_MAX_CAR_SPEED, lrPitchCurve.x, lrPitchCurve.y);
    mfPitchScale = CgsSound::Utils::Slope(lParams).GetValue(
        lfRelativeVelocity, CgsSound::Utils::Curve::E_ONE_MINUS_EQPWR);

    const Attrib::Gen::passbybin::RwVector2& lrVolumeCurve =
        mbPlayerIsBoosting ? mAttribs.VolumeBoost() : mAttribs.VolumeNormal();
    lParams.mfMinOutput = lrVolumeCurve.x;
    lParams.mfMaxOutput = lrVolumeCurve.y;
    mfVolumeScale = CgsSound::Utils::Slope(lParams).GetValue(
        lfRelativeVelocity, CgsSound::Utils::Curve::E_ONE_MINUS_EQPWR);
}

// ---------------------------------------------------------------------------
// ProcessUpdate  @ 0x826F99C0  (DWARF cpp:306)
//
//   mpDynamicMixIo && SetDMixInput(0, 0)
//   lPassby = GetPassbyData()
//   mVoice.IsPlaying():
//     meLifetime == E_NOT_PLAYED -> E_HAS_PLAYED
//     azimuth = dmix ? (f32)GetDMixOutput(0, 3) * flt_820AA8EC : 0.0
//     pitch   = dmix ? (f32)GetDMixOutput(1, 1) * flt_820AA8F4 : 0.0
//     gain    = mfVolumeScale * GetRWACMixerOutputValue(lPassby.meType, 0) * lPassby.mfVolumeModifier
//     pitch   = mfPitchScale * pitch
//     lPassby.meType == Collision (12):
//         azimuth = 0.0
//         pitch = fmadds(min(mfRelativeVelocityMagnitude, 15) * (1/15), 0.4, 0.8) * pitch
//         (`fsubs ; fsel`: a NaN speed stays NaN)
//     mVoice.SetGain(0, gain, "Send01") ; SetParameter(0, pitch, "~SplicerPlayerVoice::Pitch~")
//     SetParameter(1, azimuth, "~SplicerPlayerVoice::Azimuth~")  (dword_8300832C / _30, CRT
//     0x82C63F10)
//   not playing: meLifetime is E_HAS_PLAYED or E_CULLED -> mpState->Detach()   ; state vt +0x18
// ---------------------------------------------------------------------------
void PassbyEffect::ProcessUpdate()
{
    if (GetDMixIOPtr())
        GetDMixIOPtr()->SetDMixInput(0, 0);
    const PassbyStateManager::Passby& lrPassby = GetPassbyData();

    if (mVoice.IsPlaying())
    {
        if (meLifetime == E_NOT_PLAYED)
            meLifetime = E_HAS_PLAYED;

        f32 lfAzimuth = 0.0f;
        if (GetDMixIOPtr())
            lfAzimuth = static_cast<f32>(GetDMixIOPtr()->GetDMixOutput(0, 3)) * KF_MIXER_AZIMUTH_TO_DEGREES;
        f32 lfPitch = 0.0f;
        if (GetDMixIOPtr())
            lfPitch = static_cast<f32>(GetDMixIOPtr()->GetDMixOutput(1, 1)) * KF_MIXER_Q12_TO_PITCH;

        const f32 lfGain = mfVolumeScale * GetRWACMixerOutputValue(lrPassby.meType, 0) * lrPassby.mfVolumeModifier;
        lfPitch = mfPitchScale * lfPitch;
        if (lrPassby.meType == AttribSys::Enums::ePassbyTypes::Collision)
        {
            lfAzimuth = 0.0f;
            const f32 lfSpeed = (lrPassby.mfRelativeVelocityMagnitude - KF_COLLISION_SPEED_MAX >= 0.0f)
                                    ? KF_COLLISION_SPEED_MAX
                                    : lrPassby.mfRelativeVelocityMagnitude;
            lfPitch = std::fmaf(lfSpeed * KF_COLLISION_SPEED_SCALE, KF_COLLISION_PITCH_RANGE,
                                KF_COLLISION_PITCH_MIN) * lfPitch;
        }

        const u32 luSend01 = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));
        const u32 luPitch = static_cast<u32>(CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Pitch~"));
        const u32 luAzimuth = static_cast<u32>(CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Azimuth~"));
        mVoice.SetGain(0, lfGain, &luSend01);
        mVoice.SetParameter(0, lfPitch, &luPitch);
        mVoice.SetParameter(1, lfAzimuth, &luAzimuth);

        // [DIAG] NOT IN THE X360 BINARY (BRN_PASSBY_SOUND_DIAG): what a playing pass-by sends.
        if (PassbyEffectDiag())
        {
            static u32 suPrintCount = 0;
            if (suPrintCount++ < 96u)
            {
                char lacLine[192];
                std::snprintf(lacLine, sizeof(lacLine),
                              "[passby-sound] playing type=%d sample=%u gain=%g pitch=%g azimuth=%g "
                              "volScale=%g pitchScale=%g lifetime=%d\n",
                              static_cast<s32>(lrPassby.meType), muSampleId, static_cast<double>(lfGain),
                              static_cast<double>(lfPitch), static_cast<double>(lfAzimuth),
                              static_cast<double>(mfVolumeScale), static_cast<double>(mfPitchScale),
                              static_cast<s32>(meLifetime));
                *CgsDev::Log::gpDebugPrint << lacLine;
            }
        }
        return;
    }

    if (meLifetime == E_HAS_PLAYED || meLifetime == E_CULLED)
    {
        // [DIAG] NOT IN THE X360 BINARY (BRN_PASSBY_SOUND_DIAG): the voice ended; the state lets go.
        if (PassbyEffectDiag())
        {
            static u32 suPrintCount = 0;
            if (suPrintCount++ < 64u)
            {
                char lacLine[128];
                std::snprintf(lacLine, sizeof(lacLine),
                              "[passby-sound] finished type=%d sample=%u lifetime=%d -> state Detach\n",
                              static_cast<s32>(lrPassby.meType), muSampleId, static_cast<s32>(meLifetime));
                *CgsDev::Log::gpDebugPrint << lacLine;
            }
        }
        GetStateBase()->Detach();
    }
}

// ---------------------------------------------------------------------------
// Detach  @ 0x826F9F10  (DWARF cpp:526)
//   !BrnEffectObject::Detach() -> false ; mVoice.Stop() ; SetMixerInputValue(1, 0) ; true
// ---------------------------------------------------------------------------
bool PassbyEffect::Detach()
{
    if (!BrnSound::Logic::BrnEffectObject::Detach())
        return false;
    mVoice.Stop();
    SetMixerInputValue(1, 0);
    return true;
}

} // namespace Passby
} // namespace Logic
} // namespace BrnSound
