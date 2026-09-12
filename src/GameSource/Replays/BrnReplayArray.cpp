#include "GameSource/Replays/BrnReplayArray.h"

#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"
#include "GameSource/Replays/Serialisers/BrnReplayPropSerialiserFrame.h"  // PropLoadedZoneRecord + the frame's element types

// The delta serialisation pair every BrnReplayArray in a prop-serialiser frame drives:
//   Read  : length byte, then key-frame verbatim OR delta {index, value} records.
//   Write : length byte, then key-frame verbatim OR delta records (changed prefix bits +
//           appended tail).
// Assert file/line strings in the image cite Replays/BrnReplayArray.h and
// Containers/CgsBitArray.h; CGS_ASSERT records this file's __FILE__/__LINE__ and the messages
// are preserved verbatim (the streamed value + trailing "\n" are dropped).
//
// Eight (T, N) pairs are instantiated at the bottom -- the nine sub-arrays of
// PropSerialiserFrame, two of which share <u16, 128>. Each instantiation's own body was read
// before it was listed; they are the SAME generic, with two things varying per T:
//   * the wire record stride, which ReplayArrayUpdateRecord<T> reproduces by alignment alone;
//   * the "has this element changed?" test, which is an exact inequality for the integer
//     elements and a tolerance compare for the two float element types (below).

namespace BrnReplays
{
    namespace
    {
        // The per-lane tolerance the float-element instantiations compare against, read from the
        // constant the changed-bit scan splats before the compare. The same constant serves both
        // the position and the orientation arrays.
        const f32 KF_ELEMENT_DIFFERENCE_TOLERANCE = 0.0001f;

        bool LaneDiffers(f32 lfA, f32 lfB)
        {
            const f32 lfDelta = lfA - lfB;
            const f32 lfMagnitude = (lfDelta < 0.0f) ? -lfDelta : lfDelta;
            return lfMagnitude > KF_ELEMENT_DIFFERENCE_TOLERANCE;
        }
    }

    // Position elements: a four-lane compare whose w lane is neutralised -- the scan rotates the
    // x-lane magnitude into the w slot before comparing, so the vector's unused fourth lane can
    // never make two positions look different. That is a three-lane (x, y, z) test.
    template <>
    struct DefaultAreDifferentFunctor<Vector3>
    {
        static bool AreDifferent(const Vector3& lrA, const Vector3& lrB)
        {
            return LaneDiffers(lrA.x, lrB.x)
                || LaneDiffers(lrA.y, lrB.y)
                || LaneDiffers(lrA.z, lrB.z);
        }
    };

    // Orientation elements: the same tolerance over all FOUR lanes -- the scan does not rotate a
    // lane away here, because w is the quaternion's scalar part and carries real information.
    template <>
    struct DefaultAreDifferentFunctor<rw::math::vpu::Quaternion>
    {
        static bool AreDifferent(const rw::math::vpu::Quaternion& lrA,
                                 const rw::math::vpu::Quaternion& lrB)
        {
            return LaneDiffers(lrA.x, lrB.x)
                || LaneDiffers(lrA.y, lrB.y)
                || LaneDiffers(lrA.z, lrB.z)
                || LaneDiffers(lrA.w, lrB.w);
        }
    };

    // Loaded-zone elements: the scan compares the zone id word and then walks BOTH 600-bit runs
    // field by field. The alignment word between the id and the first run is NOT compared -- the
    // scan steps straight from the id word to the run at +0x08 -- so it is skipped here too.
    template <>
    struct DefaultAreDifferentFunctor<PropLoadedZoneRecord>
    {
        static bool AreDifferent(const PropLoadedZoneRecord& lrA, const PropLoadedZoneRecord& lrB)
        {
            if (lrA.miZoneId != lrB.miZoneId)
                return true;

            typedef CgsContainers::BitArray<KU_PROPS_PER_ZONE> TPropBits;
            for (u32 luField = 0; luField < TPropBits::kuNumberOfBitFields; ++luField)
            {
                if (lrA.maPropsAddedToScene.GetBitField(luField)
                    != lrB.maPropsAddedToScene.GetBitField(luField))
                    return true;
                if (lrA.maPropsPreviouslyHit.GetBitField(luField)
                    != lrB.maPropsPreviouslyHit.GetBitField(luField))
                    return true;
            }
            return false;
        }
    };

    template <typename T, u8 N, typename TAreDifferent>
    void BrnReplayArray<T, N, TAreDifferent>::Read(BaseSerialiser* lpSerialiser)
    {
        u8 lu8Length = 0;
        lpSerialiser->ReadByte(&lu8Length);
        CGS_ASSERT(lu8Length <= N, "Bad array size: ");
        muLength = lu8Length;

        if (lpSerialiser->IsKeyFrame())
        {
            lpSerialiser->Read(maElements, static_cast<s32>(sizeof(T)) * lu8Length);
            return;
        }

        u8 lu8NumChanged = 0;
        lpSerialiser->ReadByte(&lu8NumChanged);
        CGS_ASSERT(lu8NumChanged <= muLength, "luNumChangedElements <= muLength");

        for (u32 luRecord = 0; luRecord < lu8NumChanged; ++luRecord)
        {
            ReplayArrayUpdateRecord<T> lRecord;
            lpSerialiser->Read(&lRecord, static_cast<s32>(sizeof(lRecord)));

            const u32 luIndex = lRecord.mu8Index;
            CGS_ASSERT(luIndex < muLength, "Bad update index: ");
            maElements[luIndex] = lRecord.muValue;
        }
    }

