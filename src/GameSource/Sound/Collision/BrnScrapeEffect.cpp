#include "GameSource/Sound/Collision/BrnScrapeEffect.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
#include "GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameSource/Sound/Collision/BrnCollisionControl.h"
#include "GameSource/Sound/Collision/BrnCollisionEffect.h"
#include "GameSource/Sound/Collision/BrnCollisionState.h"
#include "GameSource/Sound/Collision/BrnCollisionStateManager.h"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [DIAG] gpDebugPrint

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

// =============================================================================
// BrnSound::Logic::Collision::ScrapeEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnScrapeEffect.h for the class
// shape (DWARF: BrnScrapeEffect.h/.cpp) and the X360-32-bit-vs-host-64-bit offset note.
//
// This TU SHIPS the scalar deleting destructor @ 0x826E8A28 (its ~ScrapeEffect anchor).
//
// MapScrapeToMaterial @ 0x8269EC18 is verified store-for-store (both prior blockers
// resolved: CgsEntityId.h is 8/14/10; RootInputBuffer::GetPlayerActiveRaceCarIndex is
// committed) and its DWARF signature (private `u8 ... const`) is declared in the header.
// The BODY is left PARTIAL/deferred here for one reason only: a faithful body must call
// SoundLogicModule::GetBrnInputStructure(), which drags in the StateManager RTTI subtree
// (CgsEnvironment.h -> CgsStateManager.h), while ScrapeEffect's own base pulls the effect
// RTTI subtree (BrnEffectObject.h -> CgsEffectBase.h). Those two subtrees each define an
// INCOMPATIBLE CgsSound::Logic::ClassTypeInfo<T> (Variant A `typeName/baseTypeInfo` with
// ctor vs Variant B `mpcTypeName/mpfnCreateObject` aggregate), so co-including them in one
// TU is a pre-existing C2953 ODR fork. Unblocking cleanly needs a tree-wide ClassTypeInfo
// unification (out of this wave's scope), NOT an offset-hack/fabrication -- so per HARD
// RULE 6 the body stays deferred (declared-only) until that reconciliation lands.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

namespace
{
const u32 KU_SCRAPE_PARAMETERS[9] = {
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_pitch")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_volume")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_friction_stress")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_normal_stress")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_time_scraping")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_material_a")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_material_b")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_is_Crashing")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_azimuth")),
};
const u32 KU_SEND01 = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("Send01"));

// DWARF BrnScrapeEffect.cpp:32..43 -- the scrape voice's start delay and intensity shaping, read from
// the image (.data, in declaration order):
const f32 KF_TIME_DELAY_BEFORE_SCRAPES = 0.15f;   // cpp:32, 0x82F2CED8 = 0x3E19999A
const u8  KI_MATERIAL_AI = 1;                     // cpp:35 (MapScrapeToMaterial's buckets)
const u8  KI_MATERIAL_TRAFFIC = 2;                // cpp:36
const f32 KF_AEMS_FRICTION_SCALE_UP = 250.0f;     // cpp:39, 0x82F2CEDC = 0x437A0000
const f32 KF_RAMP_DOWN_TIME = 0.25f;              // cpp:40, 0x82F2CEE0 = 0x3E800000
const f32 KF_AI_SCALING_UP = 10.0f;               // cpp:42, 0x82F2CEE4 = 0x41200000
const f32 KF_WORLD_SCALING_UP = 2.5f;             // cpp:43, 0x82F2CEE8 = 0x40200000
}

ScrapeEffect::ScrapeEffect()
    : BrnSound::Logic::BrnEffectObject()
    , mfScrapeStartTimeStamp(0.0f)
    , mfParam_AEMS_pitch(0.0f)
    , mfParam_AEMS_volume(0.0f)
    , mfParam_AEMS_friction_stress(0.0f)
    , mfParam_AEMS_normal_stress(0.0f)
    , mfParam_AEMS_time_scraping(0.0f)
    , mfParam_AEMS_material_a(0.0f)
    , miParam_AEMS_material_b(0)
    , mfParam_AEMS_is_Crashing(0.0f)
    , mpCollision3DControl(nullptr)
    , mpCollisionControl(nullptr)
    , mScrapeVoice()
    , mbPlaying(false)
{
}

