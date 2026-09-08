#pragma once
#include "types.hpp"
#include "GameSource/GameState/BrnCgsPlayerName.h"

namespace BrnNetwork { namespace BrnNetworkModuleIO {
// BrnNetworkSharedIO.h:599 (DecFIGS); ARTIST friend records have a 132-byte stride.
struct BuddyInformation
{
    s32 miInviteStatus;
    s32 miTotalMessages;
    s32 miUnreadMessages;
    CgsNetwork::PlayerName mPlayerName;
    bool mbIsFullBuddy;
    bool mbIsOnline;
    bool mbIsInSameLobby;
    bool mbIsJoinable;
    char macPresenceData[100];
};
static_assert(sizeof(BuddyInformation) == 132, "ARTIST buddy record");
}}
