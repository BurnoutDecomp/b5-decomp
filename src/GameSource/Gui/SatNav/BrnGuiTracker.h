#pragma once

// ===================================================================================
// BrnGui::GuiTracker  -- owning header
//   b5-decomp/src/GameSource/Gui/SatNav/BrnGuiTracker.h
//
// The sat-nav "tracker" model: it carries a fixed-capacity list of tracked-point
// records (the "tracker information" records) plus the active route-info array's live
// point count and the live route distance. No prior reconstruction carries this model's
// full member layout and there is no DecFIGS DWARF for this TU, so the shape is recovered
// from the three X360 accessors that own it:
//
//   GetTrackerInformation @ 0x82443EC0 - bounds-checks the tracker index against the
//       live count (a 32-bit word at this+0x04), firing the out-of-range assert
//       (BrnGuiTracker.h:279) on failure, then returns the pointer to record `index`:
//       this + 48*index + 0x10. The 48-byte (0x30) record stride and the +0x10 base are
//       fixed by the asm (slwi/add/slwi/addi 0x10), placing the record array at +0x10.
//   IsRouteInfoAvailable @ 0x82488ED0 - returns false unless a "has route" byte at
//       this+0x03 is set; then it reads the route-info array's live-point count word at
//       this+0x65050, asserting it was Construct/Clear'd (count != -1, CgsArray.h:336)
//       and returns true only when the count is >= 2 (a route needs at least 2 points).
//   GetRouteDistance @ 0x82488F58 - asserts IsRouteInfoAvailable() (BrnGuiTracker.h:371)
//       then returns the f32 route distance at this+0x65068 (lfsx), widened to double.
//
// The record interior (the parts these accessors do not read) is held opaque, sized to
// the attested 0x30 stride; the record array runs from +0x10 right up to the route-info
// count word at +0x65050 (an exact 0x30 multiple -> no interior gap). The route-info
// array's element body between its count word and the distance is a reserved span. All
// access is by name.
// ===================================================================================

#include "types.hpp"

namespace BrnGui
{
    class GuiTracker
    {
    public:
        // A route needs at least this many points before the route info is "available".
        static const s32 KI_ROUTE_MIN_POINTS = 2;

        // Capacity of the inline tracker-record array. Layout-derived: the records run
        // from +0x10 up to the route-info count word at +0x65050 ((0x65050-0x10)/0x30 ==
        // 0x21AC exactly), not a claimed game constant.
        static const s32 KI_TRACKER_RECORD_CAPACITY = 0x21AC;   // (0x65050 - 0x10) / 0x30

        // One tracker-information record. Stride 0x30 (48); GetTrackerInformation returns
        // a pointer to the whole record, so its interior is held opaque.
        struct TrackerInformation
        {
            u8  maStorage[0x30];   // +0x00..+0x2F  unrecovered record interior
        };

        // @ 0x82443EC0 - return the pointer to tracker record `liIndex`
        // (&maTrackerRecords[liIndex]). Asserts 0 <= liIndex < miTrackerCount.
        TrackerInformation* GetTrackerInformation(s32 liIndex);

        // @ 0x82488ED0 - true when a route of >= 2 points is loaded. Returns false when
        // the "has route" flag (mbHasRoute) is clear; otherwise asserts the route-info
        // array was Construct/Clear'd and returns (count >= 2).
        bool IsRouteInfoAvailable();

        // @ 0x82488F58 - route distance (f32 at +0x65068) as a double. Asserts
        // IsRouteInfoAvailable().
        double GetRouteDistance();

        // @ 0x824FA0A8 - drop the current route/tracker state. Declared-only here; the body
        // belongs to the BrnGuiTracker TU. Called by
        // OnlineGameRoomPlayerInfo::HandleGuiCacheEvent's showtime-completion arm via
        // mpGuiCache->GetGuiTracker()->ClearTracker(). It is a real side effect with a real
        // X360 body, so it must be called by name rather than stood in with a no-op shim
        // (BrnInGame.cpp carries such a shim, TrackerClearTracker -- that debt should shrink).
        void ClearTracker();

    private:
        // ---- recovered layout (guest 32-bit offsets) -----------------------------------
        u8  maHeadReserved[0x03];        // +0x00..+0x02  vtable + early head
        u8  mbHasRoute;                  // +0x03         has-route flag (lbz 3)
        s32 miTrackerCount;              // +0x04         live tracker-record count (lwz 4)
        u8  maPreRecordReserved[0x10 - 0x08];  // +0x08..+0x0F  head padding before records

        TrackerInformation maTrackerRecords[KI_TRACKER_RECORD_CAPACITY];
                                         // +0x10..+0x6504F  (0x21AC * 0x30)

        s32 miRouteInfoCount;            // +0x65050      route-info array live-point count
                                         //               (-1 == unconstructed sentinel)
        u8  maRouteInfoTail[0x65068 - 0x65054];
                                         // +0x65054..+0x65067  route-info array body
        f32 mfRouteDistance;             // +0x65068      live route distance (lfsx)
    };

    // The record stride is load-bearing (it reproduces the X360 48-byte stride).
    static_assert(sizeof(GuiTracker::TrackerInformation) == 0x30, "GuiTracker record stride");
}
