// GameSource/Replays/Serialisers/BrnReplayPropSerialiserFrame_wQ2_keyframe.cpp
//
// Wave Q round 2 -- BrnReplays::PropSerialiserFrame::KeyFrameRead @0x826586B0 (295 insns).
// Partfile of the BrnReplayPropSerialiserFrame TU.
//
// ⚠️⚠️  CONDUCTOR: DO NOT MOUNT THIS FILE WITHOUT DELETING THE GATE.  ⚠️⚠️
// An inert boot gate for this exact symbol lives at
//   b5-decomp/src/GameSource/World/WorldLinkStubs.cpp:3855-3866
// and WorldLinkStubs.cpp IS mounted in tools/build/build_game_exe.bat. Adding this file to the
// link WITHOUT removing that block is LNK2005 (AGENTS.md gotcha 7: gates are retired by the
// conductor, together with the mount -- never by the implementing agent). That is why
// KeyFrameRead is in its own partfile instead of the sibling _wQ2_owner.cpp, whose six bodies
// have no gate anywhere and can be mounted on their own. Its three neighbours in that gate block
// (Read @0x82653120, Write @0x82657FE0, KeyFrameWrite) are still declared-only and their gates
// must STAY.
//
// ⚠️ LINK-LEVEL FACT (report, not a defect in this file): this body needs FOUR instantiations of
// BrnReplayArray<T,N>::Read, whose generic body is out-of-line in
// GameSource/Replays/BrnReplayArray.cpp with an explicit instantiation for <u32,4> ONLY. So
// <PropLoadedZoneRecord,9>::Read, <u16,254>::Read and <u16,128>::Read are UNRESOLVED EXTERNALS.
// BrnReplayArray.cpp/.h are NOT this owner's files; the note below is what its owner must do.
//
// ⚠️⚠️ CORRECTION (round 3). An earlier revision of this banner called that "a one-line-each fix
// (`template void BrnReplayArray<T,N>::Read(BaseSerialiser*);`)". THAT IS FALSE FOR ALL THREE and
// following it would hard-break the build. The committed generic Read/Write bodies are u32-ONLY:
// they move a FIXED 8-byte `ReplayArrayUpdateRecord {u8 mu8Index; u8 maPad01[3]; u32 muValue;}`
// (BrnReplayArray.h) and then do `maElements[luIndex] = static_cast<T>(lRecord.muValue)`.
//   * <PropLoadedZoneRecord,9>: `static_cast<PropLoadedZoneRecord>(u32)` does not compile at all.
//   * <u16,254> / <u16,128>: compiles, but serialises the WRONG WIRE FORMAT (8 bytes per delta
//     record instead of 4, and the value read from the wrong offset).
// The console's delta record is PER-T -- `{u8 index; T value;}` at natural alignment -- which I
// re-measured from the three Read bodies myself:
//     <u32,4>                    0x826512A8  record 8    (index @+0, value @+4)
//     <u16,254>                  0x82651980  `li r5,4`   @0x82651ACC -> record 4;
//                                            index `lbz var_BC` @0x82651ADC, value `lhz var_BA`
//                                            @0x82651BA0 -> value @+2
//     <u16,128>                  0x82652058  `li r5,4`   @0x826521A4; same var_BC/var_BA pair
//     <PropLoadedZoneRecord,9>   0x82651058  `li r5,0xB0` @0x826511A4 -> record 176;
//                                            buffer at var_140, index `lbz var_140` @0x826511B4,
//                                            payload `addi r4,r1,var_138` @0x82651280 into a
//                                            `li r5,0xA8` memcpy -> value @+8, 8 + 168 == 176
// WHAT IS ACTUALLY REQUIRED, before any of the three can be instantiated: BrnReplayArray.cpp's
// record type must first be generalised to `template<typename T> struct { u8 mu8Index; T muValue; }`
// and the assignment must become `maElements[luIndex] = lRecord.muValue` (no static_cast). Note
// Write additionally needs `operator!=` on T for its changed-bit prefix scan, which
// PropLoadedZoneRecord does not have today.
//
// =================================================================================================
// WHAT IT DOES (MEASURED from the per-address JSON `assembly`, instruction by instruction)
// =================================================================================================
// The key-frame (non-delta) playback path. Unlike the four u16/u32 sub-arrays -- which are read
// through BrnReplayArray<T,N>::Read -- the position+orientation pairs are NOT stored on the wire
// as two arrays. They ride ONE 12-byte QuantisedQuatPos record per element, which this function
// unpacks and splits back into the two arrays:
//
//   ReadByte(&count)                       ; sub_8264FCF0 == BaseSerialiser::ReadByte
//   maPropPositions.muLength    = count    ; stb 0x15F0   -- BOTH lengths are set from the ONE
//   maPropOrientations.muLength = count    ; stb 0x25E0      count byte, BEFORE the loop, which
//                                          ;                 is what makes the indexed writes
//                                          ;                 below pass operator[]'s bounds check
//   for i in [0, count):
//       Read(packed, 12)                   ; BaseSerialiser::Read @0x8264C188, `li r5, 0xC`
//       UnPack(working, packed)            ; QuantisedQuatPos::UnPack @0x82657E50, returns dest
//       maPropPositions[i]    = { working[4], working[5], working[6], 0.0f }
//       maPropOrientations[i] = { working[0], working[1], working[2], working[3] }
//   <the same block again for the PART arrays: count -> 0x2FF0 and 0x3800, elements into
//    0x27F0 and 0x3000>
//   maLoadedZones.Read()  maTypes.Read()  maPartTypes.Read()  maPartIds.Read()
//   maRecordedCells.Read()
//   assert(maPropPositions.GetLength() == maTypes.GetLength())            ; :420
//   assert(maPropPositions.GetLength() == maPropOrientations.GetLength()) ; :421
//
// The working buffer's float layout (quaternion in floats 0..3, position in floats 4..6) is read
// straight off the `lfs 0/4/8/0xC` and `lfs 0x10/0x14/0x18` pairs, and the w lane of the position
// is an explicit `stw r24(==0)`. This is the same 8-float working set the committed
// BrnReplayQuantisedQuatPos.h banner describes for the traffic serialiser's ReadAsQuatPos.
//
// THE TWO ASSERTS ARE THE SOURCE OF THREE MEMBER NAMES. Their streamed message fragments are, in
// the image, "maPropPositions.GetLength(): ", " maTypes.GetLength(): " and
// " maPropOrientations.GetLength(): ", baked against BrnReplayPropEntitySerialiser.h lines
// 0x1A4/0x1A5 (420/421) -- that is what pinned those three names in the header. Each streamed
// message collapses to one CGS_ASSERT carrying BOTH of that site's literals (only the streamed
// VALUE between them is dropped) -- the leading literal alone is shared by the two sites and
// would make them indistinguishable in the host log.
// Both asserts are NON-GATING on the console (it falls through and returns), and so here.
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
        // One packed orientation+position record on the wire (`li r5, 0xC` at both Read sites).
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
        // with only "maPropPositions.GetLength(): " the two console sites (:420 and :421) are
        // indistinguishable in the host log, which is a real loss of the console's own
        // discrimination. The dropped part is the streamed VALUE between the two fragments.
        CGS_ASSERT(maPropPositions.muLength == maTypes.muLength,
                   "maPropPositions.GetLength():  maTypes.GetLength(): ");
        CGS_ASSERT(maPropPositions.muLength == maPropOrientations.muLength,
                   "maPropPositions.GetLength():  maPropOrientations.GetLength(): ");
    }
}
