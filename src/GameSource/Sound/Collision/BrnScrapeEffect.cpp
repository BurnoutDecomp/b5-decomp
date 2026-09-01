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

#include <algorithm>

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

f32 ScrapeEffect::GetIntensity(CollisionState* /*apState*/,
                               const ScrapeInfo& arScrapeInfo)
{
    return arScrapeInfo.mfIntensity;
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

void ScrapeEffect::UpdateParams(f32 /*afDeltaTime*/)
{
    mScrapeVoice.Update();
    CGS_ASSERT(mpCollisionControl != nullptr, "mpCollisionControl");
    CollisionState* lpState =
        mpCollisionControl ? mpCollisionControl->GetCollisionState() : nullptr;
    CGS_ASSERT(lpState != nullptr, "lpCollisionState");
    if (!lpState)
        return;

    const CollisionState::ELifetime leCurrent =
        lpState->GetLifetime().GetCurrent();
    const CollisionState::ELifetime lePrevious =
        lpState->GetLifetime().GetPrevious();
    const ScrapeInfo& lrScrape =
        mpCollisionControl->GetScrapeInfo().GetCurrent();

    mfParam_AEMS_pitch = GetPitch();
    mfParam_AEMS_volume = GetGain();

    if (leCurrent == CollisionState::E_SCRAPE)
    {
        mfParam_AEMS_is_Crashing = lrScrape.mbCrashing ? 32767.0f : 0.0f;
        if (!mbPlaying && lpState->GetCurrentTime() > lpState->GetTimeWeAttached())
        {
            CollisionStateManager* lpManager =
                static_cast<CollisionStateManager*>(lpState->GetStateManager());
            CGS_ASSERT(lpManager != nullptr, "lpCollisionStateManager");
            if (!lpManager)
                return;

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
            mfScrapeStartTimeStamp = lpState->GetCurrentTime();
            mbPlaying = true;
        }
        else if (mbPlaying)
        {
            const f32 lfIntensity = std::max(0.0f,
                GetIntensity(lpState, lrScrape));
            mfParam_AEMS_friction_stress =
                std::max(0.0f, 32767.0f - std::min(32767.0f, lfIntensity));
            mfParam_AEMS_normal_stress = lfIntensity;
            mfParam_AEMS_time_scraping =
                lpState->GetCurrentTime() - mfScrapeStartTimeStamp;
            mfParam_AEMS_material_a = static_cast<f32>(lrScrape.meOrientation);
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

void ScrapeEffect::ProcessUpdate()
{
    if (!mScrapeVoice.HasLiveVoice())
        return;

    mScrapeVoice.SetParameter(0, mfParam_AEMS_pitch, &KU_SCRAPE_PARAMETERS[0]);
    mScrapeVoice.SetParameter(1, mfParam_AEMS_volume, &KU_SCRAPE_PARAMETERS[1]);
    mScrapeVoice.SetParameter(2, mfParam_AEMS_friction_stress, &KU_SCRAPE_PARAMETERS[2]);
    mScrapeVoice.SetParameter(3, mfParam_AEMS_normal_stress, &KU_SCRAPE_PARAMETERS[3]);
    mScrapeVoice.SetParameter(4, mfParam_AEMS_time_scraping, &KU_SCRAPE_PARAMETERS[4]);
    mScrapeVoice.SetParameter(5, mfParam_AEMS_material_a, &KU_SCRAPE_PARAMETERS[5]);
    mScrapeVoice.SetParameter(6,
        miParam_AEMS_material_b == 2 ? 1.0f : static_cast<f32>(miParam_AEMS_material_b),
        &KU_SCRAPE_PARAMETERS[6]);
    mScrapeVoice.SetParameter(7, mfParam_AEMS_is_Crashing, &KU_SCRAPE_PARAMETERS[7]);
    const f32 lfAzimuth = GetMixerOutputValue(0, 3);
    mScrapeVoice.SetParameter(8, lfAzimuth, &KU_SCRAPE_PARAMETERS[8]);
    mScrapeVoice.SetGain(0, 1.0f, &KU_SEND01);
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
