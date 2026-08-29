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
// ⭐⭐ 2026-08-29 (FIX1): THE LAYOUT IS NO LONGER GUESSED IN THE MIDDLE. RecEvent
// @0x82501D28 and its two tails (GenerateRouteData @0x824FA008, RegenerateRouteData
// @0x824F41E0) are bodied now, and between them they load or store nearly every member.
// The old "the record array runs from +0x10 right up to the route-info count word at
// +0x65050" reading is REFUTED (see KI_TRACKER_RECORD_CAPACITY below) -- the records stop
// at +0xC10 and the space the old model claimed is the 64-entry route-record array. The
// three original accessors' offsets all survive the rebuild unchanged, and two of them
// (+0x65050, +0x65060) are now corroborated a second way, which is what makes the new
// shape trustworthy rather than merely newer.
// All access is by name.
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 (TrackerInformation::mv3Position)
// [FIX1 2026-08-29] the flattened route-point array and the per-event route record.
// Same pairing the existing Array<rw::math::vpu::Vector3,5120> instantiation TU uses
// (GameShared/GameClasses/Containers/CgsArrayVpuVector3_5120.cpp).
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "rw/math/vpu/types.h"
// [wave J] TrackerInformation::meIconType is
// GuiEventUpdateSatNav::SatNavIconInfo::SatNavIconType (DWARF BrnGuiTracker.h:59), so the
// complete enum is required here. No cycle: BrnGuiEventTypeDefs.h does not reach this header.
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"

namespace CgsModule { struct Event; }   // GuiTracker::RecEvent payload base (pointer-only)

namespace BrnGui
{
    class GuiCache;   // GuiTracker::mpGuiCache (pointer only; home GameSource/Gui/BrnGuiCache.h)

    class GuiTracker
    {
    public:
        // A route needs at least this many points before the route info is "available".
        static const s32 KI_ROUTE_MIN_POINTS = 2;

        // DWARF BrnGuiTracker.h:53 (`extern const int32_t KI_TRACKER_STACK_SIZE = 64;`).
        // The publish-side capacity: the size of GuiEventSetTracker's record array, the
        // bound RecEvent's case-232 arm asserts ("Invalid number of trackers", `cmplwi
        // count, 0x40`), and the bound GuiCache::HACK_..._SetActiveLandmarksByEventID
        // asserts on its own count ("lSetTrackerEvent.miNumTrackedItems <=
        // GuiTracker::KI_TRACKER_STACK_SIZE", BrnGuiCache.cpp:4227). NOT the same number as
        // KI_TRACKER_RECORD_CAPACITY below, which is the tracker's own inline array size.
        static const s32 KI_TRACKER_STACK_SIZE = 64;

        // ⚠️⚠️ CORRECTED 2026-08-29 (FIX1). This used to read 0x21AC with the reasoning
        // "the records run from +0x10 up to the route-info count word at +0x65050". That
        // inference is REFUTED, and by the very body that was missing when it was written:
        // GuiTracker::RecEvent's case-211 arm memcpy's 5136-byte route records into
        // `this + 3136 + 5136*eventId` (+0xC40..+0x5103F, array end +0x51040), inside the claimed
        // record span. The record array is 64 entries -- exactly KI_TRACKER_STACK_SIZE --
        // and three independent witnesses agree:
        //   * RegenerateRouteData @0x824F41E0 memcpy's `this+0x10` into a 0xC00-byte stack
        //     buffer (`_OWORD v15[192]`), i.e. 64 * 0x30, no more;
        //   * the record at +0xC10 that immediately follows is a SINGLE record with its own
        //     name -- mPlayersTrackerInfo, named by RegenerateRouteData's own assert
        //     "mPlayersTrackerInfo.muTargetSectionId != BrnWorld::KI_INVALID_SECTION_INDEX"
        //     at the +0xC30 store (== +0xC10 + the record's muTargetSectionId at +0x20);
        //   * 0xC10 + 0x30 == 0xC40, the route-record array base to the byte.
        static const s32 KI_TRACKER_RECORD_CAPACITY = KI_TRACKER_STACK_SIZE;   // 64