// ---------------------------------------------------------------------------
// ~ScrapeEffect  (the out-of-line anchor the scalar deleting destructor @ 0x826E8A28
// forwards to).
//
//   0x826E8A44  bl   ScrapeEffect::~ScrapeEffect()   ; real virtual dtor body
//   0x826E8A48  if (a2 & 1) { free via off_82FFB954 (slot +0x14) }
//   0x826E8A90  return this
//
// Same shape as the committed Brn3DEffectControl / Passby3DControl scalar deleting
// destructors: the compiler-emitted wrapper calls the real virtual destructor and then
// conditionally frees the object through the global sound allocator (off_82FFB954) when
// bit0 of the second arg is set. The real member teardown (mScrapeVoice + base settle)
// is produced by the inherited BrnEffectObject base chain + the embedded VoiceWrapper
// member dtor (BY NAME), so this anchor body is empty; the host toolchain re-synthesises
// the deleting-destructor thunk from this virtual destructor + operator delete. The raw
// allocator vtable call is NOT reproduced and no allocator is fabricated.
// ---------------------------------------------------------------------------
ScrapeEffect::~ScrapeEffect()
{
}

CgsSound::Logic::EffectObject* ScrapeEffect::CreateObject(u32 /*auAllocator*/)
{
    return new ScrapeEffect();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>*
ScrapeEffect::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo(
        0x50010, "ScrapeEffect",
        CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
        &ScrapeEffect::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>*
ScrapeEffect::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* ScrapeEffect::GetTypeName() const
{
    return "ScrapeEffect";
}

s32 ScrapeEffect::GetController(s32 aiIndex)
{
    if (aiIndex == 0)
        return 0;
    if (aiIndex == 1)
        return 1;
    return -1;
}

void ScrapeEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    CGS_ASSERT(apController != nullptr, "lpController");
    if (!apController)
        return;

    switch (apController->GetEffectID())
    {
    case 0:
        mpCollisionControl = static_cast<CollisionControl*>(apController);
        break;
    case 1:
        mpCollision3DControl = static_cast<Collision3DControl*>(apController);
        break;
    default:
        CGS_ASSERT(false, "Cound't attach controller");
        break;
    }
}

bool ScrapeEffect::Attach()
{
    CgsSound::Logic::EffectBase::Attach();
    mbPlaying = false;
    return true;
}

f32 ScrapeEffect::GetPitch() const
{
    return const_cast<ScrapeEffect*>(this)->GetMixerOutputValue(
        1, Nicotine::DMixIO::DMX_PITCH);
}

f32 ScrapeEffect::GetGain() const
{
    return const_cast<ScrapeEffect*>(this)->GetMixerOutputValue(
        0, Nicotine::DMixIO::DMX_VOL);
}

// ---------------------------------------------------------------------------
// ScrapeEffect::GetIntensity(CollisionState*, const ScrapeInfo&)  @ 0x826BD620  (DWARF cpp:360)
//
// How hard the scrape grinds, from the two sides' RELATIVE VELOCITY (refreshed every frame by
// CollisionStateManager::UpdateScrapeHistory), not the contact impulse:
//   |mRelativeVelocity| (vmsum3fp128 + the rsqrte refinement; a zero length stays 0, vsel)
//     * KF_AEMS_FRICTION_SCALE_UP
//     * KF_AI_SCALING_UP against a car or traffic (material_b 1 or 2: `addi -1 ; cmplwi 1 ; bgt`),
//       else KF_WORLD_SCALING_UP outside a fatality (the collision manager's meFatality == E_FATAL_OFF,
//       `lwz 0x24(state) ; lwzx +0x81E0`), else 1
//     * (1 - min(age, KF_RAMP_DOWN_TIME) / KF_RAMP_DOWN_TIME), age = now - the scrape's stamp (`fsel`:
//       a NaN age takes KF_RAMP_DOWN_TIME, i.e. 0).
// "now" is the collision manager's clock (`lfs 4(mgr)`); StateManager::UpdateParams stamps it into
// every state (mfCurTime) right before the state -- and so this effect -- updates, and the base's
// GetCurTime() accessor (DWARF CgsStateManager.h:229) is not in this tree, so the state's copy is read.
// The PC returned the contact impulse (mfIntensity).
// ---------------------------------------------------------------------------
f32 ScrapeEffect::GetIntensity(CollisionState* apState, const ScrapeInfo& arScrapeInfo)
{
    const Vector3& lrVelocity = arScrapeInfo.mRelativeVelocity;
    f32 lfIntensity = std::sqrt(lrVelocity.x * lrVelocity.x + lrVelocity.y * lrVelocity.y +
                                lrVelocity.z * lrVelocity.z) * KF_AEMS_FRICTION_SCALE_UP;
    const CollisionStateManager* lpManager =
        static_cast<const CollisionStateManager*>(apState->GetStateManager());
    if (miParam_AEMS_material_b == KI_MATERIAL_AI || miParam_AEMS_material_b == KI_MATERIAL_TRAFFIC)
        lfIntensity = KF_AI_SCALING_UP * lfIntensity;
    else if (lpManager->GetFrameInformation().meFatality.GetCurrent() == E_FATAL_OFF)
        lfIntensity = KF_WORLD_SCALING_UP * lfIntensity;
    const f32 lfAge = apState->GetCurrentTime() - arScrapeInfo.mfTimeStamp;
    const f32 lfRamp = (KF_RAMP_DOWN_TIME - lfAge >= 0.0f) ? lfAge : KF_RAMP_DOWN_TIME;
    return (1.0f - lfRamp / KF_RAMP_DOWN_TIME) * lfIntensity;
}

u8 ScrapeEffect::MapScrapeToMaterial(const ScrapeInfo& arScrapeInfo) const
{
    const BrnSound::Module::Io::LogicInputBuffer* lpInput =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule())
            ->GetBrnInputStructure();
    if (!lpInput)
        return 0;

    const u16 luPlayerIndex = static_cast<u16>(
        lpInput->GetPlayerActiveRaceCarIndex());
    const CgsSceneManager::EntityId lEntityA(arScrapeInfo.mEntityIdA.muValue);
    const CgsSceneManager::EntityId lEntityB(arScrapeInfo.mEntityIdB.muValue);

    u8 luOtherOwner = 0;
    if (lEntityA.GetOwner() == 1 && lEntityA.GetEntityIndex() == luPlayerIndex)
        luOtherOwner = lEntityB.GetOwner();
    else if (lEntityB.GetOwner() == 1 && lEntityB.GetEntityIndex() == luPlayerIndex)
        luOtherOwner = lEntityA.GetOwner();

    if (luOtherOwner == 1)
        return 1;
    if (luOtherOwner == 2)
        return 2;
    return 0;
}

