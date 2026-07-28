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

    // @ 0x824FA0A8 - drop the current route / tracker state. A straight-line, branch-free,
    // call-free store sequence (blr right after the last store); reproduced store-for-store
    // in the X360 order:
    //
    //   lfs   f0, flt_82001CC0   /  stfsx f0, r3, 0x65068  -> mfRouteDistance    = 0.0f
    //   li    r11, 0             /  stwx  r11, r3, 0x65050 -> miRouteInfoCount   = 0
    //                               stb   r11, 0(r3)       -> head byte 0        = 0
    //                               stb   r11, 1(r3)       -> head byte 1        = 0
    //                               stb   r11, 2(r3)       -> head byte 2        = 0
    //                               stw   r11, 4(r3)       -> miTrackerCount     = 0
    //   li    r10, -1            /  stwx  r10, r3, 0x65060 -> word @+0x65060     = -1
    //
    // Notes on the two spans this has to reach through, both held opaque by the owning
    // header (BrnGuiTracker.h), which recovered only the offsets its three accessors read:
    //
    //  * The three `stb 0` at +0x00/+0x01/+0x02 land in maHeadReserved. They are three
    //    separate byte flags, not a vtable pointer: the X360 writes single bytes into them,
    //    and IsRouteInfoAvailable reads a fourth byte flag (mbHasRoute) immediately after at
    //    +0x03. ClearTracker deliberately does NOT clear mbHasRoute -- the has-route flag
    //    survives the clear.
    //  * The `stw -1` at +0x65060 lands inside maRouteInfoTail, the reserved span between
    //    the route-info count word (+0x65050) and the route distance (+0x65068). -1 is the
    //    same "used before Construct/Clear was called" sentinel the CgsArray count carries
    //    (see the assert in IsRouteInfoAvailable), so this word is very likely a second
    //    array's count being returned to the unconstructed state -- but no X360 reader of
    //    +0x65060 is recovered, so it is left unnamed rather than given an invented name.
    //    It is written through the named reserved member; -1 is all-ones, so the byte-wise
    //    store reproduces the 32-bit store exactly on either host endianness.
    void GuiTracker::ClearTracker()
    {
        mfRouteDistance  = 0.0f;   // stfsx flt_82001CC0 (0.0f), r3, 0x65068
        miRouteInfoCount = 0;      // stwx  0,  r3, 0x65050

        maHeadReserved[0] = 0;     // stb   0,  0(r3)
        maHeadReserved[1] = 0;     // stb   0,  1(r3)
        maHeadReserved[2] = 0;     // stb   0,  2(r3)

        miTrackerCount = 0;        // stw   0,  4(r3)

        // stwx -1, r3, 0x65060 -- see the note above.
        const s32 KI_TAIL_SENTINEL_BYTE = 0x65060 - 0x65054;   // offset within maRouteInfoTail
        static_assert(KI_TAIL_SENTINEL_BYTE + 4 <= sizeof(maRouteInfoTail),
                      "the +0x65060 word lies inside the reserved route-info span");
        for (s32 liByte = 0; liByte < 4; ++liByte)
        {
            maRouteInfoTail[KI_TAIL_SENTINEL_BYTE + liByte] = 0xFFu;
        }
    }
}
