#include "SDKs/Realmc/RealmcIfaceSetAutosaveTask.h"

#include "SDKs/Realmc/RealmcCore.h"
#include "SDKs/Realmc/RealmcMemcardState.h"
#include "SDKs/Realmc/RealmcIfaceSaveCheckTask.h"
#include "SDKs/Realmc/RealmcIfaceMessageSetAutosaveDone.h"
#include "SDKs/Realmc/RealmcMessageQueue.h"

#include <new>

namespace RealmcIface
{

SetAutosaveTask::SetAutosaveTask(RealmcCore::MessageQueue* pMessageQueue,
                                 RealmcCore::MemcardState* pMemcardState,
                                 XenonUtil::State* pState,
                                 const GameInfo* pGameInfo,
                                 s32 iEnabled,
                                 const SaveCheckParams& rParams)
    : XenonRunnableTask(pMessageQueue, pMemcardState, pState)
    , mpGameInfo(pGameInfo)
    , miEnabled(iEnabled)
    , mParams(rParams.mCount, rParams.mppReqs)
{
}

SetAutosaveTask::~SetAutosaveTask()
{
}

// ARTIST SetAutosaveTask::Execute @0x82B56CB0.
void SetAutosaveTask::Execute()
{
    s32 liResult = 0;
    s32 liCheckResult = 0;
    s32 liEnabled = miEnabled;

    mpMemcardState->SetAutosaveState(liEnabled == 1);

    if (miEnabled == 1)
    {
        if (mParams.mCount > 0)
        {
            SaveCheckTask lSaveCheckTask(mpMessageQueue, mpMemcardState, mpState,
                                         mpGameInfo, mParams);
            lSaveCheckTask.InvokeSynchronously();
            liResult = lSaveCheckTask.miField20;
            liCheckResult = lSaveCheckTask.miField24;
        }

        if (liCheckResult == 0 || liCheckResult == 3)
        {
            liCheckResult = 0;
        }
        else
        {
            liEnabled = 0;
        }

        mpMemcardState->SetAutosaveState(liEnabled == 1);

        const int liVerifyActiveCard = VerifyActiveCard();
        if (liVerifyActiveCard)
            liCheckResult = liVerifyActiveCard;
    }

    const s32 liCurrentAutosave = mpMemcardState->GetAutosaveState() != 0;
    if (miEnabled != liCurrentAutosave)
        liResult = 1;

    mpState->mbFieldD8 = static_cast<u8>(mpMemcardState->GetAutosaveState());

    void* const lpMemory = RealmcCore::AllocateMem(0, sizeof(MessageSetAutosaveDone));
    MessageSetAutosaveDone* const lpMessage = lpMemory
        ? new (lpMemory) MessageSetAutosaveDone(static_cast<u32>(liResult),
                                               static_cast<u32>(liCheckResult),
                                               static_cast<u32>(liCurrentAutosave))
        : 0;

    {
        const RealmcCore::MessagePtr lMessage(lpMessage);
        const RealmcCore::ResponsePtr lResponse = SendMessage(lMessage, 0);
    }
}

} // namespace RealmcIface
