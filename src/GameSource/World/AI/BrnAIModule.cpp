#include "BrnAIModule.h"

namespace BrnAI
{
AIModule::AIModule()
    : mInputMutex(nullptr, true),
      mOutputMutex(nullptr, true),
      muBaseVTable(0x820D0D98),
      miActiveRouteRequest(-1),
      miPendingRouteRequest(-1),
      muRouteRequestVTable(0x820CDF80),
      muOpenListVTable(0x820CE988),
      muClosedListVTable(0x820CDFA0),
      muScratchListVTable(0x820CDFA0),
      miLastRouteId(-1),
      muAnchorState(0),
      muAnchorPrev(0),
      muAnchorNext(0),
      mpAnchorHead(&muAnchorState),
      mpAnchorTail(&muAnchorState),
      mpAnchorCursor(&muAnchorState),
      muAnchorFlags(0),
      muAllocatorVTable(0x820CF9B4),
      mRouteMapModule(),
      miWorldRouteRequest(-1)
{
    for (RouteRequestSlot& lSlot : maRouteRequestSlots)
    {
        lSlot.miRouteId = -1;
    }
}
}
