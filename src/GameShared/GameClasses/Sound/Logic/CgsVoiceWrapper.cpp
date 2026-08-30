#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.h"
#include "GameShared/GameClasses/Sound/Playback/CgsContent.h"
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"

namespace CgsSound
{
namespace Logic
{

// ARTIST @ 0x826EAEB0.
void VoiceWrapper::Create(const CreateParams& arCreateParams)
{
    mCreateParams = arCreateParams;
    CGS_ASSERT(!mContent.IsCreated(), "!mContent");
    CGS_ASSERT(mCreateParams.mpLogicModule != 0,
               "mCreateParams.mpLogicModule");

    mVoice.Construct(mCreateParams.mpLogicModule,
                     mCreateParams.mpLogicModule->GetUniqueId(),
                     mCreateParams.mFactoryName,
                     mCreateParams.mVoiceSpecName);
    CGS_ASSERT(mVoice.GetVoiceObject() != 0, "mVoiceObject.IsCreated()");

    if (mCreateParams.mContentSpecName != 0)
    {
        Playback::Content* lpContent = 0;
        mCreateParams.mpLogicModule->GetPlaybackModule().CreateContent(
            &lpContent,
            mCreateParams.mpLogicModule->GetUniqueId(),
            Playback::Name(static_cast<uintptr_t>(mCreateParams.mFactoryName)),
            Playback::Name(static_cast<uintptr_t>(mCreateParams.mContentSpecName)));

        if (lpContent)
            lpContent->Acquire();
        mContent.Adopt(lpContent, mCreateParams.mpLogicModule);
        if (lpContent)
            lpContent->Release();
    }
    else
    {
        CGS_ASSERT(mCreateParams.mpContent != 0, "mCreateParams.mpContent");
        Playback::Content* lpContent =
            mCreateParams.mpContent->GetContent().GetObject();
        CGS_ASSERT(lpContent != 0, "Content not yet created!");
        if (lpContent)
            lpContent->Acquire();
        mContent.Adopt(lpContent, mCreateParams.mpLogicModule);
    }

    CGS_ASSERT(mContent.IsCreated(), "mContent");
    meUpdateStage = E_UPDATE_STAGE_CREATE;
}

// ARTIST @ 0x826EB088.
void VoiceWrapper::Play(u32 au32OptionalPlayParam)
{
    mbPlay = true;
    mu32OptionalPlayParam = au32OptionalPlayParam;
    CGS_ASSERT(meUpdateStage != E_UPDATE_STAGE_IDLE,
               "Cannot 'Play' a voice wrapper if it hasn't been 'Created'\n");
    if (meUpdateStage == E_UPDATE_STAGE_FINISHED)
        Create(mCreateParams);
}

// ARTIST @ 0x826DC570.
void VoiceWrapper::Stop()
{
    mbPlay = false;
    if (meUpdateStage == E_UPDATE_STAGE_PLAYING && mVoice.GetVoiceObject())
    {
        mVoice.Stop();
        meUpdateStage = E_UPDATE_STAGE_FINISHED;
    }
    if (meUpdateStage < E_UPDATE_STAGE_PLAYING)
        Release();
}

f32 VoiceWrapper::GetGain(const s32* apSendName) const
{
    return mVoice.GetVoiceObject() ? mVoice.GetGain(apSendName) : 0.0f;
}

void VoiceWrapper::SetGain(u32 au32SendIndex, f32 af32Gain,
                           const u32* apSendName)
{
    if (mVoice.GetVoiceObject())
        mVoice.SetGain(au32SendIndex, af32Gain, 0, apSendName);
}

void VoiceWrapper::SetParameter(s32 as32ParamIndex, f32 af32Value,
                                const u32* apParamName)
{
    if (mVoice.GetVoiceObject())
        mVoice.SetParameter(static_cast<u32>(as32ParamIndex), af32Value,
                            0, apParamName);
}

// ARTIST @ 0x826DC5E0.
void VoiceWrapper::Update()
{
    if (meUpdateStage == E_UPDATE_STAGE_IDLE)
        return;

    CGS_ASSERT(mCreateParams.mpLogicModule != 0,
               "mCreateParams.mpLogicModule != NULL");

    switch (meUpdateStage)
    {
        case E_UPDATE_STAGE_CREATE:
            meUpdateStage = E_UPDATE_STAGE_CREATE_WAIT;
            break;

        case E_UPDATE_STAGE_CREATE_WAIT:
            if (mCreateParams.mpOnPostInit)
                mCreateParams.mpOnPostInit->Call(*this);
            meUpdateStage = E_UPDATE_STAGE_CONNECT;
            // ARTIST continues through the connect leg in this tick.
        case E_UPDATE_STAGE_CONNECT:
        {
            mVoice.Connect(mCreateParams.mSendName,
                           mCreateParams.mSubMixVoiceID);
            if (mCreateParams.mReverbSendName &&
                mCreateParams.mReverbSubMixVoiceID)
            {
                mVoice.Connect(mCreateParams.mReverbSendName,
                               mCreateParams.mReverbSubMixVoiceID);
            }

            Playback::Content* lpContent = mContent.GetContent().GetObject();
            Playback::Handle<Playback::Content> lhContent(lpContent);
            if (lpContent)
                lpContent->Acquire();
            mVoice.Attach(static_cast<s32>(mCreateParams.mSlotName), &lhContent);
            meUpdateStage = E_UPDATE_STAGE_WAIT;
            // ARTIST checks the content state immediately after attachment.
        }
        case E_UPDATE_STAGE_WAIT:
            if (mContent.IsLoaded())
                meUpdateStage = E_UPDATE_STAGE_START;
            break;

        case E_UPDATE_STAGE_START:
            if (mbPlay)
            {
                u32 luSendName = mCreateParams.mSendName;
                mVoice.SetGain(static_cast<u32>(mCreateParams.miSendIndex),
                               0.0f, 0, &luSendName);
                mVoice.Play(static_cast<s32>(mu32OptionalPlayParam));
                meUpdateStage = E_UPDATE_STAGE_PLAYING;
            }
            break;

        case E_UPDATE_STAGE_PLAYING:
            if (!mVoice.IsPlaying())
            {
                Release();
                meUpdateStage = E_UPDATE_STAGE_FINISHED;
            }
            break;

        case E_UPDATE_STAGE_FINISHED:
            meUpdateStage = E_UPDATE_STAGE_FINISHED;
            break;

        default:
            CGS_ASSERT(false, "Unhandled case: meUpdateStage");
            break;
    }
}

// ARTIST @ 0x826C5270.
void VoiceWrapper::Release()
{
    if (mVoice.GetVoiceObject())
    {
        if (mVoice.IsPlaying())
            mVoice.Stop();
        mVoice.Detach(static_cast<s32>(mCreateParams.mSlotName));
        mVoice.Destruct();
    }

    Playback::Content* lpContent = mContent.GetContent().GetObject();
    if (mCreateParams.mContentSpecName && lpContent)
        lpContent->BeginRemove();
    if (lpContent)
        lpContent->Release();
    mContent.Adopt(0, 0);

    mbPlay = false;
    mu32OptionalPlayParam = 0;
    meUpdateStage = E_UPDATE_STAGE_IDLE;
}

// ARTIST @ 0x826C7E80; Release performs both owned-object drops.
VoiceWrapper::~VoiceWrapper()
{
    Release();
}

} // namespace Logic
} // namespace CgsSound
