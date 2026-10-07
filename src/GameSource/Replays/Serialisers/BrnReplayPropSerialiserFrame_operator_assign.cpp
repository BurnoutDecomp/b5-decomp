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
// The frame interior is nine BrnReplayArray members, pinned by the static_asserts in
// BrnReplayPropSerialiserFrame.h::_AssertLayout.
//
// WHERE THE TU'S BODIES LIVE (all three files are partfiles of this same TU):
//   BrnReplayPropSerialiserFrame_operator_assign.cpp  operator= (here)
//   BrnReplayPropSerialiserFrame.cpp                  GetZone, WasPropPreviouslyHit,
//                                                     AllocateLoadedZoneRecord, RemoveLoadedZone,
//                                                     SetPropAddedToScene, WriteProp, WritePart,
//                                                     GetPropTransform, GetPartTransform,
//                                                     IsCellActive, IsPropAddedToScene,
//                                                     KeyFrameRead
//   BrnReplayPropSerialiserFrame_serialise.cpp        Read, Write, KeyFrameWrite

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