        // Route records: one per tracked event id, capacity bounded by the same 64 the
        // case-211 assert enforces ("lpRouteInformaton->miEventId >= 0 && ... <
        // KI_TRACKER_STACK_SIZE", BrnGuiTracker.cpp:166).
        static const s32 KI_ROUTE_RECORD_CAPACITY = KI_TRACKER_STACK_SIZE;   // 64

        // Capacity of the flattened route-point array the sat-nav route line draws from
        // (the Array<rw::math::vpu::Vector3, 5120> instantiation that already ships as
        // GameShared/GameClasses/Containers/CgsArrayVpuVector3_5120.cpp, whose banner names
        // GuiTracker::GenerateRouteData as its only user).
        static const u32 KU_ROUTE_POINT_CAPACITY = 5120;

        // One tracker-information record. Stride 0x30 (48). H3a (2026-08-25): the
        // position lane @+0x10 is now named -- SatNavComponent::UpdateFreeRoaming
        // reads it (`lvx128 v0, info, 0x10` @0x8244774C/@0x82447874/@0x8244798C).
        //
        // ⭐ [wave J, GuiCache map-side closure] THE INTERIOR IS NO LONGER OPAQUE. The
        // DecFIGS DWARF (dwarfdump GameSource/Gui/SatNav/BrnGuiTracker.h:58-63) names the
        // record `BrnGui::GuiTracker::TrackerInfo` with exactly five members, and the two
        // X360 producers that fill one -- GuiCache::UpdateTrackerInfo @0x82506F28 and
        // GuiCache::HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID @0x825071C8 --
        // pin three of them by store offset within the proven 0x30 stride:
        //   `stw 4, -0x28(p)`   -> meIconType         @+0x00 (E_SATNAVICON_LANDMARK)
        //   `stvx128 v0, p,-0x18` -> mv3TargetPosition @+0x10 (whole 16-byte lane)
        //   `sth <lm idx>, 0(p)`  -> mTargetLandmarkIndex @+0x28
        // The DWARF's remaining two (muTargetSectionId, mTargetJunctionId) sit between the
        // lane and the landmark half; neither producer writes them, and no X360 reader of
        // them is recovered, so they are named at the DWARF's ORDER but their exact
        // inter-member padding is inferred from the stride, not measured. FLAG: padding
        // boundary, members not offset-proven individually.
        //
        // The TYPE NAME stays `TrackerInformation` (the committed spelling, used by
        // BrnSatNavComponent.cpp and GetTrackerInformation); `TrackerInfo` is added as the
        // DWARF alias so new code can spell it the original way. mv3Position keeps its
        // committed name as an alias member is not possible -- it IS mv3TargetPosition,
        // renamed would break three committed call sites, so the DWARF name is recorded
        // here and the committed name kept. FLAG: name drift vs DWARF, deliberate.
        struct TrackerInformation
        {
            // DWARF :59 meIconType. Stored as the 4-byte enum the producers `stw`.
            GuiEventUpdateSatNav::SatNavIconInfo::SatNavIconType meIconType; // +0x00
            u8      maPad_04[0x10 - 0x04];  // +0x04..+0x0F (the lane's 16-byte alignment)
            Vector3 mv3Position;            // +0x10  DWARF :60 mv3TargetPosition
            u16     muTargetSectionId;      // +0x20  DWARF :61 (unwritten by both producers)
            u8      maPad_22[2];            // +0x22..+0x23
            u32     mTargetJunctionId;      // +0x24  DWARF :62 (unwritten by both producers)
            u16     mTargetLandmarkIndex;   // +0x28  DWARF :63 (LandmarkIndex, `sth`)
            u8      maPad_2A[0x30 - 0x2A];  // +0x2A..+0x2F (pads to the proven 0x30 stride)
        };
        typedef TrackerInformation TrackerInfo;   // the DWARF spelling (:58)

