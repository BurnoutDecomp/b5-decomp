// GameSource/Replays/Serialisers/BrnReplayPropSerialiserFrame_operator_assign.cpp
//
// BrnReplays::PropSerialiserFrame::operator=  @ 0x822BB408
//   (called by PropEntitySerialiser::CheckPreviousFrameCleared / ::Read / ::Write)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// The X360 body is the compiler-emitted copy-assignment of a trivially-copyable
// fixed-size aggregate. It copies the frame's live regions onto this one via a mix
// of segmented memcpy calls + inline lvx128 / u32 loops + per-count byte stores, with
// NO field-specific logic and NO self-assignment guard, then returns *this. The segment
// boundaries leave only inter-array alignment padding untouched; copying those dead
// padding bytes is behaviourally identical. Mirrors the committed sibling
// SoundSerialiserFrame::operator= (@0x8264CA58): a single full-object copy of the frame.
//
// PARTIAL note -- ⚠️ REWRITTEN 2026-08-18 (wave Q round 3). The version this replaced said the
// other 13 X360 members "need a fabricated frame-interior array layout (BrnReplayArray<> views)
// ... per the no-fabrication rule they remain DECLARED-ONLY ... and are bodied in a later wave".
// That is now FALSE IN BOTH HALVES and was pointing a future agent away from finished work: the
// frame interior is DECODED and MEASURED (nine BrnReplayArray members, no pad ladder), pinned by
// the nineteen static_asserts in BrnReplayPropSerialiserFrame.h::_AssertLayout -- the opposite of
// "fabricated" -- and all but three of the members it listed now have real bodies. (Its list of
// "13" also held fourteen names, and omitted AllocateLoadedZoneRecord, which is bodied too.)
//
// WHERE THE TU'S BODIES LIVE TODAY (all four files are partfiles of this same TU):
//   BrnReplayPropSerialiserFrame_operator_assign.cpp  operator=            @0x822BB408  (here)
//   BrnReplayPropSerialiserFrame.cpp                  GetZone              @0x822BBBB8
//                                                     WasPropPreviouslyHit @0x822CD920
//                                                     AllocateLoadedZoneRecord @0x822AA150
//                                                     RemoveLoadedZone     @0x822BBB10
//                                                     SetPropAddedToScene  @0x822BBC28
//   BrnReplayPropSerialiserFrame_wQ2_owner.cpp        WriteProp            @0x822BB528
//                                                     WritePart            @0x822BB718
//                                                     GetPropTransform     @0x822BB920
//                                                     GetPartTransform     @0x822BBA18
//                                                     IsCellActive         @0x822BBDA0
//                                                     IsPropAddedToScene   @0x822CD818
//   BrnReplayPropSerialiserFrame_wQ2_keyframe.cpp     KeyFrameRead         @0x826586B0
//     (⚠️ that partfile collides with a mounted boot gate -- see its own banner before mounting)
//
// STILL DECLARED-ONLY, and only these three: Read @0x82653120, Write @0x82657FE0 and
// KeyFrameWrite (unnamed in the IDA export, and not in the ledger). Their inert boot gates in
// GameSource/World/WorldLinkStubs.cpp must STAY. Nothing blocks writing them any more -- the
// frame interior was their only blocker -- but see the keyframe partfile's link-level note about
// BrnReplayArray<T,N>::Read/Write needing a per-T delta record first.

#include "GameSource/Replays/Serialisers/BrnReplayPropSerialiserFrame.h"

#include <cstring>

namespace BrnReplays
{
    // @ 0x822BB408 -- unconditional whole-frame copy (no self-assignment guard on the X360
    // path). sizeof(PropSerialiserFrame) == KU_PROP_FRAME_SIZE (0x3A20).
    PropSerialiserFrame& PropSerialiserFrame::operator=(const PropSerialiserFrame& lrSource)
    {
        std::memcpy(this, &lrSource, sizeof(PropSerialiserFrame));
        return *this;
    }
}