// ---------------------------------------------------------------------------
// ScrapeEffect::UpdateParams(f32)  @ 0x826F8578  (DWARF cpp:228)
//
//   mScrapeVoice.Update(); lpState = mpCollisionControl's state (asserted, cpp:237)
//   pitch = GetPitch() (the mixer's pitch output), volume = GetGain()
//   lifetime SCRAPE:
//     is_Crashing = the control's scrape is crashing ? 32767 : 0
//     not playing and the state attached more than KF_TIME_DELAY_BEFORE_SCRAPES ago
//       (`lfs 0x140 ; fadds ; fcmpu 0x4C ; bge` -- a NaN clock never starts it):
//         create + play the AEMS_ScrapeGranulator voice on the manager's scrape bank; volume 32767,
//         friction / normal / time 0, pitch 4095 (flt_820B7990), material_a 0, material_b =
//         MapScrapeToMaterial; playing; start stamp = now
//     otherwise (playing, or still inside the delay):
//         friction_stress = GetIntensity clamped to [0, 32767] (`fsel` twice: a NaN intensity
//         gives 32767), time_scraping = now - start stamp; normal_stress and material_a keep what
//         the start set (0)
//   lifetime just left SCRAPE: stop the voice, friction / normal / time 0
// The PC started the voice as soon as the clock passed the attach time, and while playing wrote
// friction = 32767 - intensity (inverted), normal = intensity and material_a = the orientation.
// The tail (0x826F8A3C..0x826F8A6C) only configures the 3D control's debug renderer
// (mDebugRenderingMessageData: mbEnable = KI_DRAW_SCRAPES != 0, a zero .bss tweak; mfYOffset = 2.0)
// -- a protected member of Cgs3dEffectControl with no audio reader; left out.
// ---------------------------------------------------------------------------
void ScrapeEffect::UpdateParams(f32 /*afDeltaTime*/)
{
    mScrapeVoice.Update();
    CollisionState* lpState = mpCollisionControl->GetCollisionState();
    CGS_ASSERT(lpState != nullptr, "lpCollisionState");

    const CollisionState::ELifetime leCurrent =
        lpState->GetLifetime().GetCurrent();
    const CollisionState::ELifetime lePrevious =
        lpState->GetLifetime().GetPrevious();

    mfParam_AEMS_pitch = GetPitch();
    mfParam_AEMS_volume = GetGain();

    if (leCurrent == CollisionState::E_SCRAPE)
    {
        const ScrapeInfo& lrScrape =
            mpCollisionControl->GetScrapeInfo().GetCurrent();
        mfParam_AEMS_is_Crashing = lrScrape.mbCrashing ? 32767.0f : 0.0f;
        if (!mbPlaying &&
            lpState->GetTimeWeAttached() + KF_TIME_DELAY_BEFORE_SCRAPES < lpState->GetCurrentTime())
        {
            const CollisionStateManager* lpManager =
                static_cast<const CollisionStateManager*>(lpState->GetStateManager());

            CgsSound::Logic::VoiceWrapper::CreateParams lParams;
            lParams.mpLogicModule = GetLogicModule();
            lParams.mFactoryName = static_cast<u32>(
                CgsSound::Playback::AemsFactorySkName().GetValue());
            lParams.mVoiceSpecName = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash("AEMS_ScrapeGranulator"));
            lParams.mpContent = &lpManager->GetScrapeAemsBank();
            lParams.mSlotName = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash("AEMS_Slot"));
            lParams.mSendName = KU_SEND01;
            lParams.mSubMixVoiceID = 1;
            lParams.miSendIndex = 0;
            mScrapeVoice.Create(lParams);
            mScrapeVoice.Play(0);

            mfParam_AEMS_volume = 32767.0f;
            mfParam_AEMS_friction_stress = 0.0f;
            mfParam_AEMS_normal_stress = 0.0f;
            mfParam_AEMS_time_scraping = 0.0f;
            mfParam_AEMS_pitch = 4095.0f;
            mfParam_AEMS_material_a = 0.0f;
            miParam_AEMS_material_b = MapScrapeToMaterial(lrScrape);
            // [DIAG] NOT IN THE X360 BINARY (BRN_COLLISION_AUDIO_DIAG): the scrape voice starts.
            if (std::getenv("BRN_COLLISION_AUDIO_DIAG") != nullptr && CgsDev::Log::gpDebugPrint)
            {
                static u32 suStartPrintCount = 0;
                if (suStartPrintCount++ < 16u)
                {
                    char lacLine[192];
                    std::snprintf(lacLine, sizeof(lacLine),
                                  "[collision-audio] scrape voice start A=%u:%u B=%u:%u orient=%d material_b=%d "
                                  "attached=%.3f now=%.3f live=%d\n",
                                  lrScrape.mEntityIdA.muValue >> 24, (lrScrape.mEntityIdA.muValue >> 10) & 0x3FFFu,
                                  lrScrape.mEntityIdB.muValue >> 24, (lrScrape.mEntityIdB.muValue >> 10) & 0x3FFFu,
                                  static_cast<s32>(lrScrape.meOrientation),
                                  static_cast<s32>(miParam_AEMS_material_b), lpState->GetTimeWeAttached(),
                                  lpState->GetCurrentTime(), mScrapeVoice.HasLiveVoice() ? 1 : 0);
                    *CgsDev::Log::gpDebugPrint << lacLine;
                }
            }
            mbPlaying = true;
            mfScrapeStartTimeStamp = lpState->GetCurrentTime();
        }
        else
        {
            const f32 lfIntensity = GetIntensity(lpState, lrScrape);
            const f32 lfFloored = (-lfIntensity >= 0.0f) ? 0.0f : lfIntensity;
            mfParam_AEMS_friction_stress = (32767.0f - lfFloored >= 0.0f) ? lfFloored : 32767.0f;
            mfParam_AEMS_time_scraping =
                lpState->GetCurrentTime() - mfScrapeStartTimeStamp;
        }
    }
    else if (lePrevious == CollisionState::E_SCRAPE && lePrevious != leCurrent)
    {
        mScrapeVoice.Stop();
        mfParam_AEMS_friction_stress = 0.0f;
        mfParam_AEMS_normal_stress = 0.0f;
        mfParam_AEMS_time_scraping = 0.0f;
    }
}

