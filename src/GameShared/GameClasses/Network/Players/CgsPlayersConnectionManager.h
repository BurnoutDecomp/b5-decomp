#pragma once

#include "types.hpp"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"  // BrnNetwork::NetworkPlayerID (== s32)

namespace CgsNetwork
{
class PlayersConnectionManager
{
public:
    PlayersConnectionManager();
    s32 GetConnectionStatus(s32 liPlayerID);
    // Whether every tracked player's connection has completed successfully (the launch
    // "everyone NAT-finalised" gate). ADDITIVE GROW (BrnNetworkLaunchManager TU):
    // declared-only; body is this manager's own TU.
    bool AreAllConnectionsSuccessful() const;

    // ADDITIVE GROW (flagged by the BrnNetworkConnectionManager group). The host NAT-kick
    // walk (X360 UpdateNATData @ 0x8256CFF0) uses these when peers are still connecting:
    //   HavePlayersFailedToConnect() -- did the connection between these two players fail?
    //   GetIDOfPlayerToKick()        -- of a failed pair, which player id to evict.
    // Signatures from DecFIGS DWARF (CgsPlayersConnectionManager.h); NetworkPlayerID == s32.
    // Declared-only here; bodies are homed in this manager's own TU.
    bool HavePlayersFailedToConnect(BrnNetwork::NetworkPlayerID liPlayerID1,
                                    BrnNetwork::NetworkPlayerID liPlayerID2) const;
    BrnNetwork::NetworkPlayerID GetIDOfPlayerToKick(BrnNetwork::NetworkPlayerID liPlayerID1,
                                                    BrnNetwork::NetworkPlayerID liPlayerID2) const;

private:
    struct ConnectionRecord
    {
        u32 muReadyVTable0;
        u8  mPad0[36];
        u32 muReadyVTable1;
        u8  mPad1[36];
        u32 muConnectionState;
        f32 mfConnectionTime;
        u8  mPad2[4];
        u32 muTimerVTable0;
        u8  mPad3[92];
        u32 muTimerVTable1;
        u8  mPad4[324];
    };

    u8               mPad0[252];
    ConnectionRecord maConnections[7];
};
}
