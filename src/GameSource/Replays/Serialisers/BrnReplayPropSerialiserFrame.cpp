// GameSource/Replays/Serialisers/BrnReplayPropSerialiserFrame.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnReplays::PropSerialiserFrame::GetZone                 @ 0x822BBBB8  (27 insns)
//   BrnReplays::PropSerialiserFrame::WasPropPreviouslyHit    @ 0x822CD920  (66 insns)
//   BrnReplays::PropSerialiserFrame::AllocateLoadedZoneRecord@ 0x822AA150  (63 insns)
//   BrnReplays::PropSerialiserFrame::RemoveLoadedZone        @ 0x822BBB10  (42 insns)
//   BrnReplays::PropSerialiserFrame::SetPropAddedToScene     @ 0x822BBC28  (94 insns)
//
// The loaded-zone table's whole read/write surface -- the two lookups PropZoneManager::
// LoadProp @0x822F2EF0 needs, plus the three bookkeeping bodies PropEntitySerialiser calls
// that share the same table and the same PropLoadedZoneRecord decode.
//
// [2026-08-18 UPDATE, wave Q round 2] Six of the ten functions this banner listed as
// DECLARED-ONLY are now bodied in the sibling partfile
// BrnReplayPropSerialiserFrame_wQ2_owner.cpp (IsPropAddedToScene @0x822CD818, IsCellActive
// @0x822BBDA0, WriteProp @0x822BB528, WritePart @0x822BB718, GetPropTransform @0x822BB920,
// GetPartTransform @0x822BBA18), and KeyFrameRead @0x826586B0 in
// BrnReplayPropSerialiserFrame_wQ2_keyframe.cpp.
//
// STILL DECLARED-ONLY: Read @0x82653120, Write @0x82657FE0 and KeyFrameWrite (unnamed in the
// IDA export). All three -- and KeyFrameRead -- still carry INERT BOOT GATES in
// GameSource/World/WorldLinkStubs.cpp:3842-3900; see the report in
// scratchpad/waveQ2/replays.owner.md for the KeyFrameRead collision the conductor must resolve
// when it mounts the keyframe partfile.
//
// The class declaration's real console home is BrnReplayPropEntitySerialiser.h (the X360
// assert in SetPropAddedToScene @0x822BBC28 bakes
// "..\..\..\GameSource\Replays/Serialisers/BrnReplayPropEntitySerialiser.h", line 1026); the
// recon splits it into BrnReplayPropSerialiserFrame.h, which that header includes. Kept as-is
// -- other TUs already include the split form.
//
// ---- GetZone @0x822BBBB8 ------------------------------------------------------------------
// asm, register-exact: r3 = this, r4 = liZoneId.
//     lbz  r11, 0x5E8(this)          ; the loaded-zone array's live count
//     beq  -> return 0               ; nothing loaded
//   loop:
//     clrlwi r4, index, 24           ; the accessor takes a u8 index
//     bl   BrnReplayArray<PropLoadedZoneRecord,9>::operator[]   (0x822AA058)
//     lwz  r11, 0(r3)                ; element word 0 == miZoneId
//     cmpw r11, liZoneId ; beq -> hit
//     lbz  r11, 0x5E8(this)          ; count RE-READ every iteration (loop-invariant here)
//     addi index,1 ; cmpw index,count ; blt loop
//     -> return 0
//   hit:  bl operator[](index) again -> return that element
// The console calls the checked accessor twice on a hit (once to compare, once to return);
// semantically one lookup, so it is written as one here. The count re-read is likewise
// hoisted into the loop condition -- `muLength` cannot change inside the loop.
//
// ---- WasPropPreviouslyHit @0x822CD920 ------------------------------------------------------
// asm, register-exact: r3 = this, r4 = liZoneId, r5 = luPropIndex (r28).
//     bl   GetZone                   ; r3/r4 passed straight through, untouched
//     cmplwi r27, 0 ; beq -> return 0
//     cmplwi r28, 0x258 ; blt ok     ; assert propIndex < 600, streamed as
//                                    ;   "invalid index : " << index << " < " << 600
//                                    ;   (CgsBitArray.h:203 -- the BitArray::IsBitSet guard,
//                                    ;    inlined here, so the assert is attributed to the
//                                    ;    container header, not to this file)
//   ok:
//     clrldi r10, r28, 58            ; bit   = index & 63
//     srwi   r11, r28, 6             ; field = index >> 6
//     addi   r11, r11, 0xB           ; +11 fields == +0x58 bytes -> maPropsPreviouslyHit
//     slwi   r11, r11, 3             ; *8
//     ldx    r11, r11, r27           ; load the 64-bit field
//     sld    r10, r3(=1), r10 ; and ; cmpldi ; bne -> return 1 else 0
// The `+11` is the whole decode: 11 * 8 == 0x58, the SECOND bit run in the record. Its sibling
// IsPropAddedToScene @0x822CD818 is the identical body with `+1` (== +0x08, the first run).
//
// NO CONSOLE OFFSET IS TRANSCRIBED BELOW. The field/bit split lives inside
// CgsContainers::BitArray<600>::IsBitSet and the record is indexed by member name, so the
// x64 host recomputes both from the type -- both records are pure fixed-width POD, so
// sizeof stays 168 either way (pinned by PropLoadedZoneRecord::_AssertLayout).

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Replays/Serialisers/BrnReplayPropSerialiserFrame.h"