        // -----------------------------------------------------------------------------
        // ⭐ FIX1 (2026-08-29): the 5136-byte route record RecEvent's case-211 arm copies
        // in, one per tracked event id. WHOLLY MEASURED, no invention:
        //   * the stride 0x1410 == 5136 is the memcpy size literal AND the `v4 += 1284`
        //     (int) walk in GenerateRouteData @0x824FA008;
        //   * the point array runs from +0x0000 (GenerateRouteData's cursor starts at
        //     `v4 - 1280` ints == record+0) at a 16-byte stride (`v6 += 4` ints), and is
        //     handed to Array<rw::math::vpu::Vector3,5120>::Append whole-register, so the
        //     element is the 16-byte VPU vector;
        //   * the count word is at +0x1400 (GenerateRouteData reads `*(this + 8256 +
        //     0x1410*i)`, and 8256 - 3136 == 0x1400), which fixes the array at 320 points
        //     (0x1400 / 0x10) exactly;
        //   * miEventId at +0x1404 (`*(a2 + 1281)`, the arm's 0x40 bound check) and
        //     mfRouteDistance at +0x1408 (`*(a2 + 1282)`, accumulated into the tracker's
        //     own mfRouteDistance).
        // FLAG consumer-named: the three field NAMES come from the case-211 assert literal
        // ("lpRouteInformaton->miEventId ...") and from what each value is used for; the
        // DWARF has no row for this type.
        // -----------------------------------------------------------------------------
        static const u32 KU_ROUTE_POINTS_PER_RECORD = 320;   // 0x1400 / 0x10

        struct RouteInformation
        {
            rw::math::vpu::Vector3 mav3Points[KU_ROUTE_POINTS_PER_RECORD]; // +0x0000..+0x13FF
            s32 miNumPoints;        // +0x1400
            s32 miEventId;          // +0x1404
            f32 mfRouteDistance;    // +0x1408
            u8  maTailPad[4];       // +0x140C..+0x140F (the 0x1410 published stride)
        };

        // @ 0x82443EC0 - return the pointer to tracker record `liIndex`
        // (&maTrackerRecords[liIndex]). Asserts 0 <= liIndex < miTrackerCount.
        TrackerInformation* GetTrackerInformation(s32 liIndex);

        // The live record count (X360 inlined `lwz tracker+4` at every caller; the
        // DWARF/assert name). ADDITIVE GROW (H3a: SatNavComponent::UpdateFreeRoaming's
        // last-record lookups).
        s32 GetNumTracked() const { return miTrackerCount; }

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

        // @ 0x82501D28 (DWARF BrnGuiTracker.h:75,
        // `void RecEvent(const CgsModule::Event*, int32_t, int32_t)`). The tracker's event
        // sink: a five-arm switch over the X360 event ids 64 / 165 / 211 / 232 / 233. The
        // third argument is the record's byte size (the X360 passes `li r6, 0xC10` beside
        // `li r5, 0xE8` at every GuiEventSetTracker publish).
        //
        // ⭐ BODIED 2026-08-29 (FIX1), together with the whole three-function slice --
        // GenerateRouteData and RegenerateRouteData below. The old blocker note here said
        // "the 211 arm additionally needs the 5136-byte GuiEventRouteInformation record":
        // that record is now recovered (GuiTracker::RouteInformation above) from the arm's
        // own memcpy plus GenerateRouteData's walk, so nothing in the slice is invented.
        // ⚠️ STILL TO DO, and NOT part of this fix: delete
        // GuiCacheTrackerBoundary::PublishSetTrackerEvent (the LOG-ONCE boundary in
        // GameSource/Gui/BrnGuiCache_wJ_01.cpp) and have the three wave-J GuiCache
        // publishers call this directly -- that is what actually lights the sat-nav route
        // line, and it is a behaviour change that wants its own verification pass.
        void RecEvent(const CgsModule::Event* lpEvent, s32 liEventId, s32 liEventSizeBytes);

        // @ 0x824FA008 -- flatten every received route record's live points into
        // mRoutePoints (in record order), then publish: mbRouteDataPending = false,
        // mbHasRoute = true. Called by RecEvent's 165 and 211 arms.
        void GenerateRouteData();

        // @ 0x824F41E0 -- re-base the tracker set on the player's own record: snapshot the
        // 64 records, fold mPlayersTrackerInfo into slot 0, compact
        // [miCurrentlyTrackedIndex .. miTrackerCount) down to slot 1 onward, and reset the
        // route-receive state. Called by RecEvent's 233 arm.
        void RegenerateRouteData();

