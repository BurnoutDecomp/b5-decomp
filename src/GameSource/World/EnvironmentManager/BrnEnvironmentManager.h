#pragma once

#include "types.hpp"

namespace rw
{
namespace core
{
namespace filesys
{
class AsyncOp
{
public:
    AsyncOp();
};
}
}
}

namespace BrnWorld
{
namespace EnvironmentSettings
{
namespace
{
u32 Ptr32(const void* lpAddress)
{
    return static_cast<u32>(reinterpret_cast<usize>(lpAddress));
}
}

class EnvironmentManager
{
public:
    EnvironmentManager();

    struct IntrusiveListHead
    {
        u32 muHead;
        u32 muTail;
        u32 muCount;
        u32 muSentinel;
        u32 muPrev;
        u32 muNext;
        u32 muOwner;
        u32 muPad1C;

        void Construct()
        {
            muHead = 0;
            muTail = 0;
            muCount = 0;
            muSentinel = Ptr32(this);
            muPrev = Ptr32(this);
            muNext = Ptr32(this);
            muOwner = 0;
        }
    };

private:
    u8                mPad0[0x424];
    IntrusiveListHead mReceiverQueue;
    u8                mPad444[0x4C];
    IntrusiveListHead maSeasonDependencyQueues[2];
    u8                mPad4D0[0xCA4];
    IntrusiveListHead mFileRequestQueue;
    u8                mPad1194[0x34];
    rw::core::filesys::AsyncOp mAsyncOp;
};

static_assert(sizeof(EnvironmentManager::IntrusiveListHead) == 0x20, "IntrusiveListHead layout drift");

EnvironmentManager::EnvironmentManager()
{
    mReceiverQueue.Construct();

    for (s32 liIndex = 0; liIndex < 2; ++liIndex)
        maSeasonDependencyQueues[liIndex].Construct();

    mFileRequestQueue.Construct();
}
}
}
