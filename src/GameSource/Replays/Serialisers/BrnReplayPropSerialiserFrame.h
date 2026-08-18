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
//
// =============================================================================================
// [2026-08-18 GROW -- THE WHOLE FRAME IS DECODED. The pad ladder is GONE.]  wave Q round 2,
// replay-frame shared-header owner.  PropSerialiserFrame is NINE BrnReplayArray members and
// nothing else; every one of the nine `stb 0` bytes CheckPreviousFrameCleared writes is one of
// those arrays' muLength.  The decode is MEASURED, from four X360 bodies that walk the frame:
//
//   * PropSerialiserFrame::Read @0x82653120 is the roll call.  Its first nine calls are
//     `<array>.Read(lpSerialiser)` on, in order, `this+0` (0x82651058), +0x610 (0x826514F0),
//     +0x1600 (0x82651738), +0x25F0 (0x82651980), +0x27F0 (0x82651BC8), +0x3000 (0x82651E10),
//     +0x3810 (0x82652058), +0x3912 (0x82652058 again -- SAME instantiation, so those two
//     arrays have the same T and N) and +0x5F0 (0x826512A8 == the <u32,4> Read the committed
//     BrnReplayArray.h banner already names).  Nine bases, nine arrays, no other member.
//   * WriteProp @0x822BB528 PushBacks into +0x610 (0x822AA440), +0x1600 (0x822AA5B8) and
//     +0x25F0 (0x822AA730); WritePart @0x822BB718 into +0x27F0 (0x822AA8A8), +0x3000
//     (0x822AAA20), +0x3810 and +0x3912 (both 0x822AAB98).
//   * GetPropTransform @0x822BB920 indexes +0x1600 then +0x610; GetPartTransform @0x822BBA18
//     indexes +0x3000 then +0x27F0; IsCellActive @0x822BBDA0 scans +0x5F0 with the count at
//     +0x600; PropCellManager::RecordPropPositions @0x822E1A64 PushBacks into +0x5F0.
//
//   ELEMENT SIZE / CAPACITY, from each accessor's own guard (all read by me from the asm):
//     +0x610  stride 16, count @+0xFE0, PushBack guard `cmplwi 0xFE`   -> 16-byte T, N = 254
//     +0x1600 stride 16, count @+0xFE0, PushBack guard `cmplwi 0xFE`   -> 16-byte T, N = 254
//     +0x25F0 stride  2, count @+0x1FC, PushBack guard `cmplwi 0xFE`   -> u16,       N = 254
//     +0x27F0 stride 16, count @+0x800, PushBack guard `cmplwi 0x80`   -> 16-byte T, N = 128
//     +0x3000 stride 16, count @+0x800, PushBack guard `cmplwi 0x80`   -> 16-byte T, N = 128
//     +0x3810 stride  2, count @+0x100, PushBack guard `cmplwi 0x80`   -> u16,       N = 128
//     +0x3912 stride  2, count @+0x100, PushBack guard `cmplwi 0x80`   -> u16,       N = 128
//     +0x05F0 stride  4, count @+0x010, PushBack guard `cmplwi 4`      -> u32,       N = 4
//     +0x0000 stride 168, count @+0x5E8, alloc guard `cmplwi 9`        -> the record, N = 9
//
//   MEMBER NAMES: three are ATTESTED VERBATIM by the streamed asserts KeyFrameRead @0x826586B0
//   (and Read @0x82653120 / Write @0x82657FE0) build at BrnReplayPropEntitySerialiser.h:420/:421 --
//   "maPropPositions.GetLength(): ", " maTypes.GetLength(): ", " maPropOrientations.GetLength(): ".
//   The other six names below are DESCRIPTIVE (by role + symmetry), not attested.
//   NOTE for anyone touching BrnReplayArray.h: those asserts also attest the accessor spelling
//   `GetLength()`, which the committed template does not have -- see the report in
//   scratchpad/waveQ2/replays.owner.md.
//
//   ELEMENT TYPES: the two "16-byte T" pairs are a POSITION and an ORIENTATION array.  The
//   position element is the source/destination of the transform's Pos row verbatim
//   (WriteProp `addi r4, r31, 0x30` == &Matrix44Affine::wAxis; GetPropTransform stores the
//   element straight back to out+0x30), so it is a Vector3.  The orientation element is a unit
//   quaternion: GetPropTransform multiplies it by rw::math::vpu::detail::gSqrt2s and expands it
//   with the exact instruction sequence the committed rw::physics::Quaternion::
//   UnitQuaternionToMatrix carries (same permute constants unk_82CDA3D0/450/410, same
//   `vrlimi128 mask=2` rotates 0/3/2, same vpermwi128 0x60/0x84) -- see the .cpp banner.
//
//   HOST-vs-CONSOLE (gotcha 1): NOTHING here is a transcribed console offset.  All nine members
//   are real typed objects and every offset above is reproduced by natural C++ alignment on the
//   x64 host, because every element type is a fixed-width POD with no pointer to widen:
//   BrnReplayArray<T,N> is `T maElements[N]; u8 muLength;`, so alignof is alignof(T) and the
//   16-byte-aligned Vector3/Quaternion arrays round 0x..E1 up to 0x..F0 exactly as the console
//   does.  _AssertLayout below pins all nineteen numbers; if a future edit breaks one, the
//   static_assert fires rather than a silent stride mismatch.
// =============================================================================================

