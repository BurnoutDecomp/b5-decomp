#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.h"
#include "GameShared/GameClasses/Sound/Playback/CgsContent.h"
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"

// ARTIST's VoiceWrapper asserts name the voice source file (string 0x820B32C4).
#define CGS_VOICEWRAPPER_ARTIST_CPP "..\\..\\..\\GameShared\\GameClasses\\Sound/Logic/CgsVoice.cpp"

namespace CgsSound
{
namespace Logic
{

// ARTIST @ 0x826EAEB0.
void VoiceWrapper::Create(const CreateParams& arCreateParams)
{
    mCreateParams = arCreateParams;
    CGS_ASSERT_AT(!mContent, "!mContent", CGS_VOICEWRAPPER_ARTIST_CPP, 243);
    CGS_ASSERT_AT(mCreateParams.mpLogicModule, "mCreateParams.mpLogicModule",
                  CGS_VOICEWRAPPER_ARTIST_CPP, 244);

    mVoice.Construct(mCreateParams.mpLogicModule,
                     mCreateParams.mFactoryName,
                     mCreateParams.mVoiceSpecName);
    CGS_ASSERT_AT(mVoice.IsCreated(), "mVoiceObject.IsCreated()", CGS_VOICEWRAPPER_ARTIST_CPP, 248);

    if (mCreateParams.mContentSpecName != 0)
    {
        // DWARF BrnSoundUnity: the block local `uint32_t lUniqueId` (CgsVoice.cpp:252). The
        // returned temporary is assigned through the out-of-line Handle<Content>::operator=
        // (ARTIST 0x826EAFD0) and then destroyed.
        const u32 lUniqueId = mCreateParams.mpLogicModule->GetUniqueId();
        mContent = mCreateParams.mpLogicModule->GetPlaybackModule().CreateContent(
            lUniqueId, mCreateParams.mFactoryName, mCreateParams.mContentSpecName);
    }
    else
    {
        CGS_ASSERT_AT(mCreateParams.mpContent, "mCreateParams.mpContent",
                      CGS_VOICEWRAPPER_ARTIST_CPP, 258);
        mContent = mCreateParams.mpContent->GetContent();
    }

    CGS_ASSERT_AT(mContent, "mContent", CGS_VOICEWRAPPER_ARTIST_CPP, 261);
    meUpdateStage = E_UPDATE_STAGE_CREATE;
}

// ARTIST @ 0x826EB088.
void VoiceWrapper::Play(u32 au32OptionalPlayParam)
{
    mbPlay = true;
    mu32OptionalPlayParam = au32OptionalPlayParam;
    CGS_ASSERT_AT(meUpdateStage != E_UPDATE_STAGE_IDLE,
        "Cannot 'Play' a voice wrapper if it hasn't been 'Created'\n",
            "..\\..\\..\\GameShared\\GameClasses\\Sound/Logic/CgsVoice.cpp", 284 /* ARTIST 0x826EB0BC */);

    if (meUpdateStage == E_UPDATE_STAGE_FINISHED)
        Create(mCreateParams);
}

// ARTIST @ 0x826DC570.
void VoiceWrapper::Stop()
{
    mbPlay = false;
    if (meUpdateStage == E_UPDATE_STAGE_PLAYING && mVoice.IsCreated())
    {
        mVoice.Stop();
        meUpdateStage = E_UPDATE_STAGE_FINISHED;
    }
    if (meUpdateStage < E_UPDATE_STAGE_PLAYING)
        Release();
}

f32 VoiceWrapper::GetGain(const s32* apSendName) const
{
    if (mVoice.IsCreated())
    {
        const s32 liSendName = *apSendName;
        return mVoice.GetGain(&liSendName);
    }
    return 0.0f;
}

void VoiceWrapper::SetGain(u32 au32SendIndex, f32 af32Gain,
                           const u32* apSendName)
{
    if (mVoice.GetVoiceObject())
        mVoice.SetGain(au32SendIndex, af32Gain, apSendName);
}

void VoiceWrapper::SetParameter(s32 as32ParamIndex, f32 af32Value,
                                const u32* apParamName)
{
    if (mVoice.GetVoiceObject())
        mVoice.SetParameter(static_cast<u32>(as32ParamIndex), af32Value, apParamName);
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

            mVoice.Attach(static_cast<s32>(mCreateParams.mSlotName), mContent);
            meUpdateStage = E_UPDATE_STAGE_WAIT;
            // ARTIST checks the content state immediately after attachment.
        }
        case E_UPDATE_STAGE_WAIT:
            if (mContent->GetContentState() !=
                Playback::E_CONTENT_STATE_LOADED)
                break;
            meUpdateStage = E_UPDATE_STAGE_START;
            // ARTIST @ 0x826DC5E0 starts ready content in this update.

        case E_UPDATE_STAGE_START:
            if (mbPlay)
            {
                u32 luSendName = mCreateParams.mSendName;
                mVoice.SetGain(static_cast<u32>(mCreateParams.miSendIndex),
                               0.0f, &luSendName);
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
            CGS_FAIL("Unhandled case: meUpdateStage");
            break;
    }
}

// ARTIST @ 0x826C5270.
void VoiceWrapper::Release()
{
    if (mVoice.IsCreated())
    {
        if (mVoice.IsPlaying())
            mVoice.Stop();
        mVoice.Detach(static_cast<s32>(mCreateParams.mSlotName));
        mVoice.Destruct();
    }

    if (mCreateParams.mContentSpecName && mContent)
        mContent->BeginRemove();
    mContent = Content::ContentHandle();

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

#undef CGS_VOICEWRAPPER_ARTIST_CPP
