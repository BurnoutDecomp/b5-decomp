#include "SDKs/Realmc/RealmcIfaceSetActiveCardTask.h"

#include "SDKs/Realmc/RealmcCore.h"
#include "SDKs/Realmc/RealmcCardData.h"
#include "SDKs/Realmc/RealmcIfaceMessageSetActiveCardDone.h"
#include "SDKs/Realmc/RealmcMessageQueue.h"

#include <new>

namespace RealmcIface
{

SetActiveCardTask::SetActiveCardTask(RealmcCore::MessageQueue* pMessageQueue,
                                     RealmcCore::MemcardState* pMemcardState,
                                     XenonUtil::State* pState,
                                     const GameInfo* pGameInfo,
                                     s32 iField18, s32 iField1C, s32 iField20, char cField24)
    : XenonRunnableTask(pMessageQueue, pMemcardState, pState)
    , mpGameInfo(pGameInfo)
    , miField18(iField18)
    , miField1C(iField1C)
    , miField20(iField20)
    , mcField24(cField24)
{
}

SetActiveCardTask::~SetActiveCardTask()
{
}

// ARTIST SetActiveCardTask::Execute @0x82B55018.
void SetActiveCardTask::Execute()
{
    if (miField18 == 0 || mpState->mCardData == CardData::Empty())
        mpState->mCardData = CardData::Empty();

    if (mcField24)
    {
        if (miField20 == 0x100 || miField20 == 0x200)
            mpState->muContentFlags = static_cast<DWORD>(miField20);
        else
            mpState->muContentFlags = 0;
    }

    void* const lpMemory = RealmcCore::AllocateMem(0, sizeof(MessageSetActiveCardDone));
    MessageSetActiveCardDone* const lpMessage = lpMemory
        ? new (lpMemory) MessageSetActiveCardDone(0, 0)
        : 0;

    {
        const RealmcCore::MessagePtr lMessage(lpMessage);
        const RealmcCore::ResponsePtr lResponse = SendMessage(lMessage, 0);
    }
}

} // namespace RealmcIface