    private:
        // ===============================================================================
        // ⭐⭐ LAYOUT REBUILT 2026-08-29 (FIX1), from the three bodies that own it --
        // RecEvent @0x82501D28, GenerateRouteData @0x824FA008, RegenerateRouteData
        // @0x824F41E0. The previous shape (three opaque head bytes, a 0x21AC-entry record
        // array running to +0x65050, a reserved "route-info tail") was an inference made
        // WITHOUT those bodies and is refuted by them; see KI_TRACKER_RECORD_CAPACITY.
        // Every offset below is a store or load one of the three emits. Guest 32-bit
        // offsets; host access is BY NAME.
        // ===============================================================================

        // The four head flag bytes. ClearTracker @0x824FA0A8 writes the first three as
        // three separate `stb`s and deliberately leaves the fourth alone.
        bool mbTrackingActive;      // +0x00  set by RecEvent case 232 when the set is
                                    //        non-empty; case 165 gates on it and clears it
                                    //        when the last tracker is consumed.
        bool mbRouteDataPending;    // +0x01  "more route records still to arrive": case 232
                                    //        sets it to (count > 1), case 211's short arm
                                    //        sets it, GenerateRouteData clears it,
                                    //        RegenerateRouteData sets it. FLAG consumer-named.
        bool mbIsEntireRoute;       // +0x02  copied straight from
                                    //        GuiEventSetTracker::mbIsEntireRoute (+0xC08).
        bool mbHasRoute;            // +0x03  the flag IsRouteInfoAvailable @0x82488ED0
                                    //        gates on. Only GenerateRouteData sets it
                                    //        (`stb 1, 3`); case 232 clears it.

        s32 miTrackerCount;         // +0x04  live tracker-record count (GetNumTracked)
        u8  maPreRecordReserved[0x10 - 0x08];   // +0x08..+0x0F  head pad (record alignment)

        // +0x10..+0xC0F -- exactly 64 records. RegenerateRouteData snapshots this whole
        // span (0xC00 bytes) onto its stack before compacting it in place.
        TrackerInformation maTrackerRecords[KI_TRACKER_RECORD_CAPACITY];

        // +0xC10..+0xC3F -- the local player's own tracker record, ONE record, named by
        // RegenerateRouteData's assert. RecEvent's case-64 arm rewrites it every cache
        // update (icon type 0 at +0xC10, the world-camera lane at +0xC20); case 233 stores
        // the incoming section id into it (+0xC30) and RegenerateRouteData then slides it
        // into maTrackerRecords[0].
        TrackerInformation mPlayersTrackerInfo;

        // +0xC40..+0x5103F -- 64 * 0x1410 (end +0x51040). Indexed by RouteInformation::miEventId.
        RouteInformation maRouteInformation[KI_ROUTE_RECORD_CAPACITY];

        s32 miNumRouteInfoReceived;  // +0x51040  how many route records have arrived since
                                     //           (RecEvent case 211 `*(this+331840)` and
                                     //           GenerateRouteData both read 331840 == 0x51040)
                                     //           the set was published (case 211 counts up,
                                     //           232 and RegenerateRouteData zero it).
        u8  maRoutePointPad[0x51050 - 0x51044];  // +0x51044..+0x5104F  16-byte alignment pad
                                     //           ahead of the VMX point array.

        // +0x51050..+0x65053 -- the flattened route line. GenerateRouteData Appends every
        // live point of every received route record into it. Its own miCount lands at
        // +0x65050 (0x51050 + 5120*16), which is EXACTLY the word IsRouteInfoAvailable
        // reads with the CgsArray "used before Construct/Clear" assert -- so the old
        // `miRouteInfoCount` member was this array's count all along, and the two
        // independently-recovered offsets corroborate each other.
        Array<rw::math::vpu::Vector3, KU_ROUTE_POINT_CAPACITY> mRoutePoints;

        u8  maPostRoutePad[0x65060 - 0x65054];   // +0x65054..+0x6505F

        s32 miCurrentlyTrackedIndex; // +0x65060  index into maTrackerRecords of the tracker
                                     //           the player is currently heading for; -1 ==
                                     //           none (ClearTracker's `stwx -1` target, and
                                     //           RegenerateRouteData's assert names it).
        s32 muPlayerTargetSectionId; // +0x65064  latched by case 233, consumed by
                                     //           RegenerateRouteData into
                                     //           mPlayersTrackerInfo.muTargetSectionId.
        f32 mfRouteDistance;         // +0x65068  live route distance (lfsx)
        u8  maTailReserved[0x650EC - 0x6506C];   // +0x6506C..+0x650EB  unread by this slice
        GuiCache* mpGuiCache;        // +0x650EC  latched ONCE by case 64 (first non-null
                                     //           cache pointer wins).
    };

