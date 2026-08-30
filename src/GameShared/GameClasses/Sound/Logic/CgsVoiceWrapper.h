#ifndef CGS_SOUND_LOGIC_CGSVOICEWRAPPER_H
#define CGS_SOUND_LOGIC_CGSVOICEWRAPPER_H

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Logic/CgsVoice.h"
#include "GameShared/GameClasses/Sound/Logic/CgsContent.h"

namespace CgsSound
{
namespace Logic
{

// DecFIGS fixes this declaration shape; ARTIST CgsVoice.cpp fixes the state
// machine. The console object is 0x50 bytes behind 32-bit pointers.
class VoiceWrapper
{
public:
    enum E_UPDATE_STAGE
    {
        E_UPDATE_STAGE_IDLE = 0,
        E_UPDATE_STAGE_CREATE = 1,
        E_UPDATE_STAGE_CREATE_WAIT = 2,
        E_UPDATE_STAGE_CONNECT = 3,
        E_UPDATE_STAGE_WAIT = 4,
        E_UPDATE_STAGE_START = 5,
        E_UPDATE_STAGE_PLAYING = 6,
        E_UPDATE_STAGE_FINISHED = 7
    };

    struct AbstractFunctionPointer
    {
        virtual void Call(VoiceWrapper& arVoice) = 0;
    };

    struct CreateParams
    {
        CreateParams() { Clear(); }
        void Clear()
        {
            mpLogicModule = 0;
            mpOnPostInit = 0;
            mFactoryName = 0;
            mVoiceSpecName = 0;
            mpContent = 0;
            mContentSpecName = 0;
            mSlotName = 0;
            mSendName = 0;
            mSubMixVoiceID = 0;
            mReverbSendName = 0;
            mReverbSubMixVoiceID = 0;
            miSendIndex = -1;
        }

        Module* mpLogicModule;
        AbstractFunctionPointer* mpOnPostInit;
        Command::QueueElement mFactoryName;
        Command::QueueElement mVoiceSpecName;
        const Content* mpContent;
        Command::QueueElement mContentSpecName;
        Command::QueueElement mSlotName;
        Command::QueueElement mSendName;
        Command::QueueElement mSubMixVoiceID;
        Command::QueueElement mReverbSendName;
        Command::QueueElement mReverbSubMixVoiceID;
        s32 miSendIndex;
    };

    VoiceWrapper()
        : mCreateParams(), mVoice(), mContent(), mu32OptionalPlayParam(0),
          meUpdateStage(E_UPDATE_STAGE_IDLE), mbPlay(false) {}
    virtual ~VoiceWrapper();

    void Create(const CreateParams& arCreateParams);
    void Play(u32 au32OptionalPlayParam);
    void Stop();
    void Update();
    void Release();

    void SetGain(u32 au32SendIndex, f32 af32Gain, const u32* apSendName);
    f32 GetGain(const s32* apSendName) const;
    void SetParameter(s32 as32ParamIndex, f32 af32Value, const u32* apParamName);

    Voice& GetVoice() { return mVoice; }
    const Voice& GetVoice() const { return mVoice; }
    bool HasLiveVoice() const { return mVoice.GetVoiceObject() != 0; }
    E_UPDATE_STAGE GetUpdateStage() const { return meUpdateStage; }
    s32 GetState() const { return static_cast<s32>(meUpdateStage); }
    void SetState(s32 as32Stage)
    {
        meUpdateStage = static_cast<E_UPDATE_STAGE>(as32Stage);
    }
    bool IsAlive() const { return meUpdateStage != E_UPDATE_STAGE_IDLE; }
    bool IsPlaying() const { return meUpdateStage == E_UPDATE_STAGE_PLAYING; }
    const CreateParams& GetCreateParams() const { return mCreateParams; }
    u32 GetOptionalParam() const { return mu32OptionalPlayParam; }

    void ResetDeferredState() { Release(); mCreateParams.Clear(); }

private:
    CreateParams mCreateParams;             // X360 +0x04
    Voice mVoice;                           // X360 +0x34
    Content mContent;                       // X360 +0x40
    u32 mu32OptionalPlayParam;              // X360 +0x44
    E_UPDATE_STAGE meUpdateStage;           // X360 +0x48
    bool mbPlay;                            // X360 +0x4c
};

} // namespace Logic
} // namespace CgsSound

#endif
