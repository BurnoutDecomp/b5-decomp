#pragma once

// BrnReplays::PropSerialiserFrame -- the per-frame snapshot the prop-entity replay
// serialiser records/plays back. No DWARF or prior Feb-2007 source recovered for
// this type, so most of it is modelled as an opaque fixed-size frame whose named
// members are the reset-flag bytes that PropEntitySerialiser::CheckPreviousFrameCleared
// @0x822CD650 and ::RemoveAllLoadedZones @0x822CD790 zero by hand. Everything else
// is held as undecoded blocks. All access is BY NAME. GROW this home (do not
// redefine it) when the PropSerialiserFrame TU family is reconstructed.
//
// [2026-08-12 GROW -- the loaded-zone table at frame +0x0000 is now DECODED.] The first
// 0x5E9 bytes are not opaque: they are a BrnReplayArray<PropLoadedZoneRecord, 9>. Four X360
// bodies agree, and between them they pin the element type, the stride, the capacity and
// the count byte:
//   * BrnReplayArray<PropLoadedZoneRecord,9>::operator[] @0x822AA058 -- bounds-checks the
//     index against `lbz 0x5E8(this)` and returns `mulli index,0xA8; add this`. So the
//     element buffer STARTS AT THE FRAME BASE, the stride is 168 (0xA8), and the u8 live
//     count sits at 9*168 == 0x5E8. (Assert: BrnReplayArray.h:93.)
//   * AllocateLoadedZoneRecord @0x822AA150 -- `cmplwi 0x5E8(this), 9` ("Length: <n> is over
//     max length: 9", BrnReplayArray.h:103), returns element[count], post-increments the
//     byte. Capacity N == 9, matching BrnPropZoneManager's KU_NUM_ZONE_SLOTS.
//   * GetZone @0x822BBBB8 -- linear search of [0, count) comparing element word 0 to the
//     zone id, so PropLoadedZoneRecord's first dword is the zone id.
//   * SetPropAddedToScene @0x822BBC28 / IsPropAddedToScene @0x822CD818 /
//     WasPropPreviouslyHit @0x822CD920 -- two DISTINCT 600-bit runs inside the element, at
//     +0x08 and +0x58 (see PropLoadedZoneRecord below).
// Consequence: the byte the recon previously called `mbZonesLoaded` IS that array's
// muLength. RemoveAllLoadedZones' lone `stb 0` at frame +0x5E8 is literally "set the loaded
// zone count to zero", which is why that one write drops every zone at once.
//
// X360 attestation for the frame SIZE and the named flag offsets:
//   * The static layout buffer is 0x7480 (29824) bytes (Construct arg @0x8264C6DC,
//     `li r7,0x7480`). It holds TWO PropSerialiserFrame copies -- the "previous"
//     frame at static-base +0 and the "live" frame at static-base +0x3A20 (14880)
//     -- so each frame is 0x3A20 (14880) bytes. PropEntitySerialiser::operator path
//     copies one whole 14880-byte frame onto the other (operator_ @ PropSerialiserFrame).
//   * CheckPreviousFrameCleared @0x822CD650 / RemoveAllLoadedZones @0x822CD790 write
//     `stb 0` to bytes inside the LIVE frame; offsets below are static-base offsets
//     minus 0x3A20 to make them frame-relative:
//         static 0x4008 -> frame 0x05E8   (also the lone byte RemoveAllLoadedZones clears)
//         static 0x4020 -> frame 0x0600
//         static 0x5010 -> frame 0x15F0
//         static 0x6000 -> frame 0x25E0
//         static 0x620C -> frame 0x27EC
//         static 0x6A10 -> frame 0x2FF0
//         static 0x7220 -> frame 0x3800
//         static 0x7330 -> frame 0x3910
//         static 0x7432 -> frame 0x3A12
//     They are the per-sub-array "added to scene" reset flags the frame keeps; the
//     exact role of each is not separately attested, so they carry generic grounded
//     names indexed by their frame offset.

#include <cstddef>                                                  // offsetof (uncalled _AssertLayout)

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"           // CgsContainers::BitArray<600>
#include "GameSource/Replays/BrnReplayArray.h"                       // BrnReplayArray<T,N>

namespace BrnReplays
{
    // Forward declaration so the frame methods can take a BaseSerialiser*.
    class BaseSerialiser;

    // Size of one prop-serialiser frame (X360: 0x3A20). Two of these plus a small
    // tail make up the 0x7480 static-layout buffer.
    static const u32 KU_PROP_FRAME_SIZE = 0x3A20;

    // Per-zone prop capacity. 600 == BrnPhysics::Props::KU_MAX_PROP_INSTANCES_PER_ZONE
    // (BrnPropConstants.h:30, mirrored in BrnPropZoneManager.h). Spelled locally rather than
    // pulling the world-module header into the replay layer; the value is independently
    // pinned by this file's own X360 bodies (`cmplwi r28, 0x258` in IsPropAddedToScene /
    // SetPropAddedToScene / WasPropPreviouslyHit, and the streamed "< 600" assert operand).
    static const u32 KU_PROPS_PER_ZONE = 600;

