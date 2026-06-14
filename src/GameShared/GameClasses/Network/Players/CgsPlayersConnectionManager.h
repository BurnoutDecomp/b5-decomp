#pragma once

#include "types.hpp"

namespace CgsNetwork
{
class PlayersConnectionManager
{
public:
    PlayersConnectionManager();
    s32 GetConnectionStatus(s32 liPlayerID);

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
