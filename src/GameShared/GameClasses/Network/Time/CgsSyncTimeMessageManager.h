#pragma once

#include "types.hpp"

namespace CgsNetwork
{
class SyncTimeMessageManager
{
public:
    SyncTimeMessageManager();

private:
    struct SyncTimeRecord
    {
        u32 muMessageVTable;
        u8  mPad0[28];
        u32 muSendTicks;
        f32 mfSendTime;
        u32 muReceiveTicks;
        f32 mfReceiveTime;
        u8  mPad1[8];
    };

    u8             mPad0[28];
    SyncTimeRecord maRecords[14];
};
}
