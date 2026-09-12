// GameSource/Replays/Serialisers/BrnReplayPropSerialiserFrame_wQ2_keyframe.cpp
//
// Partfile of the BrnReplayPropSerialiserFrame TU: PropSerialiserFrame::KeyFrameRead, the
// key-frame (non-delta) playback path. Its three siblings -- Read, Write and KeyFrameWrite --
// live in BrnReplayPropSerialiserFrame_serialise.cpp.
//
// =================================================================================================
// WHAT IT DOES
// =================================================================================================
// Unlike the four u16/u32 sub-arrays -- which are read through BrnReplayArray<T,N>::Read -- the
// position+orientation pairs are NOT stored on the wire as two arrays. They ride ONE 12-byte
// QuantisedQuatPos record per element, which this function unpacks and splits back into the two
// arrays:
//
//   ReadByte(&count)
//   maPropPositions.muLength    = count    ; BOTH lengths come from the ONE count byte, and both
//   maPropOrientations.muLength = count    ; are set BEFORE the loop -- which is what makes the
//                                          ; indexed writes below pass operator[]'s bounds check
//   for i in [0, count):
//       Read(packed, 12)
//       UnPack(working, packed)
//       maPropPositions[i]    = { working[4], working[5], working[6], 0.0f }
//       maPropOrientations[i] = { working[0], working[1], working[2], working[3] }
//   <the same block again for the PART arrays>
//   maLoadedZones.Read()  maTypes.Read()  maPartTypes.Read()  maPartIds.Read()
//   maRecordedCells.Read()
//   assert(maPropPositions.GetLength() == maTypes.GetLength())
//   assert(maPropPositions.GetLength() == maPropOrientations.GetLength())
//
// The working buffer's float layout (quaternion in floats 0..3, position in floats 4..6) is read
// straight off the load pairs, and the w lane of the position is an explicit zero store. This is
// the same 8-float working set the committed BrnReplayQuantisedQuatPos.h banner describes for the
// traffic serialiser's ReadAsQuatPos, and the exact set the sibling KeyFrameWrite fills.
//
// THE TWO ASSERTS ARE THE SOURCE OF THREE MEMBER NAMES. Their streamed message fragments are, in
// the image, "maPropPositions.GetLength(): ", " maTypes.GetLength(): " and
// " maPropOrientations.GetLength(): " -- that is what pinned those three names in the header.
// Each streamed message collapses to one CGS_ASSERT carrying BOTH of that site's literals (only
// the streamed VALUE between them is dropped) -- the leading literal alone is shared by the two
// sites and would make them indistinguishable in the host log. Both asserts are NON-GATING on the
// console (it falls through and returns), and so here.
//
// NOTE (faithful, not a bug): the assert compares maPropPositions against maTypes even though
// maTypes was filled by its own Read a few lines earlier -- the console really does cross-check
// the two independently-streamed lengths.
//
// NO CONSOLE OFFSET IS TRANSCRIBED: every array is reached by member name.
// =================================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Replays/BrnReplayQuantisedQuatPos.h"
#include "GameSource/Replays/Serialisers/BrnReplayPropSerialiserFrame.h"

namespace BrnReplays
{
    namespace
    {
        // One packed orientation+position record on the wire (12 bytes at both Read sites).
        const s32 KI_QUANTISED_QUATPOS_BYTES = 12;
        // The unpacked working set UnPack fills: 8 floats, quaternion in 0..3, position in 4..6.
        const s32 KI_QUATPOS_WORKING_FLOATS = 8;

        // The per-element body both halves of KeyFrameRead run, split out because the console
        // emits it twice with only the two destination arrays changed.
        template <u8 N>
        void ReadQuantisedPairs(BaseSerialiser*                     lpSerialiser,
                                BrnReplayArray<Vector3, N>&         lrPositions,
                                BrnReplayArray<rw::math::vpu::Quaternion, N>& lrOrientations)
        {
            u8 lu8Count = 0;
            lpSerialiser->ReadByte(&lu8Count);

            // Both lengths come from the one streamed count, and both are set BEFORE the loop.
            lrPositions.muLength    = lu8Count;
            lrOrientations.muLength = lu8Count;

            for (u8 lu8Index = 0; lu8Index < lu8Count; ++lu8Index)
            {
                u8 lau8Packed[KI_QUANTISED_QUATPOS_BYTES];
                lpSerialiser->Read(lau8Packed, KI_QUANTISED_QUATPOS_BYTES);

                f32        lafWorking[KI_QUATPOS_WORKING_FLOATS];
                const f32* lpfQuatPos = QuantisedQuatPos::UnPack(lafWorking, lau8Packed);

                Vector3 lPosition;
                lPosition.x = lpfQuatPos[4];
                lPosition.y = lpfQuatPos[5];
                lPosition.z = lpfQuatPos[6];
                lPosition.w = 0.0f;

                rw::math::vpu::Quaternion lOrientation;
                lOrientation.x = lpfQuatPos[0];
                lOrientation.y = lpfQuatPos[1];
                lOrientation.z = lpfQuatPos[2];
                lOrientation.w = lpfQuatPos[3];

                lrPositions[lu8Index]    = lPosition;
                lrOrientations[lu8Index] = lOrientation;
            }
        }
    }

    void PropSerialiserFrame::KeyFrameRead(BaseSerialiser* lpSerialiser)
    {
        ReadQuantisedPairs(lpSerialiser, maPropPositions, maPropOrientations);
        ReadQuantisedPairs(lpSerialiser, maPartPositions, maPartOrientations);

        maLoadedZones.Read(lpSerialiser);
        maTypes.Read(lpSerialiser);
        maPartTypes.Read(lpSerialiser);
        maPartIds.Read(lpSerialiser);
        maRecordedCells.Read(lpSerialiser);

        // Each assert carries BOTH of its streamed literals, not just the shared leading one --
        // with only "maPropPositions.GetLength(): " the two adjacent console sites are
        // indistinguishable in the host log, which is a real loss of the console's own
        // discrimination. The dropped part is the streamed VALUE between the two fragments.
        CGS_ASSERT(maPropPositions.muLength == maTypes.muLength,
                   "maPropPositions.GetLength():  maTypes.GetLength(): ");
        CGS_ASSERT(maPropPositions.muLength == maPropOrientations.muLength,
                   "maPropPositions.GetLength():  maPropOrientations.GetLength(): ");
    }
}
