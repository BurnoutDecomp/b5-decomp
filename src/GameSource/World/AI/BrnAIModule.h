#pragma once

#include "types.hpp"
#include "GameSource/World/AI/Route/BrnRouteMapModule.h"

#include <eathread/eathread_rwmutex.h>

namespace BrnAI
{
struct Route;

class AIModule
{
public:
    AIModule();

    // Declared-only accessors needed by RouteMapDebugComponent::RenderHUD (its own TU). Bodies live
    // in the AIModule TU. X360 offsets (read in RouteMapDebugComponent::RenderHUD @0x827940A0):
    //   - the master route is a Route embedded at this+0x46B60 (289632); GetMasterRoute returns it.
    //   - the default A* distance-function enum is at this+0x424A8 (271528); the X360 indexes the
    //     KAPC_ASTAR_DISTANCE_FUNCTION_NAMES table with it -- GetDefaultDistanceFunctionName returns
    //     the matching name string.
    const Route* GetMasterRoute() const;                  // this + 0x46B60
    const char*  GetDefaultDistanceFunctionName() const;  // KAPC_..._NAMES[*(this + 0x424A8)]

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
