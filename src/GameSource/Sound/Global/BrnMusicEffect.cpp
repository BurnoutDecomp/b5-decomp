#include "GameSource/Sound/Global/BrnMusicEffect.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Streaming/BrnStreamingStateManager.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnSound
{
namespace Logic
{

MusicEffect::EaTraxData::EaTraxData()
    : maEaTraxHandles(), maReserved0x20(), miTrackIndex0(-1), miTrackIndex1(-1),
      miTrackIndex2(-1), mpLogicModule(0), miState(0), mbReserved0x44(0) {}

bool MusicEffect::EaTraxData::Prepare(Module::SoundLogicModule* apLogicModule)
{
    CGS_ASSERT(apLogicModule != 0, "lpLogicModule");
    for (s32 i = 0; i < 4; ++i)
        maEaTraxHandles[i] = 0;
    miTrackIndex0 = miTrackIndex1 = miTrackIndex2 = -1;
    mpLogicModule = apLogicModule;
    miState = 0;
    mbReserved0x44 = 0;
    return true;
}

MusicEffect::MusicEffect()
    : BrnEffectObject(), mEaTraxStream(), mEventStream(), mSpecialStream(),
      mMenuStream(), mEaTraxData() {}

MusicEffect::~MusicEffect() {}

CgsSound::Logic::EffectObject* MusicEffect::CreateObject(u32)
{
    return new MusicEffect();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* MusicEffect::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo(
        0x20, "MusicEffect", CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
        &MusicEffect::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* MusicEffect::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* MusicEffect::GetTypeName() const { return "MusicEffect"; }

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpMusicEffectReg =
    CgsSound::Logic::EffectObject::AddToClassTypeInfoArray(MusicEffect::GetStaticTypeInfo());

bool MusicEffect::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;
    Module::SoundLogicModule* lpModule = static_cast<Module::SoundLogicModule*>(mpLogicModule);
    Streaming::StreamingStateManager* lpStreaming =
        static_cast<Streaming::StreamingStateManager*>(
            lpModule->GetEnvironment().GetStateManager(6));
    CGS_ASSERT(lpStreaming != 0, "lpStreamingStateManager");
    if (!lpStreaming)
        return false;
    mEaTraxStream.Prepare(lpModule, lpStreaming, "MusicFiltVoiceSpec");
    mEventStream.Prepare(lpModule, lpStreaming, "MusicFiltVoiceSpec");
    mSpecialStream.Prepare(lpModule, lpStreaming, "MusicFiltVoiceSpec");
    mMenuStream.Prepare(lpModule, lpStreaming, "MusicFiltVoiceSpec");
    return mEaTraxData.Prepare(lpModule);
}

void MusicEffect::UpdateParams(f32 afDeltaTime)
{
    mEaTraxStream.Update(afDeltaTime);
    mEventStream.Update(afDeltaTime);
    mSpecialStream.Update(afDeltaTime);
    mMenuStream.Update(afDeltaTime);
}

void MusicEffect::ProcessUpdate() {}

void MusicEffect::Notify(const CgsSound::Io::MessageHeader* apMessage)
{
    if (!apMessage)
        return;
    if (apMessage->GetEventId() == 13)
    {
        const CgsSound::Io::Message<CgsGui::GuiEventPlayMusicOnMenuStream>* lpMessage =
            static_cast<const CgsSound::Io::Message<CgsGui::GuiEventPlayMusicOnMenuStream>*>(apMessage);
        const u32 luMenuName = lpMessage->mData.muStreamNameHash;
        if (luMenuName == 0)
            mMenuStream.Stop();
        else
        {
            // GetEventStartContentSpec maps the authored menu event name onto
            // its StreamsRegistry ContentSpec.  This is the title path executed
            // by the current PC boot flow; both names are from the shipped data.
            u32 luContentSpec = luMenuName;
            const u32 luGunsAndRoses = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash("GunsAndRoses"));
            if (luMenuName == luGunsAndRoses)
                luContentSpec = static_cast<u32>(
                    CgsSound::Playback::Name::MakeHash("Guns_And_Roses"));
            mMenuStream.Queue(luContentSpec, 6);
        }
    }
    else if (apMessage->GetEventId() == 28)
    {
        const CgsSound::Io::Message<CgsSound::Playback::Name>* lpMessage =
            static_cast<const CgsSound::Io::Message<CgsSound::Playback::Name>*>(apMessage);
        mSpecialStream.Queue(static_cast<u32>(lpMessage->mData.GetValue()), 6);
    }
}

} // namespace Logic
} // namespace BrnSound
