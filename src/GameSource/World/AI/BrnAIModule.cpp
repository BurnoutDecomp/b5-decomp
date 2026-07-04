#include "BrnAIModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnAI
{

// X360 0x82765A10 -- BrnAI::operator++(AIModule::EPrepareStage&, int)
// DWARF BrnAIModule.h:417. Post-increment over the AI prepare-stage enum: caches
// the current stage, advances it by one, asserts the result has not walked past
// E_PREPARESTAGE_DONE (5), and returns the pre-increment value. Called by
// AIModule::Prepare to step through the multi-frame prepare state machine.
AIModule::EPrepareStage operator++(AIModule::EPrepareStage& leEnumIndex, int)
{
    AIModule::EPrepareStage leOldValue = leEnumIndex;
    leEnumIndex = static_cast<AIModule::EPrepareStage>(static_cast<int>(leEnumIndex) + 1);
    CGS_ASSERT(leEnumIndex <= AIModule::E_PREPARESTAGE_DONE,
               "leEnumIndex <= AIModule::E_PREPARESTAGE_DONE");
    return leOldValue;
}

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