// ---------------------------------------------------------------------------
// ScrapeEffect::ProcessUpdate()  @ 0x826BD6F8  (DWARF cpp:470)
//
// While the voice is live (`lwz 0x38(voice) ; cmplwi ; beq` before each call) the nine AEMS
// parameters and the send gain go to it; the mixer input 0 takes friction_stress EVERY frame
// (`fctiwz` + SetMixerInputValue, 0x826BD900..0x826BD91C, after the last voice test) -- the PC
// returned before it whenever no voice was live, so the mixer kept a stale friction.
// ---------------------------------------------------------------------------
void ScrapeEffect::ProcessUpdate()
{
    if (mScrapeVoice.HasLiveVoice())
    {
        mScrapeVoice.SetParameter(0, mfParam_AEMS_pitch, &KU_SCRAPE_PARAMETERS[0]);
        mScrapeVoice.SetParameter(1, mfParam_AEMS_volume, &KU_SCRAPE_PARAMETERS[1]);
        mScrapeVoice.SetParameter(2, mfParam_AEMS_friction_stress, &KU_SCRAPE_PARAMETERS[2]);
        mScrapeVoice.SetParameter(3, mfParam_AEMS_normal_stress, &KU_SCRAPE_PARAMETERS[3]);
        mScrapeVoice.SetParameter(4, mfParam_AEMS_time_scraping, &KU_SCRAPE_PARAMETERS[4]);
        mScrapeVoice.SetParameter(5, mfParam_AEMS_material_a, &KU_SCRAPE_PARAMETERS[5]);
    }
    const f32 lfAzimuth = GetMixerOutputValue(0, 3);
    if (mScrapeVoice.HasLiveVoice())
    {
        mScrapeVoice.SetParameter(8, lfAzimuth, &KU_SCRAPE_PARAMETERS[8]);
        mScrapeVoice.SetParameter(6,
            miParam_AEMS_material_b == 2 ? 1.0f : static_cast<f32>(miParam_AEMS_material_b),
            &KU_SCRAPE_PARAMETERS[6]);
        mScrapeVoice.SetParameter(7, mfParam_AEMS_is_Crashing, &KU_SCRAPE_PARAMETERS[7]);
        mScrapeVoice.SetGain(0, 1.0f, &KU_SEND01);
    }
    SetMixerInputValue(0, static_cast<s32>(mfParam_AEMS_friction_stress));
}

bool ScrapeEffect::Detach()
{
    mScrapeVoice.Stop();
    mScrapeVoice.Release();
    mbPlaying = false;
    return BrnSound::Logic::BrnEffectObject::Detach();
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const
    gpScrapeEffectReg =
        CgsSound::Logic::EffectObject::AddToClassTypeInfoArray(
            ScrapeEffect::GetStaticTypeInfo());

} // namespace Collision
} // namespace Logic
} // namespace BrnSound
