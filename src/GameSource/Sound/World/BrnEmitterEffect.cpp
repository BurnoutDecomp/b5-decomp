#include "GameSource/Sound/World/BrnEmitterEffect.h"

#include "GameSource/Sound/Module/LogicModule/BrnEmitter3dControl.h"
#include "GameSource/Sound/Module/LogicModule/BrnEmitterState.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "SharedClasses/Sound/World/BrnStaticSoundMap.h"
#include "GameSource/AttribSys/Generated/classes/worldemitter.h"
#include "GameSource/AttribSys/Generated/classes/worldemitterlist.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"      // CgsSound::Utils::Curve (ProcessUpdate fall-off)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [DIAG] BRN_EMITTER_DIAG

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace BrnSound
{
namespace Logic
{
namespace World
{

namespace
{
// The emitter effect's mixer outputs. Only the azimuth output is attested by name: ProcessUpdate
// @0x826E6BC8 asserts `mi16PitchOutput < E_WORLD_EMITTER_EFFECT_OUTPUT_AZIMUTH` (l.185) as
// `extsh r9 ; cmpwi cr6,r9,3` (0x826E6DB0/0x826E6DB4) and reads the azimuth from output 3.
enum { E_WORLD_EMITTER_EFFECT_OUTPUT_AZIMUTH = 3 };
} // namespace

EmitterEffect::EmitterEffect()
    : BrnEffectObject()
    , mVoice()
    , mPos()
    , mp3dControl(0)
    , mi16PitchOutput(0)
{
}

EmitterEffect::~EmitterEffect()
{
}

const char* EmitterEffect::GetTypeName() const
{
    return "EmitterEffect";
}

CgsSound::Logic::EffectObject* EmitterEffect::CreateObject(u32)
{
    return new EmitterEffect();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>*
EmitterEffect::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>
        sTypeInfo(0x70000, "EmitterEffect",
                  CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
                  &EmitterEffect::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>*
EmitterEffect::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const
    gpEmitterEffectReg =
        CgsSound::Logic::EffectObject::AddToClassTypeInfoArray(
            EmitterEffect::GetStaticTypeInfo());

s32 EmitterEffect::GetController(s32 aiIndex)
{
    return aiIndex == 0 ? 0 : -1;
}

// ARTIST @0x826866A8: `lwz r11,0x14(r4) ; rlwinm r11,r11,0,21,27` (id & 0x7F0) -- non-zero fires
// "Unexpected control." (l.138) and returns, zero stores the controller at +0x9C. The console reads
// the controller without a null test.
void EmitterEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    CGS_ASSERT((apController->GetId() & 0x7F0) == 0, "Unexpected control.");
    if ((apController->GetId() & 0x7F0) == 0)
        mp3dControl = static_cast<Emitter3dControl*>(apController);
}

const BrnSound::World::StaticSoundEntity& EmitterEffect::GetSoundEntity() const
{
    const EmitterState* lpState = static_cast<const EmitterState*>(mpState);
    CGS_ASSERT(lpState != 0, "lpState");
    CGS_ASSERT(lpState && lpState->IsAttached(), "IsAttached()");
    return lpState->GetSoundEntity();
}

bool EmitterEffect::Attach()
{
    CgsSound::Logic::EffectBase::Attach();

    // ARTIST 0x826F576C..0x826F57C0: the entity position is copied into mPos and handed to the
    // 3D control (`lwz r3,0x9C(r31)` ; vtable +0x34 ; `bctrl`) with no null test, then
    // "lpLogicModule" is asserted (l.231) and the body carries on regardless.
    const BrnSound::World::StaticSoundEntity& lrEntity = GetSoundEntity();
    mPos = lrEntity.GetPos();
    mp3dControl->AttachEmitterPosition(&mPos);

    BrnSound::Module::SoundLogicModule* lpLogicModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    CGS_ASSERT(lpLogicModule != 0, "lpLogicModule");

    // ARTIST 0x826F57E4..0x826F57F8: the entity's packed W lane read as one u32, high half
    // (`srwi r29,r11,16`). The count is the list's SCALAR mNumWorldEmitters at layout +0x4B8
    // (`lwz r11,0x4B8(r11)`, 0x826F5804 for the assert and 0x826F5838 for the gate), not
    // the array header's Num_mWorldEmitters -- see worldemitterlist.h.
    Attrib::Gen::worldemitterlist lWorldEmitters(
        lpLogicModule->GetGlobalData().WorldEmitterList());
    const u32 luEmitter = lrEntity.GetType();
    CGS_ASSERT(luEmitter < static_cast<u32>(lWorldEmitters.mNumWorldEmitters()),
               "luEmitter < static_cast< uint32_t >( lWorldEmitters.mNumWorldEmitters() )");

    // [DIAG] NOT IN THE X360 BINARY (BRN_EMITTER_DIAG=1): FX-EMITTER live witness -- every
    // world emitter that attaches (capped) and every entity type the console gate refuses.
    static const bool sbEmitterDiag = std::getenv("BRN_EMITTER_DIAG") != nullptr;
    if (sbEmitterDiag && CgsDev::Log::gpDebugPrint &&
        luEmitter >= static_cast<u32>(lWorldEmitters.mNumWorldEmitters()))
    {
        static u32 suRefusedLines = 0;
        if (suRefusedLines++ < 16u)
            *CgsDev::Log::gpDebugPrint << "[emitter] REFUSED type " << luEmitter
                << " >= mNumWorldEmitters " << lWorldEmitters.mNumWorldEmitters()
                << " (array count " << lWorldEmitters.Num_mWorldEmitters() << ") radius "
                << lrEntity.GetRadius() << " pos (" << mPos.x << ", " << mPos.y << ", "
                << mPos.z << ")\n";
    }

    mi16PitchOutput = 1;
    if (luEmitter < static_cast<u32>(lWorldEmitters.mNumWorldEmitters()))
    {
        Attrib::Gen::worldemitter lEmitter;
        lEmitter.ChangeWithDefault(lWorldEmitters.mWorldEmitters(luEmitter));
        if (sbEmitterDiag && CgsDev::Log::gpDebugPrint)
        {
            static u32 suAttachLines = 0;
            if (suAttachLines++ < 48u)
                *CgsDev::Log::gpDebugPrint << "[emitter] attach type " << luEmitter << " of "
                    << lWorldEmitters.mNumWorldEmitters() << " name "
                    << (lEmitter.EmitterName() ? lEmitter.EmitterName() : "(null)")
                    << " radius " << lrEntity.GetRadius() << " pos (" << mPos.x << ", "
                    << mPos.y << ", " << mPos.z << ") doppler "
                    << static_cast<u32>(lEmitter.AffectedByDoppler()) << " stream "
                    << static_cast<u32>(lEmitter.IsStream()) << "\n";
        }
        // ARTIST 0x826F586C..0x826F5890: a streamed emitter fires the assert (l.287) and skips
        // the voice; any other emitter's EmitterName is hashed as it stands (no null test).
        CGS_ASSERT(!lEmitter.IsStream(),
                   "EmitterEffect : Emitter streams not yet supported.");
        if (!lEmitter.IsStream())
        {
            if (lEmitter.AffectedByDoppler())
                mi16PitchOutput = 2;

            CgsSound::Logic::VoiceWrapper::CreateParams lParams;
            lParams.mpLogicModule = lpLogicModule;
            lParams.mFactoryName = static_cast<u32>(
                CgsSound::Playback::GenericRwacFactorySkName().GetValue());
            lParams.mVoiceSpecName = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash("PositionalVoiceSpec"));
            lParams.mContentSpecName = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash(lEmitter.EmitterName()));
            lParams.mSlotName = static_cast<u32>(
                CgsSound::Playback::PlayerVoice::SK_PLAYER_SLOT_NAME.GetValue());
            lParams.mSendName = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash("Send01"));
            lParams.mSubMixVoiceID = 1;
            lParams.mReverbSendName = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash("ReverbSend"));
            lParams.mReverbSubMixVoiceID = 2;
            lParams.miSendIndex = 0;
            mVoice.Create(lParams);
            mVoice.Play(0);

            const u32 luRadius = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash("SimplePanningRadius"));
            const u32 luCentre = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash("SimplePanningCentreLevel"));
            const u32 luMain = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash("SimplePanningMainLevel"));
            const u32 luLfe = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash("SimplePanningLfeLevel"));
            mVoice.SetParameter(2, 0.85f, &luRadius);
            mVoice.SetParameter(3, 1.0f, &luCentre);
            mVoice.SetParameter(4, 1.0f, &luMain);
            mVoice.SetParameter(5, 0.0f, &luLfe);
        }
    }
    return true;
}

