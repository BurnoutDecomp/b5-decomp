// GameSource/Replays/Serialisers/BrnReplayPropSerialiserFrame_serialise.cpp
//
// Partfile of the BrnReplayPropSerialiserFrame TU: the three of the frame's four stream entry
// points that are NOT the key-frame reader (that one keeps its own partfile, the sibling
// BrnReplayPropSerialiserFrame_wQ2_keyframe.cpp).
//
//   PropSerialiserFrame::Read           the delta playback path
//   PropSerialiserFrame::Write          the delta record path
//   PropSerialiserFrame::KeyFrameWrite  the full (non-delta) record path
//
// All three are thin: the frame is nine BrnReplayArray members and nothing else, so Read and
// Write are nine per-array calls plus the pair of cross-check asserts, and KeyFrameWrite is the
// mirror image of KeyFrameRead -- the position+orientation pairs ride ONE 12-byte quantised
// record per element instead of two arrays, and only the remaining five arrays go through the
// array serialiser.
//
// NO CONSOLE OFFSET IS TRANSCRIBED HERE: every array is reached by member name. The frame
// header's _AssertLayout pins the offsets the console's absolute forms use.
//
// ================================================================================================
// THE TWO CROSS-CHECK ASSERTS
// ================================================================================================
// All four entry points carry the same pair, built at two adjacent lines of the prop entity
// serialiser: maPropPositions.GetLength() against maTypes.GetLength(), then against
// maPropOrientations.GetLength(). Each streamed message is collapsed to one CGS_ASSERT carrying
// BOTH of that site's literals -- with only the shared leading literal the two sites are
// indistinguishable in the host log, which is a real loss of the console's own discrimination.
// The dropped part is the streamed VALUE between the two fragments.
//
// Read runs them AFTER its nine reads (it is checking what it just played back); Write and
// KeyFrameWrite run them BEFORE recording anything (they are checking what the frame holds).
// That ordering difference is in the image and is reproduced. Both asserts are non-gating --
// the console falls through and carries on -- and so are these.
//
// ================================================================================================
// WHAT lpStaticLayout IS
// ================================================================================================
// The static-layout buffer holds two frames: the "previous" frame at its base and the live frame
// after it. The prop entity serialiser calls these methods on the LIVE frame and passes the
// buffer base, so the argument the header declares as lpStaticLayout is, at every call site, the
// PREVIOUS frame -- which is exactly what the per-array delta writer wants for its
// (previous data, previous length) pair.

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
        // One packed orientation+position record on the wire.
        const s32 KI_QUANTISED_QUATPOS_BYTES = 12;
        // The working set the quantiser reads: 8 floats, quaternion in 0..3, position in 4..6.
        const s32 KI_QUATPOS_WORKING_FLOATS = 8;

        // The recorder clamps the position's Y lane against a constant before quantising it.
        // The constant is a 16-byte vector literal in the image and every lane of it reads
        // ZERO, so the clamp is "never record a height below zero". Only the Y lane is
        // clamped; X and Z are quantised as they stand.
        const f32 KF_MIN_RECORDED_HEIGHT = 0.0f;

        // The per-element body both halves of KeyFrameWrite run, split out because the console
        // emits it twice with only the two source arrays changed. The mirror of the sibling
        // partfile's ReadQuantisedPairs.
        template <u8 N>
        void WriteQuantisedPairs(BaseSerialiser*                                    lpSerialiser,
                                 BrnReplayArray<Vector3, N>&                        lrPositions,
                                 BrnReplayArray<rw::math::vpu::Quaternion, N>&      lrOrientations)
        {
            // ONE count byte covers both arrays -- the reader sets both lengths from it.
            u8 lu8Count = lrPositions.muLength;
            lpSerialiser->WriteByte(&lu8Count);

            for (u8 lu8Index = 0; lu8Index < lu8Count; ++lu8Index)
            {
                const Vector3&                   lrPosition    = lrPositions[lu8Index];
                const rw::math::vpu::Quaternion& lrOrientation = lrOrientations[lu8Index];

                f32 lafWorking[KI_QUATPOS_WORKING_FLOATS];
                lafWorking[0] = lrOrientation.x;
                lafWorking[1] = lrOrientation.y;
                lafWorking[2] = lrOrientation.z;
                lafWorking[3] = lrOrientation.w;
                lafWorking[4] = lrPosition.x;
                lafWorking[5] = (lrPosition.y > KF_MIN_RECORDED_HEIGHT)
                                    ? lrPosition.y
                                    : KF_MIN_RECORDED_HEIGHT;
                lafWorking[6] = lrPosition.z;

                u8 lau8Packed[KI_QUANTISED_QUATPOS_BYTES];
                QuantisedQuatPos::Pack(lau8Packed, lafWorking);
                lpSerialiser->Write(lau8Packed, KI_QUANTISED_QUATPOS_BYTES);
            }
        }
    }

    void PropSerialiserFrame::Read(BaseSerialiser* lpSerialiser)
    {
        maLoadedZones.Read(lpSerialiser);
        maPropPositions.Read(lpSerialiser);
        maPropOrientations.Read(lpSerialiser);
        maTypes.Read(lpSerialiser);
        maPartPositions.Read(lpSerialiser);
        maPartOrientations.Read(lpSerialiser);
        maPartTypes.Read(lpSerialiser);
        maPartIds.Read(lpSerialiser);
        maRecordedCells.Read(lpSerialiser);

        CGS_ASSERT(maPropPositions.muLength == maTypes.muLength,
                   "maPropPositions.GetLength():  maTypes.GetLength(): ");
        CGS_ASSERT(maPropPositions.muLength == maPropOrientations.muLength,
                   "maPropPositions.GetLength():  maPropOrientations.GetLength(): ");
    }

    void PropSerialiserFrame::Write(BaseSerialiser* lpSerialiser,
                                    PropSerialiserFrame* lpStaticLayout)
    {
        CGS_ASSERT(maPropPositions.muLength == maTypes.muLength,
                   "maPropPositions.GetLength():  maTypes.GetLength(): ");
        CGS_ASSERT(maPropPositions.muLength == maPropOrientations.muLength,
                   "maPropPositions.GetLength():  maPropOrientations.GetLength(): ");

        maLoadedZones.Write(lpSerialiser,
                            lpStaticLayout->maLoadedZones.maElements,
                            lpStaticLayout->maLoadedZones.muLength);
        maPropPositions.Write(lpSerialiser,
                              lpStaticLayout->maPropPositions.maElements,
                              lpStaticLayout->maPropPositions.muLength);
        maPropOrientations.Write(lpSerialiser,
                                 lpStaticLayout->maPropOrientations.maElements,
                                 lpStaticLayout->maPropOrientations.muLength);
        maTypes.Write(lpSerialiser,
                      lpStaticLayout->maTypes.maElements,
                      lpStaticLayout->maTypes.muLength);
        maPartPositions.Write(lpSerialiser,
                              lpStaticLayout->maPartPositions.maElements,
                              lpStaticLayout->maPartPositions.muLength);
        maPartOrientations.Write(lpSerialiser,
                                 lpStaticLayout->maPartOrientations.maElements,
                                 lpStaticLayout->maPartOrientations.muLength);
        maPartTypes.Write(lpSerialiser,
                          lpStaticLayout->maPartTypes.maElements,
                          lpStaticLayout->maPartTypes.muLength);
        maPartIds.Write(lpSerialiser,
                        lpStaticLayout->maPartIds.maElements,
                        lpStaticLayout->maPartIds.muLength);
        maRecordedCells.Write(lpSerialiser,
                              lpStaticLayout->maRecordedCells.maElements,
                              lpStaticLayout->maRecordedCells.muLength);
    }

    void PropSerialiserFrame::KeyFrameWrite(BaseSerialiser* lpSerialiser,
                                            PropSerialiserFrame* lpStaticLayout)
    {
        CGS_ASSERT(maPropPositions.muLength == maTypes.muLength,
                   "maPropPositions.GetLength():  maTypes.GetLength(): ");
        CGS_ASSERT(maPropPositions.muLength == maPropOrientations.muLength,
                   "maPropPositions.GetLength():  maPropOrientations.GetLength(): ");

        WriteQuantisedPairs(lpSerialiser, maPropPositions, maPropOrientations);
        WriteQuantisedPairs(lpSerialiser, maPartPositions, maPartOrientations);

        // The four position/orientation arrays went out quantised above; the remaining five
        // still go through the array serialiser, which takes its own key-frame branch.
        maLoadedZones.Write(lpSerialiser,
                            lpStaticLayout->maLoadedZones.maElements,
                            lpStaticLayout->maLoadedZones.muLength);
        maTypes.Write(lpSerialiser,
                      lpStaticLayout->maTypes.maElements,
                      lpStaticLayout->maTypes.muLength);
        maPartTypes.Write(lpSerialiser,
                          lpStaticLayout->maPartTypes.maElements,
                          lpStaticLayout->maPartTypes.muLength);
        maPartIds.Write(lpSerialiser,
                        lpStaticLayout->maPartIds.maElements,
                        lpStaticLayout->maPartIds.muLength);
        maRecordedCells.Write(lpSerialiser,
                              lpStaticLayout->maRecordedCells.maElements,
                              lpStaticLayout->maRecordedCells.muLength);
    }
}
