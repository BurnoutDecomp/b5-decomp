// XDK size optimization matches the ARTIST bodies at 0x82B56038.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "SDKs/Realmc/RealmcIfaceSaveCheckTask.h"

namespace RealmcIface
{

SaveCheckTask::SaveCheckTask(RealmcCore::MessageQueue* pMessageQueue,
                             RealmcCore::MemcardState* pMemcardState,
                             XenonUtil::State* pState,
                             const GameInfo* pGameInfo,
                             const SaveCheckParams& rParams)
    : XenonRunnableTask(pMessageQueue, pMemcardState, pState)
    , mpGameInfo(pGameInfo)
    , mParams(rParams.mCount, rParams.mppReqs)
    , miField20(4)
    , miField24(0)
{
}

SaveCheckTask::~SaveCheckTask()
{
}

// ARTIST SaveCheckTask::Execute @0x82B56028.
void SaveCheckTask::Execute()
{
    miField20 = 0;
    miField24 = 0;
}

} // namespace RealmcIface