    // Loaded-zone table capacity -- `cmplwi 0x5E8(this), 9` in AllocateLoadedZoneRecord
    // @0x822AA150 ("is over max length: 9"). Same 9 as PropZoneManager's KU_NUM_ZONE_SLOTS.
    static const u8  KU_MAX_LOADED_ZONES = 9;

    // One loaded-zone record AddLoadedZone @0x822E7738 fills, and the element type of the
    // frame's BrnReplayArray. sizeof == 168 (0xA8), fixed by the `mulli index,0xA8` stride in
    // operator[] @0x822AA058 -- and it stays 168 on the x64 host because every member is a
    // fixed-width POD (no pointers to widen).
    //
    // The two 600-bit runs are the reason AddLoadedZone's "zero the payload" shows up in the
    // asm as TWO 0x50-byte runs of `std 0` (x10 from +0x08, x10 from +0x58) rather than one
    // 0xA0 run: they are two separate CgsContainers::BitArray<600> members, each 10 u64
    // fields == 80 bytes. Which run is which is attested by the callers' index arithmetic:
    //   +0x08 : `8 * ((index >> 6) + 1)`   IsPropAddedToScene @0x822CD818,
    //                                      SetPropAddedToScene @0x822BBC28 (`addi r27,r31,8`)
    //   +0x58 : `8 * ((index >> 6) + 11)`  WasPropPreviouslyHit @0x822CD920
    struct PropLoadedZoneRecord
    {
        s32 miZoneId;        // @0x00 set to the zone id by AddLoadedZone; the GetZone search key
        s32 mPad04;          // @0x04 untouched by AddLoadedZone (alignment before the bit runs)

        // @0x08 "is this prop currently in the scene?" -- the run Set/IsPropAddedToScene drive.
        CgsContainers::BitArray<KU_PROPS_PER_ZONE> maPropsAddedToScene;
        // @0x58 "had this prop already been hit when the replay was recorded?" -- the run
        // WasPropPreviouslyHit tests so a played-back zone respawns exactly what it did live.
        CgsContainers::BitArray<KU_PROPS_PER_ZONE> maPropsPreviouslyHit;

        // Never called; pins the record layout the console's index arithmetic assumes.
        static void _AssertLayout()
        {
            static_assert(offsetof(PropLoadedZoneRecord, miZoneId) == 0x00,
                          "PropLoadedZoneRecord::miZoneId is the GetZone search key at +0");
            static_assert(offsetof(PropLoadedZoneRecord, maPropsAddedToScene) == 0x08,
                          "added-to-scene bit run at +0x08 (8*((i>>6)+1))");
            static_assert(offsetof(PropLoadedZoneRecord, maPropsPreviouslyHit) == 0x58,
                          "previously-hit bit run at +0x58 (8*((i>>6)+11))");
            static_assert(sizeof(PropLoadedZoneRecord) == 0xA8,
                          "168-byte element stride (mulli index,0xA8 @0x822AA058)");
        }
    };

    // A bound-checked frame whose touched flag bytes are named and whose unmodelled
    // regions are explicit padding so the named bytes land at their X360 offsets.
    struct PropSerialiserFrame
    {
        // @0x0000 .. @0x05E8 the nine loaded-zone records, @0x05E8 their u8 live count.
        // (The array's own trailing alignment padding takes it to 0x05F0; the console frame
        // has nothing named in 0x05E9..0x0600 either, so the pad below just resumes there.)
        BrnReplayArray<PropLoadedZoneRecord, KU_MAX_LOADED_ZONES> maLoadedZones;
        u8   maPad05F0[0x0600 - 0x05F0];
        bool mbAddedFlag0600;        // @0x0600 reset by CheckPreviousFrameCleared
        u8   maPad0601[0x15F0 - 0x0601];
        bool mbAddedFlag15F0;        // @0x15F0 reset by CheckPreviousFrameCleared
        u8   maPad15F1[0x25E0 - 0x15F1];
        bool mbAddedFlag25E0;        // @0x25E0 reset by CheckPreviousFrameCleared
        u8   maPad25E1[0x27EC - 0x25E1];
        bool mbAddedFlag27EC;        // @0x27EC reset by CheckPreviousFrameCleared
        u8   maPad27ED[0x2FF0 - 0x27ED];
        bool mbAddedFlag2FF0;        // @0x2FF0 reset by CheckPreviousFrameCleared
        u8   maPad2FF1[0x3800 - 0x2FF1];
        bool mbAddedFlag3800;        // @0x3800 reset by CheckPreviousFrameCleared
        u8   maPad3801[0x3910 - 0x3801];
        bool mbAddedFlag3910;        // @0x3910 reset by CheckPreviousFrameCleared
        u8   maPad3911[0x3A12 - 0x3911];
        bool mbAddedFlag3A12;        // @0x3A12 reset by CheckPreviousFrameCleared
        u8   maPad3A13[KU_PROP_FRAME_SIZE - 0x3A13];