// ARTIST @0x826E6BC8. "lpModule" is asserted (l.165) and the body carries on. The listener
// (module +0x29E0, the frame's player position) is measured against the entity through the
// entity radius (W & 0xFFFF: `and r10,r10,r12` with r12 = ...0000FFFF, `fcfid`, then
// `fdivs f0,f31(1.0),f0` -- a reciprocal) and the vmsum3fp/vrsqrtefp length, clamped to
// [0, 1] by the two `fsel`s. The volume is the mixer output scaled by the fall-off
// Curve::GetOutput(1 - t, E_ONE_MINUS_EQPWR), inlined at 0x826E6D70..0x826E6DE4 (the
// "( lfInput <= 1.0f ) && ( lfInput >= 0.0f )" assert of CgsSoundUtils.cpp:161, then
// `fmsubs f0,f30,511,511 ; fctiwz ; slwi 2 ; subf` into the quarter-sine table at 0x82F2D920
// and `fsubs f0,1.0,f0`) -- 1 - sin(t * pi/2), not the linear 1 - t this body used to carry.
void EmitterEffect::ProcessUpdate()
{
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    CGS_ASSERT(lpModule != 0, "lpModule");

    const BrnSound::World::StaticSoundEntity& lrEntity = GetSoundEntity();
    const rw::math::vpu::Vector3 lListener =
        lpModule->GetFrameInformation().mPlayerTransform.Pos();
    const f32 lfInvRadius = 1.0f / lrEntity.GetRadius();
    const f32 lfDistance = rw::math::vpu::Length(lListener - lrEntity.GetPos());
    const f32 lfFraction = std::max(0.0f, std::min(1.0f, lfDistance * lfInvRadius));

    const f32 lfVolume = GetRWACMixerOutputValue(0, 0);
    const f32 lfReverb = GetRWACMixerOutputValue(4, 0);
    const f32 lfDistanceGain = CgsSound::Utils::Curve::GetOutput(
        1.0f - lfFraction, CgsSound::Utils::Curve::E_ONE_MINUS_EQPWR);

    CGS_ASSERT(mi16PitchOutput < E_WORLD_EMITTER_EFFECT_OUTPUT_AZIMUTH,
               "mi16PitchOutput < E_WORLD_EMITTER_EFFECT_OUTPUT_AZIMUTH");
    const f32 lfPitch = GetRWACMixerOutputValue(mi16PitchOutput, 1);
    const f32 lfAzimuth = GetRWACMixerOutputValue(3, 3);

    const u32 luAzimuth = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("SimplePanningAzimuth"));
    const u32 luPitch = static_cast<u32>(CgsSound::Playback::Name::MakeHash(
        "~GenericRwacPlayerVoice::SK_PLAYER_PARAMETER_PITCH~"));
    const u32 luSend = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("Send01"));
    const u32 luReverb = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("ReverbSend"));
    mVoice.SetParameter(1, lfAzimuth, &luAzimuth);
    mVoice.SetParameter(0, lfPitch, &luPitch);
    mVoice.SetGain(0, lfDistanceGain * lfVolume, &luSend);
    mVoice.SetGain(1, lfReverb, &luReverb);
    mVoice.Update();
}

// ARTIST @0x826F5A10 -- an export hole: slot 8 of the effect's +4 vtable 0x820B3FDC, the bytes
// right after Attach (tools/re/ppcdis.py). A staged detach on meDetachState (`cmplwi 3 ; bgt`
// + a four-entry jump table): NONE/BEGIN store BEGIN and fall into UPDATING, which runs
// BrnEffectObject::Detach and reports "not yet" while that fails; FINISHED releases the voice
// and reports done; any other state reports "not detached". The 3D control keeps its position
// pointer -- the console never hands it a null one.
bool EmitterEffect::Detach()
{
    switch (meDetachState)
    {
    case E_DETACH_STATE_NONE:
    case E_DETACH_STATE_BEGIN:
        meDetachState = E_DETACH_STATE_BEGIN;
        // fall through
    case E_DETACH_STATE_UPDATING:
        meDetachState = E_DETACH_STATE_UPDATING;
        if (!BrnEffectObject::Detach())
            return false;
        // fall through
    case E_DETACH_STATE_FINISHED:
        meDetachState = E_DETACH_STATE_FINISHED;
        mVoice.Release();
        return true;
    default:
        return false;
    }
}

} // namespace World
} // namespace Logic
} // namespace BrnSound
