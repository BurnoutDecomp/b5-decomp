// ===================================================================================
// BrnGui::GuiTracker  -- implementation
//   class:BrnGui::GuiTracker
//
// Reconstructed store-for-store from the X360 ARTIST build. Member-by-name access; the
// tracker-information record interior and the route-info array body are held opaque in
// the owning header (their full element layout is not recovered by these accessors), so
// the accessor returns a pointer to the named record element.
// ===================================================================================
#include "GameSource/Gui/SatNav/BrnGuiTracker.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{
    // @ 0x82443EC0 - bounds-checked pointer to tracker record `liIndex`.
    // The X360 tests both ends (liIndex < 0 || liIndex >= miTrackerCount), streaming the
    // dynamic "Invalid tracker index - out of range: <i> :: <n>" message into the assert
    // buffer (BrnGuiTracker.h:279) on failure; the house assert forwards the static text.
    // Then returns this + 48*liIndex + 0x10 (slwi/add/slwi address arithmetic) ==
    // &maTrackerRecords[liIndex].
    GuiTracker::TrackerInformation* GuiTracker::GetTrackerInformation(s32 liIndex)
    {
        CGS_ASSERT(liIndex >= 0 && liIndex < miTrackerCount,
                   "Invalid tracker index - out of range");

        return &maTrackerRecords[liIndex];
    }

    // @ 0x82488ED0 - route-info availability. Returns false when the has-route flag at
    // +0x03 is clear (early `lbz 3` / `beq`). Otherwise reads the route-info array's
    // live-count word at +0x65050, asserting it was Construct/Clear'd (count != -1,
    // CgsArray.h:336), and returns true only when the count is >= 2 (the `cmplwi 2` /
    // `bge` -> 1, else 0). The X360 returns the 0/1 in the low byte (clrlwi 24).
    bool GuiTracker::IsRouteInfoAvailable()
    {
        if (!mbHasRoute)
        {
            return false;
        }

        CGS_ASSERT(miRouteInfoCount != -1,
                   "Array used before Construct/Clear was called");

        return static_cast<u32>(miRouteInfoCount) >= static_cast<u32>(KI_ROUTE_MIN_POINTS);
    }

    // @ 0x82488F58 - route distance. Asserts IsRouteInfoAvailable() (BrnGuiTracker.h:371)
    // then loads the f32 at +0x65068 (lfsx) and returns it in f1 (widened to double by
    // the float-return convention).
    double GuiTracker::GetRouteDistance()
    {
        CGS_ASSERT(IsRouteInfoAvailable(), "IsRouteInfoAvailable()");

        return mfRouteDistance;
    }
}