namespace BrnReplays
{
    const PropLoadedZoneRecord* PropSerialiserFrame::GetZone(s32 liZoneId) const
    {
        for (u8 lu8Index = 0; lu8Index < maLoadedZones.muLength; ++lu8Index)
        {
            const PropLoadedZoneRecord& lrRecord = maLoadedZones[lu8Index];
            if (lrRecord.miZoneId == liZoneId)
            {
                return &lrRecord;
            }
        }

        // Zone is not in the replay's loaded set.
        return 0;
    }

    // [2026-08-18] The non-const GetZone cv-forwarder MOVED to the header as an inline. There
    // is exactly ONE console GetZone (@0x822BBBB8), so the class must carry exactly one
    // out-of-line definition of that name -- a second one made coverage_check report a
    // (spurious) ODR duplicate, because it keys on (class, leaf) with no arity or cv.
    // Behaviour is unchanged: the forwarder is the same two lines, now inline.

    bool PropSerialiserFrame::WasPropPreviouslyHit(s32 liZoneId, u32 luPropIndex) const
    {
        const PropLoadedZoneRecord* lpZone = GetZone(liZoneId);
        if (lpZone == 0)
        {
            return false;
        }

        CGS_ASSERT(luPropIndex < KU_PROPS_PER_ZONE, "invalid index : ");

        return lpZone->maPropsPreviouslyHit.IsBitSet(luPropIndex);
    }

    // ---- AllocateLoadedZoneRecord @0x822AA150 (sub_822AA150, 63 insns) --------------------
    // [round-3: the count was 41 in both places above; RECOUNTED from the per-address JSON
    //  `assembly` -- 63 address lines, extent 0x822AA150..0x822AA248 inclusive.]
    // The loaded-zone array's "append uninitialised" accessor (BrnReplayArray.h:103), reached
    // through the frame because the array base IS the frame base:
    //     lbz    r11, 0x5E8(this) ; cmplwi r11, 9 ; blt ok
    //     <assert "Length: " << muLength << " is over max length: " << 9>
    //   ok:
    //     lbz r11,0x5E8 ; mulli r10,r11,0xA8 ; addi r11,r11,1 ; add r3,r10,this
    //     stb r11,0x5E8 ; return r3
    // The capacity assert is a NON-GATING tripwire, exactly like every other assert in this
    // family -- the console falls through and hands out the out-of-range slot anyway. Kept
    // faithful; do not "fix" it into a null return.
    PropLoadedZoneRecord* PropSerialiserFrame::AllocateLoadedZoneRecord()
    {
        CGS_ASSERT(maLoadedZones.muLength < KU_MAX_LOADED_ZONES, "Length: ");

        PropLoadedZoneRecord* lpRecord = &maLoadedZones.maElements[maLoadedZones.muLength];
        ++maLoadedZones.muLength;
        return lpRecord;
    }