#include <cstddef>                                                  // offsetof (uncalled _AssertLayout)

#include "types.hpp"
#include "BrnCommonTypes.h"                                         // Vector3, Matrix44Affine (rw::math::vpu)
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

    // Per-frame record capacities. Each is the N its own accessors bake (see the frame banner):
    //   254 -- `cmplwi 0xFE` in the PushBacks at 0x822AA440 / 0x822AA5B8 / 0x822AA730, and the
    //          count-byte offsets 0xFE0 (== 254*16) / 0x1FC (== 254*2).
    //   128 -- `cmplwi 0x80` in the PushBacks at 0x822AA8A8 / 0x822AAA20 / 0x822AAB98, and the
    //          count-byte offsets 0x800 (== 128*16) / 0x100 (== 128*2).
    //     4 -- `cmplwi 4` in the PushBack the console folded into PropCellManager::
    //          RecordPropPositions @0x822E1A6C, count byte at +0x10 (== 4*4).
    // The 254/128 pair is also what PropCellManager::RecordPropPositions tests against before
    // streaming its "Prop replay full" / "Prop part replay full" warnings.
    static const u8  KU_MAX_RECORDED_PROPS = 254;
    static const u8  KU_MAX_RECORDED_PARTS = 128;
    static const u8  KU_MAX_RECORDED_CELLS = 4;

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
        // ---- the nine sub-arrays, in declaration order == console order ------------------
        // Every muLength listed below is one of the nine bytes CheckPreviousFrameCleared
        // @0x822CD650 zeroes ("reset the frame's record arrays"); no separate flag byte exists.

        // @0x0000  the zones this replay has loaded.        muLength @0x05E8
        BrnReplayArray<PropLoadedZoneRecord, KU_MAX_LOADED_ZONES> maLoadedZones;

        // @0x05F0  the cell ids recorded into this frame.   muLength @0x0600
        // The element is the console's PACKED cell WORD, not a PropCellId struct: the producer
        // (PropCellManager::RecordPropPositions) builds `(cellId.GetX() << 16) | cellId.GetZ()`
        // with insrwi and IsCellActive compares it with a 32-bit `cmplw`. Keeping it a u32 that
        // both ends compute ARITHMETICALLY is what makes the record endian-independent -- a
        // by-value PropCellId {u16 muX; u16 muZ} would only reproduce the console word on a
        // big-endian host (gotcha 1).
        BrnReplayArray<u32, KU_MAX_RECORDED_CELLS> maRecordedCells;

        // @0x0610  one recorded prop position per prop.     muLength @0x15F0  [NAME ATTESTED]
        BrnReplayArray<Vector3, KU_MAX_RECORDED_PROPS> maPropPositions;
        // @0x1600  its unit-quaternion orientation.         muLength @0x25E0  [NAME ATTESTED]
        BrnReplayArray<rw::math::vpu::Quaternion, KU_MAX_RECORDED_PROPS> maPropOrientations;
        // @0x25F0  the prop's type id.                      muLength @0x27EC  [NAME ATTESTED]
        BrnReplayArray<u16, KU_MAX_RECORDED_PROPS> maTypes;

        // @0x27F0  one recorded part position per part.     muLength @0x2FF0
        BrnReplayArray<Vector3, KU_MAX_RECORDED_PARTS> maPartPositions;
        // @0x3000  its unit-quaternion orientation.         muLength @0x3800
        BrnReplayArray<rw::math::vpu::Quaternion, KU_MAX_RECORDED_PARTS> maPartOrientations;
        // @0x3810  the part's owning prop type id (WritePart r5 == PropPartEntityInstance
        //          muTypeId, `lhz 0x40`).                   muLength @0x3910
        BrnReplayArray<u16, KU_MAX_RECORDED_PARTS> maPartTypes;
        // @0x3912  the part index within that type (WritePart r6 == muPartId, `lhz 0x42`).
        //                                                   muLength @0x3A12
        BrnReplayArray<u16, KU_MAX_RECORDED_PARTS> maPartIds;
        // (the frame ends at 0x3A13; alignof == 16 rounds sizeof to the console's 0x3A20)

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
        // RECON-ONLY cv forwarder (there is ONE console GetZone). Inline here rather than a
        // second out-of-line body so the class keeps exactly one definition per X360 symbol.
        PropLoadedZoneRecord* GetZone(s32 liZoneId)
        {
            const PropSerialiserFrame* lpConstThis = this;
            return const_cast<PropLoadedZoneRecord*>(lpConstThis->GetZone(liZoneId));
        }

        // ADDITIVE GROW (wave Q round 2). "Is this prop currently in the scene?" -- X360
        // @0x822CD818 (66 insns). Byte-for-byte WasPropPreviouslyHit with `addi r11,r11,1`
        // (+0x08, maPropsAddedToScene) instead of `addi r11,r11,0xB` (+0x58): look the zone
        // up, return false when it is not loaded, assert luPropIndex < 600 (the inlined
        // BitArray::IsBitSet guard, CgsBitArray.h:203) and test the bit.
        // Called by PropEntityModule::GenerateDispatchLists @0x822FB4F0.
        bool IsPropAddedToScene(s32 liZoneId, u32 luPropIndex) const;

        // ADDITIVE GROW. X360 @0x822BBDA0 (26 insns). Linear scan of maRecordedCells for the
        // packed cell word; the count byte is re-read every iteration on the console (it cannot
        // change inside the loop, so it is hoisted here). luPackedCellId is
        // `(cellId.GetX() << 16) | cellId.GetZ()` -- see maRecordedCells above.
        // Called by PropEntityModule::GenerateDispatchLists @0x822FB4F0.
        bool IsCellActive(u32 luPackedCellId) const;

        // ADDITIVE GROW. X360 @0x822BB920 / @0x822BBA18 (62 insns each). Rebuild one recorded
        // prop / part transform: expand maProp{Part}Orientations[i] (a unit quaternion) into
        // the rotation rows, then drop maProp{Part}Positions[i] into the Pos row. The console
        // returns the Matrix44Affine BY VALUE -- r3 is the hidden sret pointer, r4 is `this`,
        // r5 the u8 index (`clrlwi r29, r5, 24`), and the epilogue does `mr r3, r31`.
        // Callers: PropEntityModule::RenderReplayProp @0x822EF968 (prop) and
        // ::GenerateDispatchLists @0x822FB4F0 (part).
        Matrix44Affine GetPropTransform(u8 lu8Index) const;
        Matrix44Affine GetPartTransform(u8 lu8Index) const;

        // ADDITIVE GROW. X360 @0x822BB528 (124 insns) / @0x822BB718 (129 insns). Append one
        // prop / one prop part to this frame's record arrays: position, orientation (extracted
        // from the transform's 3x3) and the id word(s).
        //
        // The transform parameter is a REFERENCE, not the instance: the console reads exactly
        // the four 16-byte rows at r4+0/+0x10/+0x20/+0x30 and nothing else. Its one caller,
        // PropCellManager::RecordPropPositions @0x822E1988, passes the PropEntityInstance* /
        // PropPartEntityInstance* directly because mWorldTransform is that class's FIRST member
        // (BrnPropEntityInstance.h) -- so pass `lpProp->mWorldTransform` at the call site.
        // [round-3: this line used to say `GetWorldTransform()`, disagreeing with the lander
        //  recipe in scratchpad/waveQ2/replays.owner.md section 7.2. `mWorldTransform` is the
        //  spelling that works for BOTH instance types: PropEntityInstance has the accessor
        //  (BrnPropEntityInstance.h:110-111), PropPartEntityInstance has only the public member
        //  (:200). One spelling, both call sites; the spec now says the same.]
        // [INFERENCE, stated because the asm cannot separate the two readings: taking the
        // transform keeps the replay layer free of a GameSource/World include and matches
        // Get{Prop,Part}Transform's own Matrix44Affine vocabulary.]
        void WriteProp(const Matrix44Affine& lrTransform, u16 lu16TypeId);
        void WritePart(const Matrix44Affine& lrTransform, u16 lu16TypeId, u16 lu16PartId);

        // Never called; pins the frame offsets the console's absolute stores use.
        // Nineteen numbers, every one of them read from the X360 asm (see the banner).
        static void _AssertLayout()
        {
            static_assert(offsetof(PropSerialiserFrame, maLoadedZones) == 0x0000,
                          "loaded-zone array at the frame base (operator[] @0x822AA058)");
            static_assert(offsetof(PropSerialiserFrame, maLoadedZones.muLength) == 0x05E8,
                          "loaded-zone count byte at frame +0x5E8 (lbz 0x5E8 in all four bodies)");

            static_assert(offsetof(PropSerialiserFrame, maRecordedCells) == 0x05F0,
                          "recorded-cell array at +0x5F0 (IsCellActive `addi r30,r28,0x5F0`)");
            static_assert(offsetof(PropSerialiserFrame, maRecordedCells.muLength) == 0x0600,
                          "recorded-cell count at +0x600 (IsCellActive `lbz r11,0x600`)");

            static_assert(offsetof(PropSerialiserFrame, maPropPositions) == 0x0610,
                          "prop positions at +0x610 (WriteProp `addi r3,r30,0x610`)");
            static_assert(offsetof(PropSerialiserFrame, maPropPositions.muLength) == 0x15F0,
                          "prop-position count at +0x15F0 (0x610 + 254*16)");

            static_assert(offsetof(PropSerialiserFrame, maPropOrientations) == 0x1600,
                          "prop orientations at +0x1600 (WriteProp `addi r3,r30,0x1600`)");
            static_assert(offsetof(PropSerialiserFrame, maPropOrientations.muLength) == 0x25E0,
                          "prop-orientation count at +0x25E0 (0x1600 + 254*16)");

            static_assert(offsetof(PropSerialiserFrame, maTypes) == 0x25F0,
                          "prop type ids at +0x25F0 (WriteProp `addi r3,r30,0x25F0`)");
            static_assert(offsetof(PropSerialiserFrame, maTypes.muLength) == 0x27EC,
                          "prop type-id count at +0x27EC (0x25F0 + 254*2)");

            static_assert(offsetof(PropSerialiserFrame, maPartPositions) == 0x27F0,
                          "part positions at +0x27F0 (WritePart `addi r3,r31,0x27F0`)");
            static_assert(offsetof(PropSerialiserFrame, maPartPositions.muLength) == 0x2FF0,
                          "part-position count at +0x2FF0 (0x27F0 + 128*16)");

            static_assert(offsetof(PropSerialiserFrame, maPartOrientations) == 0x3000,
                          "part orientations at +0x3000 (WritePart `addi r3,r31,0x3000`)");
            static_assert(offsetof(PropSerialiserFrame, maPartOrientations.muLength) == 0x3800,
                          "part-orientation count at +0x3800 (0x3000 + 128*16)");

            static_assert(offsetof(PropSerialiserFrame, maPartTypes) == 0x3810,
                          "part type ids at +0x3810 (WritePart `addi r3,r31,0x3810`)");
            static_assert(offsetof(PropSerialiserFrame, maPartTypes.muLength) == 0x3910,
                          "part type-id count at +0x3910 (0x3810 + 128*2)");

            static_assert(offsetof(PropSerialiserFrame, maPartIds) == 0x3912,
                          "part ids at +0x3912 (WritePart `addi r3,r31,0x3912`)");
            static_assert(offsetof(PropSerialiserFrame, maPartIds.muLength) == 0x3A12,
                          "part-id count at +0x3A12 (0x3912 + 128*2)");

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
