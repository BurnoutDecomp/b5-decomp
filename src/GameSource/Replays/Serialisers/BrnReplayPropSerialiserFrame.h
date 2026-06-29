#pragma once

// BrnReplays::PropSerialiserFrame -- the per-frame snapshot the prop-entity replay
// serialiser records/plays back. No DWARF or prior Feb-2007 source recovered for
// this type, so it is modelled as an opaque fixed-size frame whose ONLY named
// members are the reset-flag bytes that PropEntitySerialiser::CheckPreviousFrameCleared
// @0x822CD650 and ::RemoveAllLoadedZones @0x822CD790 zero by hand. Everything else
// is held as undecoded blocks. All access is BY NAME. GROW this home (do not
// redefine it) when the PropSerialiserFrame TU family is reconstructed.
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

#include "types.hpp"

namespace BrnReplays
{
    // Forward declaration so the frame methods can take a BaseSerialiser*.
    class BaseSerialiser;

    // Size of one prop-serialiser frame (X360: 0x3A20). Two of these plus a small
    // tail make up the 0x7480 static-layout buffer.
    static const u32 KU_PROP_FRAME_SIZE = 0x3A20;

    // One loaded-zone record AddLoadedZone @0x822E7738 fills. The X360 body stores the
    // zone id in the first dword (`stw r29,0(r3)`) then zeroes 0xA0 (160) trailing bytes
    // as two 0x50-byte runs (`std r9=0` x10 from +8, x10 from +0x58). 0x04..0x08 is left
    // untouched by AddLoadedZone (the dword store does not clear it), so it is padding.
    struct PropLoadedZoneRecord
    {
        s32 miZoneId;        // @0x00 set to the zone id by AddLoadedZone
        s32 mPad04;          // @0x04 untouched
        u8  maZeroed[0xA0];  // @0x08 zeroed by AddLoadedZone (160 bytes)
    };

    // A bound-checked frame whose touched flag bytes are named and whose unmodelled
    // regions are explicit padding so the named bytes land at their X360 offsets.
    struct PropSerialiserFrame
    {
        u8   maPad0000[0x05E8];      // @0x0000 undecoded frame state
        bool mbZonesLoaded;          // @0x05E8 cleared by RemoveAllLoadedZones + CheckPreviousFrameCleared
        u8   maPad05E9[0x0600 - 0x05E9];
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
        // frame TU; declared here for the gate.
        PropLoadedZoneRecord* AllocateLoadedZoneRecord();
        void RemoveLoadedZone(s32 liZoneId);
        void SetPropAddedToScene(s32 liArg2, u32 luArg3, s32 liArg4);
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
