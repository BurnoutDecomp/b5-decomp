// ============================================================================
// pingmanagerpc.cpp -- PingManager for the PC build.
//
// [PC platform layer] The game pings the online service's region servers to pick a
// matchmaking region. The PC build has no region servers in either mode: a ping cannot be
// sent (-1), so the CGS ping-regions component records the region as unreachable and moves
// on. The manager is a plain ref (DirtyMemAlloc / DirtyMemFree); no request is ever pending.
// ============================================================================

#include "pingmanager.h"
#include "dirtymem.h"   // DirtyMemAlloc / DirtyMemFree / DirtyMemGroupQuery

#include <cstring>      // memset

namespace
{
    // DirtyMem module id.
    const s32 KI_MEMID_PINGMANAGER = ('l' << 24) | ('p' << 16) | ('m' << 8) | 'g';

    // Smallest record cache PingManagerCreate accepts.
    const u32 KU_PINGMANAGER_MIN_RECORDS = 16;
}

struct PingManagerRefT
{
    s32 iMemGroup;
    u32 uMaxRecords;
};

extern "C" PingManagerRefT* PingManagerCreate(u32 uMaxRecords)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    PingManagerRefT* pPingManager =
        static_cast<PingManagerRefT*>(DirtyMemAlloc(sizeof(PingManagerRefT), KI_MEMID_PINGMANAGER, iMemGroup));
    if (pPingManager == NULL)
    {
        return NULL;
    }
    memset(pPingManager, 0, sizeof(*pPingManager));
    pPingManager->iMemGroup   = iMemGroup;
    pPingManager->uMaxRecords = (uMaxRecords < KU_PINGMANAGER_MIN_RECORDS) ? KU_PINGMANAGER_MIN_RECORDS : uMaxRecords;
    return pPingManager;
}

extern "C" void PingManagerDestroy(PingManagerRefT* pPingManager)
{
    DirtyMemFree(pPingManager, KI_MEMID_PINGMANAGER, pPingManager->iMemGroup);
}

extern "C" s32 PingManagerPingServer2(PingManagerRefT* /*pPingManager*/, u32 /*uServerAddress*/,
                                      PingManagerCallbackT* /*pCallback*/, void* /*pCallbackData*/)
{
    // No ping service: the request cannot be sent.
    return -1;
}

extern "C" void PingManagerCancelServerRequest(PingManagerRefT* /*pPingManager*/, u32 /*uSeqn*/)
{
    // No request is ever pending.
}
