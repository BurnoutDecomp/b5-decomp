#pragma once

#include "types.hpp"
#include "GameSource/World/AI/Route/BrnRouteMapModule.h"

#include <eathread/eathread_rwmutex.h>

namespace BrnAI
{
class AIModule
{
public:
    AIModule();

private:
    struct RouteRequestSlot
    {
        s32 miRouteId;
        u8  mPad0[32];
    };

    u32                 muBaseVTable;
    EA::Thread::RWMutex mInputMutex;
    EA::Thread::RWMutex mOutputMutex;
    u8                  mPad0[252520];
    s32                 miActiveRouteRequest;
    u8                  mPad1[18088];
    s32                 miPendingRouteRequest;
    u8                  mPad2[12];
    u32                 muRouteRequestVTable;
    u8                  mPad3[56];
    RouteRequestSlot    maRouteRequestSlots[15];
    u8                  mPad4[8];
    u32                 muOpenListVTable;
    u8                  mPad5[68];
    u32                 muClosedListVTable;
    u8                  mPad6[7244];
    u32                 muScratchListVTable;
    u8                  mPad7[7832];
    s32                 miLastRouteId;
    u8                  mPad8[300];
    u32                 muAnchorState;
    u32                 muAnchorPrev;
    u32                 muAnchorNext;
    void*               mpAnchorHead;
    void*               mpAnchorTail;
    void*               mpAnchorCursor;
    u32                 muAnchorFlags;
    u8                  mPad9[452];
    u32                 muAllocatorVTable;
    u8                  mPad10[8420];
    RouteMapModule      mRouteMapModule;
    s32                 miWorldRouteRequest;
};
}
