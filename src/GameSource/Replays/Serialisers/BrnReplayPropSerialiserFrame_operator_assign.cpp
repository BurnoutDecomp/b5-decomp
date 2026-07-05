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
// PARTIAL note: this is the ONE self-contained function of the PropSerialiserFrame TU.
// The other 13 X360 members (GetPartTransform / GetPropTransform / GetZone / IsCellActive
// / IsPropAddedToScene / Read / KeyFrameRead / Write / KeyFrameWrite / WritePart / WriteProp
// / RemoveLoadedZone / SetPropAddedToScene / WasPropPreviouslyHit) need a fabricated
// frame-interior array layout (BrnReplayArray<> views) and unhomed vendor types
// (rw::math::vpu::Matrix44Affine, quantiser spine); per the no-fabrication rule they remain
// DECLARED-ONLY in the committed BrnReplayPropSerialiserFrame.h and are bodied in a later wave.

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