    // ---- RemoveLoadedZone @0x822BBB10 (42 insns) -----------------------------------------
    // GetZone's search open-coded, then a SWAP-REMOVE: decrement the count and block-copy the
    // (new) last record over the removed slot.
    //     beq/loop identical to GetZone
    //   hit:
    //     lbz r10,0x5E8 ; mulli r11,index,0xA8 ; addi r10,r10,0xFF (== -1) ; clrlwi (u8)
    //     stb newLength,0x5E8 ; memcpy(this + 0xA8*index, this + 0xA8*newLength, 0xA8)
    //   miss (or empty):
    //     assert "Zone not loaded" (BrnReplayPropEntitySerialiser.h:952) and do nothing.
    // When the hit IS the last slot the console memcpy's the record onto itself; expressed
    // here as a struct assignment, which is well-defined for that case (and is the same
    // 168-byte copy).
    void PropSerialiserFrame::RemoveLoadedZone(s32 liZoneId)
    {
        for (u8 lu8Index = 0; lu8Index < maLoadedZones.muLength; ++lu8Index)
        {
            if (maLoadedZones[lu8Index].miZoneId == liZoneId)
            {
                --maLoadedZones.muLength;
                maLoadedZones.maElements[lu8Index] =
                    maLoadedZones.maElements[maLoadedZones.muLength];
                return;
            }
        }

        CGS_ASSERT(false, "Zone not loaded");
    }

    // ---- SetPropAddedToScene @0x822BBC28 (94 insns) --------------------------------------
    //     bl GetZone ; cmplwi r31,0 ; bne ok
    //     <assert "lpZone != NULL" (BrnReplayPropEntitySerialiser.h:1026)>   -- NON-GATING:
    //     the console falls through and dereferences anyway, so this reconstruction does too.
    //   ok:
    //     addi r27, r31, 8                 ; &maPropsAddedToScene (the FIRST bit run)
    //     if (liAddedToScene != 0)   -- the console tests only the low byte (clrlwi r11,r30,24)
    //         <assert index < 600: "Index: " << i << ", Number of bits: " << 600,
    //          CgsBitArray.h:222 == the inlined BitArray::SetBit guard>
    //         ldx/or/stdx  -> SetBit
    //     else
    //         <assert index < 600: "luIndex < NUMBITS", CgsBitArray.h:241 == UnSetBit's guard>
    //         ldx/andc/stdx -> UnSetBit
    // Both asserts belong to the inlined container, which is why their file is CgsBitArray.h;
    // the bit math itself is BitArray<600>'s, so it is called, not transcribed.
    void PropSerialiserFrame::SetPropAddedToScene(s32 liZoneId, u32 luPropIndex, s32 liAddedToScene)
    {
        PropLoadedZoneRecord* lpZone = GetZone(liZoneId);
        CGS_ASSERT(lpZone != 0, "lpZone != NULL");

        if (static_cast<u8>(liAddedToScene) != 0)
        {
            CGS_ASSERT(luPropIndex < KU_PROPS_PER_ZONE, "Index: ");
            lpZone->maPropsAddedToScene.SetBit(luPropIndex);
        }
        else
        {
            CGS_ASSERT(luPropIndex < KU_PROPS_PER_ZONE, "luIndex < NUMBITS");
            lpZone->maPropsAddedToScene.UnSetBit(luPropIndex);
        }
    }
}
