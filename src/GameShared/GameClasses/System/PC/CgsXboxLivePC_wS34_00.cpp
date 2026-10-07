// CgsXboxLivePC_wS34_00.cpp -- PC body for the guide's friends-list call (CgsXboxLivePC.cpp family).
// FLAG PC-platform leaf: the Xbox guide is not available on the host.

#include "types.hpp"

namespace
{
    const u32 KU_ERROR_SUCCESS = 0u;
}

// [PC platform leaf] Opens the guide's friends list for a profile; 0, nothing shown.
extern "C" u32 XShowFriendsUI(u32 /*luUserIndex*/)
{
    return KU_ERROR_SUCCESS;
}