    template <typename T, u8 N, typename TAreDifferent>
    void BrnReplayArray<T, N, TAreDifferent>::Write(BaseSerialiser* lpSerialiser,
                                                    const T* lpPrevData,
                                                    u8 lu8PrevLength)
    {
        CGS_ASSERT(muLength <= N, "Bad array length: ");

        lpSerialiser->WriteByte(&muLength);

        if (lpSerialiser->IsKeyFrame())
        {
            lpSerialiser->Write(maElements, static_cast<s32>(sizeof(T)) * muLength);
            return;
        }

        CGS_ASSERT(lu8PrevLength <= N, "Bad previous array length: ");

        // Compare the shared prefix of the live and previous arrays; record a "changed" bit
        // for every element the policy calls different. The changed set is an inlined
        // CgsContainers::BitArray<N>.
        const u32 luCompareCount = (muLength >= lu8PrevLength) ? lu8PrevLength : muLength;
        CgsContainers::BitArray<N> lChangedBits;
        lChangedBits.UnSetAll();
        u32 luNumChanged = 0;
        for (u32 luElement = 0; luElement < luCompareCount; ++luElement)
        {
            if (TAreDifferent::AreDifferent(maElements[luElement], lpPrevData[luElement]))
            {
                CGS_ASSERT(luElement < N, "Index: ");
                lChangedBits.SetBit(luElement);
                ++luNumChanged;
            }
        }

        // Record count byte = changed-in-prefix + appended tail (muLength - luCompareCount).
        const u8 lu8NumRecords =
            static_cast<u8>(luNumChanged - luCompareCount + muLength);
        lpSerialiser->WriteByte(&lu8NumRecords);

        // Emit one record per set changed bit, walking the mask low-to-high (bounded to N).
        for (s32 liScan = lChangedBits.GetFirstNonZeroBit();
             liScan != CgsContainers::BitArray<N>::KI_INVALID_BITINDEX;
             liScan = lChangedBits.GetNextNonZeroBit(liScan))
        {
            ReplayArrayUpdateRecord<T> lRecord;
            lRecord.mu8Index = static_cast<u8>(liScan);
            lRecord.muValue  = maElements[liScan];
            lpSerialiser->Write(&lRecord, static_cast<s32>(sizeof(lRecord)));
        }

        // Append every element beyond the previous length: index runs prevLength.., value is the
        // live element.
        if (lu8PrevLength < muLength)
        {
            u8 lu8Index = lu8PrevLength;
            for (u32 luElement = lu8PrevLength; luElement < muLength; ++luElement)
            {
                ReplayArrayUpdateRecord<T> lRecord;
                lRecord.mu8Index = lu8Index;
                lRecord.muValue  = maElements[luElement];
                lpSerialiser->Write(&lRecord, static_cast<s32>(sizeof(lRecord)));
                lu8Index = static_cast<u8>(luElement + 1);
            }
        }
    }

    // -------- the eight instantiated (T, N) pairs: the nine sub-arrays of one prop-serialiser
    //          frame, with the part type-id and part-id arrays sharing <u16, 128>. --------
    template void BrnReplayArray<PropLoadedZoneRecord, KU_MAX_LOADED_ZONES>::Read(BaseSerialiser*);
    template void BrnReplayArray<u32, KU_MAX_RECORDED_CELLS>::Read(BaseSerialiser*);
    template void BrnReplayArray<Vector3, KU_MAX_RECORDED_PROPS>::Read(BaseSerialiser*);
    template void BrnReplayArray<rw::math::vpu::Quaternion, KU_MAX_RECORDED_PROPS>::Read(BaseSerialiser*);
    template void BrnReplayArray<u16, KU_MAX_RECORDED_PROPS>::Read(BaseSerialiser*);
    template void BrnReplayArray<Vector3, KU_MAX_RECORDED_PARTS>::Read(BaseSerialiser*);
    template void BrnReplayArray<rw::math::vpu::Quaternion, KU_MAX_RECORDED_PARTS>::Read(BaseSerialiser*);
    template void BrnReplayArray<u16, KU_MAX_RECORDED_PARTS>::Read(BaseSerialiser*);

    template void BrnReplayArray<PropLoadedZoneRecord, KU_MAX_LOADED_ZONES>::Write(
        BaseSerialiser*, const PropLoadedZoneRecord*, u8);
    template void BrnReplayArray<u32, KU_MAX_RECORDED_CELLS>::Write(BaseSerialiser*, const u32*, u8);
    template void BrnReplayArray<Vector3, KU_MAX_RECORDED_PROPS>::Write(
        BaseSerialiser*, const Vector3*, u8);
    template void BrnReplayArray<rw::math::vpu::Quaternion, KU_MAX_RECORDED_PROPS>::Write(
        BaseSerialiser*, const rw::math::vpu::Quaternion*, u8);
    template void BrnReplayArray<u16, KU_MAX_RECORDED_PROPS>::Write(BaseSerialiser*, const u16*, u8);
    template void BrnReplayArray<Vector3, KU_MAX_RECORDED_PARTS>::Write(
        BaseSerialiser*, const Vector3*, u8);
    template void BrnReplayArray<rw::math::vpu::Quaternion, KU_MAX_RECORDED_PARTS>::Write(
        BaseSerialiser*, const rw::math::vpu::Quaternion*, u8);
    template void BrnReplayArray<u16, KU_MAX_RECORDED_PARTS>::Write(BaseSerialiser*, const u16*, u8);
}