        // --- methods owned by the PropSerialiserFrame TU (declared for the gate;
        //     bodies live in their own [todo] translation units). Signatures taken
        //     from the X360 call sites in PropEntitySerialiser. ---

        // operator= : copy one whole frame onto this one (PropSerialiserFrame::operator_).
        PropSerialiserFrame& operator=(const PropSerialiserFrame& lrSource);

        // Record this frame into the serialiser stream (delta path / key-frame path).
        void Read(BaseSerialiser* lpSerialiser);
        void KeyFrameRead(BaseSerialiser* lpSerialiser);
        void Write(BaseSerialiser* lpSerialiser, PropSerialiserFrame* lpStaticLayout);
        void KeyFrameWrite(BaseSerialiser* lpSerialiser, PropSerialiserFrame* lpStaticLayout);

        // Zone / scene-membership bookkeeping.
        // AllocateLoadedZoneRecord (X360 sub_822AA150): returns the next free
        // loaded-zone record slot for the caller to populate. Body lives in the
        // frame TU; declared here for the gate. (It is the array's own
        // "append-uninitialised" accessor, BrnReplayArray.h:103 -- guard `muLength < 9`,
        // return &maLoadedZones.maElements[muLength], post-increment muLength.)
        PropLoadedZoneRecord* AllocateLoadedZoneRecord();
        // Swap-remove the zone's record (X360 @0x822BBB10). Body: the .cpp.
        void RemoveLoadedZone(s32 liZoneId);
        // Set/clear this prop's bit in the zone's added-to-scene run (X360 @0x822BBC28).
        // The third argument is the console's `char` boolean, widened to the s32 the
        // committed PropEntitySerialiser forwarder already passes. Body: the .cpp.
        void SetPropAddedToScene(s32 liZoneId, u32 luPropIndex, s32 liAddedToScene);

        // ADDITIVE GROW (PropZoneManager::LoadProp group, X360 @0x822CD920 -- 66 insns).
        // "Was this prop already hit at the point the replay was recorded?" Looks up the
        // zone's PropLoadedZoneRecord (GetZone(liZoneId); returns false when the zone is
        // not in the replay's loaded set), asserts luPropIndex < 600
        // (KU_PROPS_PER_ZONE), then tests bit luPropIndex of the record's previously-hit
        // run (maPropsPreviouslyHit, PropLoadedZoneRecord +0x58). LoadProp calls this
        // INSTEAD of PropZoneManager::HasPropBeenHit whenever the replay stage is active,
        // so a played-back zone respawns exactly the props it did during recording.
        // Body: BrnReplayPropSerialiserFrame.cpp.
        bool WasPropPreviouslyHit(s32 liZoneId, u32 luPropIndex) const;

        // X360 @0x822BBBB8 -- the zone's loaded record, or null when the zone is not in the
        // replay's loaded set. Linear search of maLoadedZones for miZoneId == liZoneId.
        // Body: BrnReplayPropSerialiserFrame.cpp.
        //
        // The console GetZone is ONE function; SetPropAddedToScene @0x822BBC28 calls it and
        // then writes through the result, so both cv-forms are declared and the non-const one
        // forwards to the const search rather than duplicating it.
        const PropLoadedZoneRecord* GetZone(s32 liZoneId) const;
        PropLoadedZoneRecord*       GetZone(s32 liZoneId);

        // Never called; pins the frame offsets the console's absolute stores use.
        static void _AssertLayout()
        {
            static_assert(offsetof(PropSerialiserFrame, maLoadedZones) == 0x0000,
                          "the loaded-zone array starts at the frame base (operator[] @0x822AA058)");
            static_assert(offsetof(PropSerialiserFrame, maLoadedZones.muLength) == 0x05E8,
                          "loaded-zone count byte at frame +0x5E8 (lbz 0x5E8 in all four bodies)");
            static_assert(offsetof(PropSerialiserFrame, mbAddedFlag0600) == 0x0600,
                          "CheckPreviousFrameCleared static 0x4020 -> frame 0x0600");
            static_assert(offsetof(PropSerialiserFrame, mbAddedFlag3A12) == 0x3A12,
                          "CheckPreviousFrameCleared static 0x7432 -> frame 0x3A12");
            static_assert(sizeof(PropSerialiserFrame) == KU_PROP_FRAME_SIZE,
                          "one frame is 0x3A20 (static layout 0x7480 holds two)");
        }
    };

    // The static-layout buffer GetStaticLayout returns: a "previous" frame followed by
    // the "live" frame. Sized to the 0x7480 Construct argument; both frames are named.
    struct PropSerialiserStaticLayout
    {
        PropSerialiserFrame mPreviousFrame; // @0x0000  destination of operator_ copies
        PropSerialiserFrame mLiveFrame;     // @0x3A20  the frame the serialiser drives
        // @0x7440 .. @0x7480 : 64-byte undecoded tail (Construct sizes the buffer to 0x7480).
        u8                  maTail[0x7480 - 2 * KU_PROP_FRAME_SIZE];
    };
}
