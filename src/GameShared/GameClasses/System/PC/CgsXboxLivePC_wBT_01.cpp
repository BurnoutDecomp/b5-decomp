// CgsXboxLivePC_wBT_01.cpp -- PC bodies for the profile achievement and presence-property calls
// the game-state managers make (CgsXboxLivePC.cpp family).
// FLAG PC-platform leaf: the host has no system achievement store and no profile presence, so
// enumeration fails the way it does without one (the achievement manager closes the enumerator
// and retries next frame), a write is accepted and finishes at once, and a presence property is
// dropped like XUserSetContext's context.

#include "types.hpp"
#include "GameShared/GameClasses/System/CgsXOverlapped.h"   // XOVERLAPPED

namespace
{
    const u32 KU_ERROR_SUCCESS           = 0u;
    const u32 KU_ERROR_INVALID_PARAMETER = 87u;
    const u32 KU_ERROR_IO_PENDING        = 997u;
}

// [PC platform leaf] Creates an enumerator over a user's achievements; 87, no achievement store
// on PC. The buffer size and handle are left as the caller holds them.
extern "C" u32 XUserCreateAchievementEnumerator(u32 /*luTitleId*/, u32 /*luUserIndex*/, u64 /*luXuid*/,
                                                u32 /*luDetailFlags*/, u32 /*luStartingIndex*/,
                                                u32 /*luItems*/, u32* /*lpcbBuffer*/, void** /*lphEnum*/)
{
    return KU_ERROR_INVALID_PARAMETER;
}

// [PC platform leaf] Awards achievements to local users. Asynchronous: 997 (started), and the
// overlapped finishes at once with success; there is no store to record them in.
extern "C" u32 XUserWriteAchievements(u32 /*luNumAchievements*/, const void* /*lpAchievements*/,
                                      void* lpOverlapped)
{
    if (lpOverlapped != 0)
    {
        XOVERLAPPED* lpXOverlapped     = static_cast<XOVERLAPPED*>(lpOverlapped);
        lpXOverlapped->InternalLow     = KU_ERROR_SUCCESS;
        lpXOverlapped->InternalHigh    = 0u;
        lpXOverlapped->dwExtendedError = 0u;
    }
    return KU_ERROR_IO_PENDING;
}

// [PC platform leaf] Sets one of the user's presence properties; no profile presence on PC.
extern "C" void XUserSetProperty(u32 /*luUserIndex*/, u32 /*luPropertyId*/, u32 /*lcbValue*/,
                                 const void* /*lpvValue*/)
{
}