    // The record stride is load-bearing (it reproduces the X360 48-byte stride).
    static_assert(sizeof(GuiTracker::TrackerInformation) == 0x30, "GuiTracker record stride");

    // [FIX1] The route record's stride is the case-211 memcpy size literal AND
    // GenerateRouteData's per-record walk; pin it the way the tree pins every other
    // measured stride.
    static_assert(sizeof(GuiTracker::RouteInformation) == 0x1410,
                  "GuiTracker::RouteInformation is the 5136-byte record RecEvent memcpy's");
    static_assert(sizeof(rw::math::vpu::Vector3) == 0x10,
                  "the route point is the 16-byte VMX register GenerateRouteData Appends");

    // ===============================================================================
    // BrnGui::GuiEventSetTracker -- the "here is the whole tracked set" payload.
    //   DWARF home: this header (dwarfdump BrnGuiTracker.h:191,
    //   `struct GuiEventSetTracker : public GuiEvent<229>`), members verbatim:
    //     GuiTracker::TrackerInfo mTrackedDataInfo[64];
    //     int32_t miNumTrackedItems;
    //     int32_t miCurrentlyTrackedIndex;
    //     bool    mbIsEntireRoute;
    //
    // ADDITIVE GROW (wave J, GuiCache map-side closure). Three X360 producers build one on
    // their stack and publish it with `RecEvent(&record, 232, 3088)`:
    //   GuiCache::UpdateTrackerInfo @0x82506F28, the un-named sub_82507070 twin, and
    //   GuiCache::HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID @0x825071C8.
    //
    // ⭐ THE LAYOUT IS PINNED BY THOSE PRODUCERS AND BY GuiTracker::RecEvent's case-232 arm,
    // which reads the SAME three trailing fields back at the same offsets:
    //   record base .. +0xBFF  mTrackedDataInfo[64]  (64 * 0x30 == 0xC00, the stride proven
    //                          by the producers' `addi p, p, 0x30` per element)
    //   +0xC00  miNumTrackedItems        (`stw`; RecEvent bounds it against 0x40 and copies
    //                                     exactly this many 48-byte records to tracker+0x10)
    //   +0xC04  miCurrentlyTrackedIndex  (`stw`; RecEvent stores it at tracker+0x65060)
    //   +0xC08  mbIsEntireRoute          (`stb 1`; RecEvent stores it at tracker+0x02)
    // and the published size literal is 0xC10 == 3088, which is 0xC08 rounded to the record's
    // 8-byte alignment -- so the struct below reproduces the console size exactly.
    //
    // ⛔ NOT DERIVED FROM CgsGui::GuiEvent<229>, for the same X360-proven reason
    // GuiEventSetActiveLandmarks is not (see its banner in BrnGuiEventTypeDefs.h): this
    // tree's GuiEvent<N> carries a real 12-byte header, and every producer passes the
    // record's base as the first tracked item. The id travels as the RecEvent argument.
    // ⚠️ ID DRIFT, X360 WINS: the PS3 DWARF says 229; the X360 publishes 232 (`li r5, 0xE8`)
    // and RecEvent's switch arm is `case 232`. GetEventType() below returns the X360 value.
    // ===============================================================================
    struct GuiEventSetTracker
    {
        GuiTracker::TrackerInformation mTrackedDataInfo[GuiTracker::KI_TRACKER_STACK_SIZE];
        s32  miNumTrackedItems;         // +0xC00
        s32  miCurrentlyTrackedIndex;   // +0xC04
        bool mbIsEntireRoute;           // +0xC08

        s32 GetEventType() const { return 232; }   // X360 `li r5, 0xE8`
    };

    // The published byte count is the console's own literal.
    static_assert(sizeof(GuiEventSetTracker) == 0xC10,
                  "GuiEventSetTracker is the 3088-byte record RecEvent is handed");
}
