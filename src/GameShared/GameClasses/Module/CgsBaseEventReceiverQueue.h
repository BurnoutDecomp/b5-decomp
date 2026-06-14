#pragma once

#include "types.hpp"

namespace CgsModule
{
class BaseEventReceiverQueue
{
public:
    BaseEventReceiverQueue* Clear();

private:
    u32 muBaseOffset;
    s32 miReadOffset;
    s32 miCount;
    s32 miWriteOffset;
    u32 muPad0;
    s32 miCapacity;
};
}
