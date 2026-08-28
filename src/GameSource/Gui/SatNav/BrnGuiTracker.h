#pragma once

// ===================================================================================
// BrnGui::GuiTracker  -- owning header
//   b5-decomp/src/GameSource/Gui/SatNav/BrnGuiTracker.h
//
// The sat-nav "tracker" model: the active tracked-point stack (up to 64 records), the
// player's own tracker record, the per-tracked-leg route-information slots the world
// route bridge fills, and the flattened route-point array + distance the sat-nav route
// highlight reads. No DecFIGS DWARF exists for this TU; the shape below is recovered
// from the X360 bodies that own it:
//
//   GetTrackerInformation @ 0x82443EC0 - record stride 0x30, array base +0x10, count
//       word +0x04 (slwi/add/addi 0x10 address arithmetic + the bounds assert).
//   IsRouteInfoAvailable  @ 0x82488ED0 - has-route byte +0x03, route-point count word
//       +0x65050 (the CgsArray "used before Construct/Clear" sentinel test), >= 2.
//   GetRouteDistance      @ 0x82488F58 - f32 route distance +0x65068.
//   ClearTracker          @ 0x824FA0A8 - the head byte trio +0x00..+0x02, count +0x04,
//       +0x65050 := 0, +0x65060 := -1, distance := 0.
//   RecEvent              @ 0x82501D28 - [map arm 2026-08-27] THE LAYOUT KEY:
//       case 64 writes mPlayersTrackerInfo (+0xC10 head word, +0xC20 position lane from
//       the cache's world-camera lane) and latches the GuiCache pointer (+0x6506C);
//       case 232 copies up to KI_TRACKER_STACK_SIZE (the "Invalid number of trackers"
//       assert, 0x40) 48-byte records to +0x10 and fills the head/count/index words;
//       case 211 memcpy's one 5136-byte route-information record to
//       +0xC40 + 5136 * miEventId (the "miEventId < KI_TRACKER_STACK_SIZE" assert) and
//       accumulates +0x51040 / +0x65068; case 233 stores +0x65064.
//   GenerateRouteData     @ 0x824FA008 - walks the +0xC40 leg array (stride 0x1410,
//       count word at leg+0x1400), appending each leg's points into the
//       Array<rw Vector3, 5120> at +0x51050 (count word +0x65050 == +0x51050 + 0x14000).
//   RegenerateRouteData   @ 0x824F41E0 - names mPlayersTrackerInfo.muTargetSectionId
//       (+0xC30 == player record +0x20) in its own assert string, reads the pending
//       section id from +0x65064 and miCurrentlyTrackedIndex from +0x65060.
//
// All access is by name.
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 (TrackerInformation::mv3Position)

namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;

    class GuiTracker
    {
    public:
        // A route needs at least this many points before the route info is "available".
        static const s32 KI_ROUTE_MIN_POINTS = 2;

        // The tracked-record stack size. Named by the X360's own assert text
        // ("lSetTrackerEvent.miNumTrackedItems <= GuiTracker::KI_TRACKER_STACK_SIZE",
        // BrnGuiCache.cpp:4227; the RecEvent case-211/232 bounds compare against 0x40).
        static const s32 KI_TRACKER_STACK_SIZE = 64;

        // Route-point capacity of the flattened route array (the X360 append helper's own
        // mangled arity: Array<rw::math::vpu::Vector3, 5120>::Append).
        static const s32 KI_ROUTE_POINT_CAPACITY = 5120;

        // One tracker-information record. Stride 0x30 (48). Interior recovered by the
        // [map arm 2026-08-27] producers/consumers:
        //   +0x10 position lane  (SatNavComponent::UpdateFreeRoaming lvx128; the 232
        //          producers stvx128 the landmark's position lane here)
        //   +0x20 muTargetSectionId (RegenerateRouteData's assert names it on the
        //          player record; case-233's pending id lands here)
        //   +0x28 miLandmarkIndex (the 232 producers `sth` the landmark index here;
        //          RecEvent case 165 compares the checkpoint event's index against it)
        // ⚠️ [FLAG PC-platform] the console 232 producers also poke an icon-type byte (4)
        // at BIG-ENDIAN byte +0x1E -- inside the position lane's w float. No recovered
        // reader consumes it, and the byte position is not endian-portable, so this host
        // drops that poke (the lane keeps the w the producer wrote, 0).
        struct TrackerInformation
        {
            u8      maHeadStorage[0x10];   // +0x00..+0x0F  unrecovered record head
            Vector3 mv3Position;           // +0x10         the tracked world position
            u32     muTargetSectionId;     // +0x20         world section id (0x7FFF invalid)
            u8      maMidStorage[0x4];     // +0x24..+0x27  unrecovered
            s16     miLandmarkIndex;       // +0x28         the tracked landmark's index
            u8      maTailStorage[0x6];    // +0x2A..+0x2F  unrecovered record tail
        };

        // The id-232 "SetTracker" event record (3088 == 0xC10 bytes on console). Producers:
        // GuiCache::UpdateTrackerInfo @0x82506F28, its online sibling @0x82507070, and
        // GuiCache::HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID @0x825071C8.
        // Field names are the X360's own assert vocabulary (BrnGuiCache.cpp:4204/:4227).
        struct SetTrackerEvent
        {
            TrackerInformation maEntries[KI_TRACKER_STACK_SIZE];   // +0x000..+0xBFF
            s32 miNumTrackedItems;                                 // +0xC00
            s32 miCurrentlyTrackedIndex;                           // +0xC04
            u8  mu8Flag;                                           // +0xC08 (always 1 from the producers)
        };

        // The id-211 route-information record (5136 == 0x1410 bytes on console): one
        // tracked leg's route polyline. Field names from the case-211 assert
        // ("lpRouteInformaton->miEventId ...", BrnGuiTracker.cpp:166) and the
        // GenerateRouteData walk (count word at +0x1400, distance accumulated from +0x1408).
        struct RouteInformation
        {
            Vector3 maPoints[320];        // +0x0000..+0x13FF (stride 16)
            s32     miNumPoints;          // +0x1400
            s32     miEventId;            // +0x1404  the leg's tracker-stack slot (0..63)
            f32     mfDistance;           // +0x1408  the leg's route distance
            u8      maTailPad[4];         // +0x140C
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

        // @ 0x824FA0A8 - drop the current route/tracker state.
        void ClearTracker();

        // @ 0x82501D28 - [map arm 2026-08-27] the tracker's event consumer:
        //   64  - latch the GuiCache pointer + refresh mPlayersTrackerInfo's position from
        //         the cache's world-camera lane;
        //   165 - checkpoint reached: advance miCurrentlyTrackedIndex when the event's
        //         landmark index matches the tracked record's; regenerate the route data
        //         mid-route, or clear the whole tracker at the final checkpoint;
        //   211 - one route-information leg arrives: copy it into its slot, accumulate the
        //         distance, and build the route once every leg is in;
        //   232 - the SetTracker publish: adopt the whole tracked-record stack;
        //   233 - a new target section id for the player record + route regeneration.
        void RecEvent(const void* lpEvent, s32 liEventId);

        // @ 0x824FA008 - flatten every received leg's points into the route-point array
        // (Array<Vector3, 5120> semantics: capacity-asserted append), then mark the route
        // present.
        void GenerateRouteData();

        // @ 0x824F41E0 - rebuild the tracked stack from the CURRENT position: the player
        // record (stamped with the pending target section id) becomes record 0, the
        // remaining un-reached records follow, and the leg counter resets.
        void RegenerateRouteData();

    private:
        // ---- recovered layout (guest 32-bit offsets) -----------------------------------
        // The head byte trio: written by ClearTracker (all 0) and RecEvent case 232
        // (1 / count>1 / the record's flag byte). FLAG: role-named -- no DWARF row; the
        // roles are read off the writers (no recovered reader of +0x01/+0x02).
        u8  mbTrackingActive;            // +0x00  1 while a tracked set is live
        u8  mbRouteDataPending;          // +0x01  raised while route legs are outstanding
        u8  mu8SetTrackerFlag;           // +0x02  the SetTracker record's flag byte
        u8  mbHasRoute;                  // +0x03  has-route flag (lbz 3)
        s32 miTrackerCount;              // +0x04  live tracker-record count (lwz 4)
        u8  maPreRecordReserved[0x10 - 0x08];  // +0x08..+0x0F  head padding before records

        TrackerInformation maTrackerRecords[KI_TRACKER_STACK_SIZE];
                                         // +0x10..+0xC0F  (64 * 0x30)
        TrackerInformation mPlayersTrackerInfo;
                                         // +0xC10..+0xC3F (RecEvent case 64 / RegenerateRouteData)

        RouteInformation maRouteLegs[KI_TRACKER_STACK_SIZE];
                                         // +0xC40..+0x5103F (64 * 0x1410)
        s32 miNumRouteLegsReceived;      // +0x51040  legs received since the last SetTracker
        u8  maPad_51044[0xC];            // +0x51044..+0x5104F

        Vector3 maRouteInfoPoints[KI_ROUTE_POINT_CAPACITY];
                                         // +0x51050..+0x6504F (the Array<Vector3,5120> storage)
        s32 miRouteInfoCount;            // +0x65050  route-point live count (-1 == unconstructed)
        u8  maPad_65054[0xC];            // +0x65054..+0x6505F
        s32 miCurrentlyTrackedIndex;     // +0x65060  (-1 == none; ClearTracker's stw -1)
        s32 muPendingTargetSectionId;    // +0x65064  RecEvent case 233's store; consumed by
                                         //           RegenerateRouteData into the player record
        f32 mfRouteDistance;             // +0x65068  live route distance (lfsx)
        GuiCache* mpGuiCache;            // +0x6506C (console ptr) RecEvent case 64's latch
    };

    // The record stride is load-bearing (it reproduces the X360 48-byte stride), and so
    // are the two big-array strides the RecEvent copies index by.
    static_assert(sizeof(GuiTracker::TrackerInformation) == 0x30, "GuiTracker record stride");
    static_assert(sizeof(GuiTracker::RouteInformation) == 0x1410, "route-information stride");
}
