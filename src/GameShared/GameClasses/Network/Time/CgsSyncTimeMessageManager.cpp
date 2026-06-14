#include "CgsSyncTimeMessageManager.h"

namespace CgsNetwork
{
SyncTimeMessageManager::SyncTimeMessageManager()
{
    for (SyncTimeRecord& lRecord : maRecords)
    {
        lRecord.muMessageVTable = 0x820CE458;
        lRecord.muSendTicks = 0;
        lRecord.mfSendTime = 0.0f;
        lRecord.muReceiveTicks = 0;
        lRecord.mfReceiveTime = 0.0f;
    }
}
}
