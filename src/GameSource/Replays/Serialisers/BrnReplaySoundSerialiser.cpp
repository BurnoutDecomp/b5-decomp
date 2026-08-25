// BrnReplays::SoundSerialiser -- the sound replay serialiser channel. Reconstructed from
// BURNOUT_X360_ARTIST.XEX. Records / plays back the per-frame sound world (collisions,
// scrapes, traffic-entity sound sources) into the SoundSerialiserStaticLayout via the shared
// BaseSerialiser stream primitives. Every branch, offset and constant below is taken from the
// X360 ASM; the static-layout sub-records are reached by their X360-attested byte offsets (the
// layout's field TYPES are not attested). See BrnReplaySoundSerialiser.h for the address map.

#include "GameSource/Replays/Serialisers/BrnReplaySoundSerialiser.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameSource/Replays/BrnReplayQuantisedQuatPos.h"
#include "vendor/renderware/physics/JointFrames.hpp"   // rw::math::vpu::QuaternionFromMatrix33 (WriteAsQuatPos)

#include <cstring>

namespace BrnReplays
{
    // @ 0x82682340 -- return the serialiser's static buffer viewed as the structured
    // SoundSerialiser static layout (asserting the buffer is large enough).
    SoundSerialiserStaticLayout* SoundSerialiser::GetStaticLayout()
    {
        CGS_ASSERT(GetStaticBufferSize() >= SoundSerialiserStaticLayout::KI_MIN_STATIC_BUFFER_SIZE,
                   "Static buffer size is too small\n");
        return reinterpret_cast<SoundSerialiserStaticLayout*>(GetStaticBuffer());
    }

    // @ 0x82695AC8 -- append one collision record (224 bytes) at index GetNumCollisions() into
    // the static layout's collision array (record base +0, stride 224), then bump the count.
    SoundSerialiserStaticLayout* SoundSerialiser::AddCollision(const void* lpCollision)
    {
        CGS_ASSERT(GetNumCollisions() >= 0, "GetNumCollisions() >= 0");
        CGS_ASSERT(GetNumCollisions() < SoundSerialiserStaticLayout::KI_MAX_COLLISIONS,
                   "GetNumCollisions() < SoundSerialiserStaticLayout::KI_MAX_COLLISIONS");

        const s32 liIndex = GetStaticLayout()->GetNumCollisions();

        u8* lpDest = reinterpret_cast<u8*>(GetStaticLayout())
                   + SoundSerialiserStaticLayout::KU_COLLISION_STRIDE * liIndex;
        std::memcpy(lpDest, lpCollision, SoundSerialiserStaticLayout::KU_COLLISION_STRIDE);

        SoundSerialiserStaticLayout* lpResult = GetStaticLayout();
        ++lpResult->miNumCollisions;
        return lpResult;
    }

    // @ 0x826959B8 -- append one scrape record: 48 bytes of scalar scrape data (from lpScrape)
    // into the scrape-data array (base +0x710, stride 48), plus a 16-byte position vector into
    // the parallel scrape-vector array (base +0x7D0, stride 16), then bump the count.
    SoundSerialiserStaticLayout* SoundSerialiser::AddScrape(const void* lpScrape, SoundVec128 lvPosition)
    {
        CGS_ASSERT(GetNumScrapes() >= 0, "GetNumScrapes() >= 0");
        CGS_ASSERT(GetNumScrapes() < SoundSerialiserStaticLayout::KI_MAX_SCRAPES,
                   "GetNumScrapes() < SoundSerialiserStaticLayout::KI_MAX_SCRAPES");

        const s32 liIndex = GetStaticLayout()->GetNumScrapes();

        u8* lpData = reinterpret_cast<u8*>(GetStaticLayout())
                   + SoundSerialiserStaticLayout::KU_SCRAPE_DATA_BASE
                   + SoundSerialiserStaticLayout::KU_SCRAPE_DATA_STRIDE * liIndex;
        std::memcpy(lpData, lpScrape, SoundSerialiserStaticLayout::KU_SCRAPE_DATA_STRIDE);

        u8* lpVec = reinterpret_cast<u8*>(GetStaticLayout())
                  + SoundSerialiserStaticLayout::KU_SCRAPE_VEC_BASE
                  + SoundSerialiserStaticLayout::KU_SCRAPE_VEC_STRIDE * liIndex;
        std::memcpy(lpVec, &lvPosition, sizeof(lvPosition));

        SoundSerialiserStaticLayout* lpResult = GetStaticLayout();
        ++lpResult->miNumScrapes;
        return lpResult;
    }

    // @ 0x82695B88 -- append one traffic-entity record (80 bytes) into the traffic array (base
    // +0xA30, stride 80), reset that entity's parallel float slot (base +0xC18, stride 12) to
    // 0.0, then bump the count.
    SoundSerialiserStaticLayout* SoundSerialiser::AddTrafficEntity(const void* lpEntity)
    {
        CGS_ASSERT(GetNumTrafficEntities() >= 0, "GetNumTrafficEntities() >= 0");
        CGS_ASSERT(static_cast<u32>(GetNumTrafficEntities()) < KU_MAX_TRAFFIC_ENTITIES,
                   "GetNumTrafficEntities() < SoundSerialiserFrame::KI_MAX_TRAFFIC_ENTITIES");

        const s32 liIndex = GetStaticLayout()->GetNumTrafficEntities();

        u8* lpRecord = reinterpret_cast<u8*>(GetStaticLayout())
                     + SoundSerialiserStaticLayout::KU_TRAFFIC_RECORD_BASE
                     + SoundSerialiserStaticLayout::KU_TRAFFIC_RECORD_STRIDE * liIndex;
        std::memcpy(lpRecord, lpEntity, SoundSerialiserStaticLayout::KU_TRAFFIC_RECORD_STRIDE);

        f32* lpSlot = reinterpret_cast<f32*>(
            reinterpret_cast<u8*>(GetStaticLayout())
            + SoundSerialiserStaticLayout::KU_TRAFFIC_SLOT_BASE
            + SoundSerialiserStaticLayout::KU_TRAFFIC_SLOT_STRIDE * liIndex);
        *lpSlot = 0.0f;

        SoundSerialiserStaticLayout* lpResult = GetStaticLayout();
        ++lpResult->miNumTrafficEntities;
        return lpResult;
    }

    // @ 0x82682488 -- find the per-entity 12-byte record for the traffic entity whose recorded
    // id matches luEntityId. Linear scan over the recorded traffic entities (match key @
    // record+0x40, i.e. base+0xA70+80*i); returns null when the count is <= 0 or no match.
    void* SoundSerialiser::GetTrafficEnt(u32 luEntityId)
    {
        s32 liIndex = 0;
        if (GetStaticLayout()->GetNumTrafficEntities() <= 0)
        {
            return nullptr;
        }

        s32 liKeyOffset = 0;
        while (*reinterpret_cast<u32*>(
                   reinterpret_cast<u8*>(GetStaticLayout())
                   + SoundSerialiserStaticLayout::KU_TRAFFIC_KEY_BASE + liKeyOffset)
               != luEntityId)
        {
            ++liIndex;
            liKeyOffset += SoundSerialiserStaticLayout::KU_TRAFFIC_RECORD_STRIDE;
            if (liIndex >= GetStaticLayout()->GetNumTrafficEntities())
            {
                return nullptr;
            }
        }

        return reinterpret_cast<u8*>(GetStaticLayout())
             + SoundSerialiserStaticLayout::KU_TRAFFIC_ENT_BASE
             + SoundSerialiserStaticLayout::KU_TRAFFIC_ENT_STRIDE * liIndex;
    }

    // @ 0x826534E0 -- read a 12-byte quantised quat+pos record off the stream, unpack it
    // (working set: quat x/y/z/w @[0..3], position @[4..6]) and expand it into the 64-byte
    // destination transform: three rotation rows @+0/+0x10/+0x20 + the position row @+0x30
    // (the asm stores pos.w = 0 explicitly: `stw r10=0, var_34`).
    //
    // RECONSTRUCTED 2026-08-25 wave 4 (was "grounded as copy" -- half the destination was
    // never written and the rotation build was absent). The 29-instruction VMX body is the
    // SAME quat->matrix idiom as the committed rw::physics::Quaternion::UnitQuaternionToMatrix
    // (vendor Quaternion.cpp) and PropSerialiserFrame::GetPropTransform @0x822BB920: Q = q *
    // gSqrt2s, diagonals via vnmsubfp (0.5 - Q^2) + the .yzxx fold, cross terms Q*Q.yzxx +/-
    // Qw*Q.zxyx, rows routed through the SAME perm constants unk_82CDA3D0/450/410 + vrlimi128
    // mask=2 rotates 0/3/2 (dumped + decoded; the nine terms below are verbatim the committed
    // vendor body's). No normalise -- the console assumes the stored quaternion is unit and
    // never writes back (same as GetPropTransform).
    //
    // ROW-W NOTE (the PropSerialiserFrame precedent, applied identically): the console's three
    // full stvx128 row stores leave each basis row's w lane holding a permute LEFTOVER
    // (decoded: row0.w=m22, row1.w=Dx, row2.w=Sx). No consumer reads those lanes; reproducing
    // indeterminate-looking junk hides bugs, so they are written as a determinate 0.0f --
    // deliberately not byte-identical in the three pad lanes, identical in every consumed lane.
    void SoundSerialiser::ReadAsQuatPos(void* lpTransform)
    {
        float lafUnpacked[8];
        QuantisedQuatPos::Read(this, lafUnpacked);

        const f32 lfX = lafUnpacked[0], lfY = lafUnpacked[1];
        const f32 lfZ = lafUnpacked[2], lfW = lafUnpacked[3];

        const f32 lfXY = 2.0f * lfX * lfY, lfYZ = 2.0f * lfY * lfZ, lfZX = 2.0f * lfZ * lfX;
        const f32 lfWX = 2.0f * lfW * lfX, lfWY = 2.0f * lfW * lfY, lfWZ = 2.0f * lfW * lfZ;
        const f32 lfXX = 2.0f * lfX * lfX, lfYY = 2.0f * lfY * lfY, lfZZ = 2.0f * lfZ * lfZ;

        f32* lpDst = static_cast<f32*>(lpTransform);
        lpDst[0]  = 1.0f - lfYY - lfZZ;   // row0 (stvx128 -> dst+0x00)
        lpDst[1]  = lfXY + lfWZ;
        lpDst[2]  = lfZX - lfWY;
        lpDst[3]  = 0.0f;                 // ROW-W note above
        lpDst[4]  = lfXY - lfWZ;          // row1 (-> dst+0x10)
        lpDst[5]  = 1.0f - lfXX - lfZZ;
        lpDst[6]  = lfYZ + lfWX;
        lpDst[7]  = 0.0f;
        lpDst[8]  = lfZX + lfWY;          // row2 (-> dst+0x20)
        lpDst[9]  = lfYZ - lfWX;
        lpDst[10] = 1.0f - lfXX - lfYY;
        lpDst[11] = 0.0f;
        lpDst[12] = lafUnpacked[4];       // position row (-> dst+0x30)
        lpDst[13] = lafUnpacked[5];
        lpDst[14] = lafUnpacked[6];
        lpDst[15] = 0.0f;                 // the asm's explicit zero store
    }

    // @ 0x82658EC8 -- derive the quaternion from the 64-byte source transform's three basis
    // rows and pack {quat, position} into the 12-byte quantised record.
    //
    // RECONSTRUCTED 2026-08-25 wave 4 (was "grounded as copy" -- the derivation was absent and
    // the first two ROWS were passed where the quat belongs). The console's matrix->quaternion
    // block (diagonal splats vxor'd with the gQuatFromMat_{x,y,z}Signs masks
    // unk_8327F120/F100/F0F0, all-four-candidates + vrsqrtefp + 2 Newton-Raphson + vsel
    // cascade) IS rw::math::vpu::QuaternionFromMatrix33 -- the committed helper recovered from
    // the ORIGINAL Feb-2007 rwmath source (JointFrames.hpp; see the PropSerialiserFrame
    // wQ2_owner banner for the full instruction-level identification, done once for the
    // identical Write{Prop,Part} inline). Called here exactly as WriteProp does; epsilon 0.0f
    // (lane 0 of unk_8201444C). The sqrt de-optimisation is the SDK's own scalar twin.
    void SoundSerialiser::WriteAsQuatPos(const void* lpTransform)
    {
        const f32* lpSrc = static_cast<const f32*>(lpTransform);

        rw::math::vpu::Matrix33 lBasis;
        lBasis.xAxis.x = lpSrc[0];  lBasis.xAxis.y = lpSrc[1];  lBasis.xAxis.z = lpSrc[2];
        lBasis.yAxis.x = lpSrc[4];  lBasis.yAxis.y = lpSrc[5];  lBasis.yAxis.z = lpSrc[6];
        lBasis.zAxis.x = lpSrc[8];  lBasis.zAxis.y = lpSrc[9];  lBasis.zAxis.z = lpSrc[10];

        const rw::math::vpu::Quaternion lQuat =
            rw::math::vpu::QuaternionFromMatrix33(lBasis, 0.0f);

        float lafScratch[8];
        lafScratch[0] = lQuat.x;
        lafScratch[1] = lQuat.y;
        lafScratch[2] = lQuat.z;
        lafScratch[3] = lQuat.w;
        lafScratch[4] = lpSrc[12];   // position (src+0x30)
        lafScratch[5] = lpSrc[13];
        lafScratch[6] = lpSrc[14];
        lafScratch[7] = 0.0f;        // unused tail lane of the working set
        QuantisedQuatPos::Write(this, lafScratch);
    }

    // @ 0x82653608 -- playback one 80-byte sound key-frame record: the quat+pos transform, then
    // the scalar/float/half/byte tail, then a packed bit-byte expanded into four 1-byte bools,
    // then a trailing byte.
    void SoundSerialiser::ReadKeyFrame(u8* lpKeyFrame)
    {
        ReadAsQuatPos(lpKeyFrame);
        BaseSerialiser::Read(lpKeyFrame + 0x40, 4);
        BaseSerialiser::ReadFloat(lpKeyFrame + 0x44);
        BaseSerialiser::Read(lpKeyFrame + 0x48, 2);
        BaseSerialiser::ReadByte(lpKeyFrame + 0x4A);

        u8 luFlags = 0;
        BaseSerialiser::ReadByte(&luFlags);
        lpKeyFrame[0x4B] = luFlags & 1;
        lpKeyFrame[0x4C] = (luFlags >> 1) & 1;
        lpKeyFrame[0x4D] = (luFlags >> 2) & 1;
        lpKeyFrame[0x4E] = (luFlags >> 3) & 1;

        BaseSerialiser::ReadByte(lpKeyFrame + 0x4F);
    }

    // @ 0x826594E8 -- record one 80-byte sound key-frame record (write mirror of ReadKeyFrame):
    // the quat+pos transform, the scalar/float/half/byte tail, the four bool lanes packed into
    // one byte (four bits, no 0x10 sentinel), and the trailing byte.
    void SoundSerialiser::WriteKeyFrame(const u8* lpKeyFrame)
    {
        WriteAsQuatPos(lpKeyFrame);
        BaseSerialiser::Write(lpKeyFrame + 0x40, 4);
        BaseSerialiser::WriteFloat(lpKeyFrame + 0x44);
        BaseSerialiser::Write(lpKeyFrame + 0x48, 2);
        BaseSerialiser::WriteByte(lpKeyFrame + 0x4A);

        u8 luFlags = static_cast<u8>(
              ((lpKeyFrame[0x4B] != 0) ? 1 : 0)
            | ((lpKeyFrame[0x4C] != 0) ? 2 : 0)
            | ((lpKeyFrame[0x4D] != 0) ? 4 : 0)
            | ((lpKeyFrame[0x4E] != 0) ? 8 : 0));
        BaseSerialiser::WriteByte(&luFlags);

        BaseSerialiser::WriteByte(lpKeyFrame + 0x4F);
    }

    // @ 0x826536C0 -- playback one delta-compressed update frame. When the per-record flag byte
    // has bit4 (0x10) set the record is a full key-frame read; otherwise it is reconstructed from
    // the previous frame's record (grounded here as a verbatim row copy). The low four flag bits
    // are always unpacked into the record's four 1-byte bools.
    void SoundSerialiser::ReadUpdateFrame(s32 liIndex, u8* lpCurrent, u8* lpPrevious)
    {
        u8* lpDest = lpCurrent + 80 * liIndex;
        u8* lpPrev = lpPrevious + 80 * liIndex;

        // *(lpPrevious+0x228): the previous FRAME's traffic-entity count (frame base).
        CGS_ASSERT(*reinterpret_cast<const u32*>(lpPrevious + 0x228) <= KU_MAX_TRAFFIC_ENTITIES,
                   "SoundSerialiser: Invalid number of traffic entities last key frame when decompressing replay data.");

        u8 luFlags = 0;
        BaseSerialiser::ReadByte(&luFlags);

        if ((luFlags & 0x10) != 0)
        {
            ReadAsQuatPos(lpDest);
            BaseSerialiser::Read(lpDest + 0x40, 4);
            BaseSerialiser::ReadFloat(lpDest + 0x44);
            BaseSerialiser::Read(lpDest + 0x48, 2);
            BaseSerialiser::ReadByte(lpDest + 0x4A);
            BaseSerialiser::ReadByte(lpDest + 0x4F);
        }
        else
        {
            // Reconstruct from the previous frame's record: the four transform rows copied
            // verbatim (stvx128 x4), the scalar tail carried across, and then -- RECONSTRUCTED
            // 2026-08-25 wave 4 (was "grounded as copy") -- the position row OVERWRITTEN with
            // the extrapolation the asm computes @0x82653858-0x82653860:
            //     destRow3 = prevRow3 + prevRow2 * splat(prev.mfField44)
            //                                   * splat(this->mfAccumulatedFrameTime)
            // (vmulfp v0 = prevRow2 * splat(prev+0x44); vmaddfp v0,v0,prevRow3,splat(this+0x5C)
            //  in RAW FIELD ORDER vD,vA,vB,vC == vA*vC + vB -- the calibration pinned by the
            //  PropSerialiserFrame wQ2 banner. Physically: position += direction * speed * dt.)
            std::memcpy(lpDest, lpPrev, 0x40);
            *reinterpret_cast<u32*>(lpDest + 0x40) = *reinterpret_cast<const u32*>(lpPrev + 0x40);
            *reinterpret_cast<f32*>(lpDest + 0x44)  = *reinterpret_cast<const f32*>(lpPrev + 0x44);
            *reinterpret_cast<u16*>(lpDest + 0x48)  = *reinterpret_cast<const u16*>(lpPrev + 0x48);
            lpDest[0x4A] = lpPrev[0x4A];
            lpDest[0x4B] = lpPrev[0x4B];
            lpDest[0x4C] = lpPrev[0x4C];
            lpDest[0x4D] = lpPrev[0x4D];
            lpDest[0x4E] = lpPrev[0x4E];
            lpDest[0x4F] = lpPrev[0x4F];

            const f32  lfScale   = *reinterpret_cast<const f32*>(lpPrev + 0x44)   // serialised record +0x44 (mfField44)
                                 * mfAccumulatedFrameTime;
            const f32* lpfRow2   = reinterpret_cast<const f32*>(lpPrev + 0x20);
            const f32* lpfRow3   = reinterpret_cast<const f32*>(lpPrev + 0x30);
            f32*       lpfDstPos = reinterpret_cast<f32*>(lpDest + 0x30);
            for (int liLane = 0; liLane < 4; ++liLane)   // all four stvx128 lanes
                lpfDstPos[liLane] = lpfRow2[liLane] * lfScale + lpfRow3[liLane];
        }

        // Always overwrite the four bool lanes from the flag byte.
        lpDest[0x4B] = luFlags & 1;
        lpDest[0x4C] = (luFlags & 2) != 0;
        lpDest[0x4D] = (luFlags & 4) != 0;
        lpDest[0x4E] = (luFlags & 8) != 0;
    }

    // @ 0x826595D0 -- record one traffic-entity UPDATE frame (delta path of Write). A leading
    // flag byte records which fields changed vs. the key frame; only when the record changed is
    // the full record streamed. Mirrors WriteKeyFrame field-for-field.
    s32 SoundSerialiser::WriteUpdateFrame(s32 liEntity, u8* lpCurrentBase, u8* lpKeyBase)
    {
        SoundSerialiserTrafficEntityRecord* lpCurrent =
            reinterpret_cast<SoundSerialiserTrafficEntityRecord*>(lpCurrentBase + 80 * liEntity);
        SoundSerialiserTrafficEntityRecord* lpKey =
            reinterpret_cast<SoundSerialiserTrafficEntityRecord*>(lpKeyBase + 80 * liEntity);

        // lwz 0x228(a4) -> the key frame's recorded traffic-entity count.
        const s32 liKeyCount = *reinterpret_cast<const s32*>(lpKeyBase + 0x228);
        CGS_ASSERT(static_cast<u32>(liKeyCount) <= 6u,
                   "SoundSerialiser: Invalid number of traffic entities last key frame when compressing replay data.");

        // Changed vs. key frame: this record did not exist in the key frame (liEntity >= count),
        // or its identity word (+0x40) differs, or its 2-byte field (+0x48) differs.
        u8 luChanged = 0;
        if (liEntity >= liKeyCount ||
            lpCurrent->muId != lpKey->muId ||
            lpCurrent->muField48 != lpKey->muField48)
        {
            luChanged = 1;
        }

        // Pack the per-field change mask into a single byte:
        //   bit0=(+0x4B!=0) bit1=(+0x4C!=0) bit2=(+0x4D!=0) bit3=(+0x4E!=0) bit4=(changed!=0).
        const u8 luByte4E = (lpCurrent->maTail[0x4E - 0x4A] != 0);
        const u8 luByte4D = (lpCurrent->maTail[0x4D - 0x4A] != 0);
        const u8 luByte4C = (lpCurrent->maTail[0x4C - 0x4A] != 0);
        const u8 luByte4B = (lpCurrent->maTail[0x4B - 0x4A] != 0);
        u8 luFlags =
            static_cast<u8>(
                (2 *
                 ((2 *
                   ((2 * (luByte4E | (2 * (luChanged != 0)))) | luByte4D)) |
                  luByte4C)) |
                luByte4B);

        s32 liResult = BaseSerialiser::WriteByte(&luFlags);
        if (luChanged != 0)
        {
            WriteAsQuatPos(lpCurrent);
            BaseSerialiser::Write(&lpCurrent->muId, 4);
            BaseSerialiser::WriteFloat(&lpCurrent->mfField44);
            BaseSerialiser::Write(&lpCurrent->muField48, 2);
            BaseSerialiser::WriteByte(&lpCurrent->maTail[0x4A - 0x4A]);
            liResult = BaseSerialiser::WriteByte(&lpCurrent->maTail[0x4F - 0x4A]);
        }
        return liResult;
    }

    // @ 0x826590E0 -- playback: deserialise a whole sound frame from the replay stream into
    // lpStatic. The static-layout sub-records are reached by their X360-attested byte offsets.
    s32 SoundSerialiser::Read(SoundSerialiserStaticLayout* lpStatic)
    {
        Lock();
        CGS_ASSERT(lpStatic != nullptr, "lpStatic");

        u8* lpBase = reinterpret_cast<u8*>(lpStatic);

        reinterpret_cast<CgsModule::VariableEventQueue<512, 16>*>(lpBase + 0x814)->Construct();

        const EMode leMode = GetMode();
        const bool lbKeyFramePath =
            leMode == E_MODE_RECORDING_STALLED || leMode == E_MODE_PLAYING_STALLED ||
            leMode == E_MODE_RECORDING_PREPARING || leMode == E_MODE_PLAYING_PREPARING ||
            leMode == E_MODE_RESTORING;

        if (lbKeyFramePath)
        {
            if (mbIsKeyFrame)
            {
                mbExtraFlag = false;
            }
            return Unlock() ? 1 : 0;
        }

        if (!mbExtraFlag && !mbIsKeyFrame)
        {
            return Unlock() ? 1 : 0;
        }

        // --- delta-time seed (a2+0xC5C) ---
        if (mbIsKeyFrame)
        {
            mbExtraFlag = true;
            ReadFloat(lpBase + 0xC5C);
        }
        else
        {
            *reinterpret_cast<f32*>(lpBase + 0xC5C) = *reinterpret_cast<f32*>(lpBase + 0xE8C);
        }
        mfAccumulatedFrameTime = mfAccumulatedFrameTime + *reinterpret_cast<f32*>(lpBase + 0xC5C);   // += the serialised blob frame delta (this+0x5C, NOT mfTime@+0x54)

        ReadVariableQueue(reinterpret_cast<CgsModule::VariableEventQueue<512, 16>*>(lpBase + 0x814));
        ReadFloat(lpBase + 0xA24);

        // --- collisions (record anchor a2+0x1C, stride 224, count @a2+0x700) ---
        Read(lpBase + 0x700, 4);
        s32 liNumCollisions = *reinterpret_cast<s32*>(lpBase + 0x700);
        if (liNumCollisions > 0)
        {
            u8* lpRec = lpBase + 0x1C;
            do
            {
                Read(lpRec - 4, 4);
                Read(lpRec, 4);
                Read(lpRec + 116, 8);
                Read(lpRec + 20, 4);
                Read(lpRec - 28, 4);
                Read(lpRec + 32, 4);
                Read(lpRec - 24, 4);
                Read(lpRec + 28, 4);
                ReadFloat(lpRec + 180);
                Read(lpRec + 100, 16);
                Read(lpRec + 4, 16);
                *(lpRec + 188) = 0;
                std::memset(lpRec + 132, 0, 48);
                --liNumCollisions;
                lpRec += 224;
            } while (liNumCollisions != 0);
        }

        // --- scrapes (data anchor a2+0x720 stride 48, vec base a2+0x7D0 stride 16, count @a2+0x810) ---
        Read(lpBase + 0x810, 4);
        s32 liNumScrapes = *reinterpret_cast<s32*>(lpBase + 0x810);
        if (liNumScrapes > 0)
        {
            u8* lpVec  = lpBase + 0x7D0;
            u8* lpData = lpBase + 0x720;
            do
            {
                Read(lpData + 12, 4);
                Read(lpData, 4);
                Read(lpData + 4, 4);
                Read(lpData + 16, 4);
                ReadFloat(lpData + 20);
                *(lpData + 25) = 1;
                Read(lpVec, 16);
                --liNumScrapes;
                lpVec  += 16;
                lpData += 48;
            } while (liNumScrapes != 0);
        }

        // --- traffic entities (count @a2+0xC58) ---
        Read(lpBase + 0xC58, 4);
        s32 liNumTraffic = *reinterpret_cast<s32*>(lpBase + 0xC58);

        if (mbIsKeyFrame)
        {
            if (liNumTraffic > 0)
            {
                u8* lpKf = lpBase + 0xA30;
                s32 liLeft = liNumTraffic;
                do
                {
                    ReadKeyFrame(lpKf);
                    --liLeft;
                    lpKf += 80;
                } while (liLeft != 0);
            }
        }
        else
        {
            for (s32 liEntity = 0; liEntity < liNumTraffic; ++liEntity)
            {
                ReadUpdateFrame(liEntity, lpBase + 0xA30, lpBase + 0xC60);
            }
        }

        // --- per-entity change bits (flag @a2+0xA7E stride 80, record @a2+0xC12 stride 12) ---
        if (liNumTraffic > 0)
        {
            u8* lpFlag = lpBase + 0xA7E;
            u8* lpEnt  = lpBase + 0xC12;
            do
            {
                if (*lpFlag)
                {
                    u8 luBits = 0;
                    ReadByte(&luBits);
                    *(lpEnt - 2) = static_cast<u8>(luBits & 1);
                    *(lpEnt - 1) = static_cast<u8>((luBits >> 1) & 1);
                    *(lpEnt + 0) = static_cast<u8>((luBits >> 2) & 1);
                    *(lpEnt + 1) = static_cast<u8>((luBits >> 3) & 1);
                    *(lpEnt + 2) = static_cast<u8>((luBits >> 4) & 1);
                    ReadFloat(lpEnt + 6);
                }
                --liNumTraffic;
                lpFlag += 80;
                lpEnt  += 12;
            } while (liNumTraffic != 0);
        }

        if (mbIsKeyFrame)
        {
            *reinterpret_cast<SoundSerialiserFrame*>(lpBase + 0xC60) =
                *reinterpret_cast<SoundSerialiserFrame*>(lpBase + 0xA30);
            mfAccumulatedFrameTime = 0.0f;   // stfs 0x5C (key-frame reset)
        }

        return Unlock() ? 1 : 0;
    }

    // @ 0x8265BDD0 -- record: serialise a whole sound frame out of lpStatic. Runs only for the
    // recording family (modes {1,2,3}) with a non-null static layout.
    s32 SoundSerialiser::Write(SoundSerialiserStaticLayout* lpStatic)
    {
        Lock();
        if (lpStatic)
        {
            u8* lpBase = reinterpret_cast<u8*>(lpStatic);
            const EMode leMode = GetMode();
            if (leMode == E_MODE_RECORDING_PREPARING ||
                leMode == E_MODE_RECORDING ||
                leMode == E_MODE_RECORDING_STALLED)
            {
                if (mbIsKeyFrame)
                {
                    BaseSerialiser::WriteFloat(lpBase + 0xC5C);
                }
                mfAccumulatedFrameTime += *reinterpret_cast<const f32*>(lpBase + 0xC5C);   // += the serialised blob frame delta (this+0x5C, NOT mfTime@+0x54)
                WriteVariableQueue(reinterpret_cast<CgsModule::VariableEventQueue<512, 16>*>(lpBase + 0x814));
                BaseSerialiser::WriteFloat(lpBase + 0xA24);

                // --- collisions ---
                s32 liCount = *reinterpret_cast<const s32*>(lpBase + 0x700);
                CGS_ASSERT(static_cast<u32>(liCount) <= static_cast<u32>(SoundSerialiserStaticLayout::KI_MAX_COLLISIONS),
                           "SoundSerialiser: Invalid number of collisions when compressing replay data.");
                BaseSerialiser::Write(&liCount, 4);
                {
                    u8* lpRec = lpBase + 0x1C;
                    for (s32 li = liCount; li > 0; --li, lpRec += 0xE0)
                    {
                        BaseSerialiser::Write(lpRec - 4, 4);
                        BaseSerialiser::Write(lpRec, 4);
                        BaseSerialiser::Write(lpRec + 0x74, 8);
                        BaseSerialiser::Write(lpRec + 0x14, 4);
                        BaseSerialiser::Write(lpRec - 0x1C, 4);
                        BaseSerialiser::Write(lpRec + 0x20, 4);
                        BaseSerialiser::Write(lpRec - 0x18, 4);
                        BaseSerialiser::Write(lpRec + 0x1C, 4);
                        BaseSerialiser::WriteFloat(lpRec + 0xB4);
                        BaseSerialiser::Write(lpRec + 0x64, 16);
                        BaseSerialiser::Write(lpRec + 4, 16);
                    }
                }

                // --- scrapes ---
                liCount = *reinterpret_cast<const s32*>(lpBase + 0x810);
                CGS_ASSERT(static_cast<u32>(liCount) <= static_cast<u32>(SoundSerialiserStaticLayout::KI_MAX_SCRAPES),
                           "SoundSerialiser: Invalid number of scrapes when compressing replay data.");
                BaseSerialiser::Write(&liCount, 4);
                {
                    u8* lpPayload = lpBase + 0x7D0;
                    u8* lpRec     = lpBase + 0x720;
                    for (s32 li = liCount; li > 0; --li, lpPayload += 0x10, lpRec += 0x30)
                    {
                        BaseSerialiser::Write(lpRec + 0xC, 4);
                        BaseSerialiser::Write(lpRec, 4);
                        BaseSerialiser::Write(lpRec + 4, 4);
                        BaseSerialiser::Write(lpRec + 0x10, 4);
                        BaseSerialiser::WriteFloat(lpRec + 0x14);
                        BaseSerialiser::Write(lpPayload, 16);
                    }
                }

                // --- traffic entities ---
                liCount = *reinterpret_cast<const s32*>(lpBase + 0xC58);
                CGS_ASSERT(static_cast<u32>(liCount) <= KU_MAX_TRAFFIC_ENTITIES,
                           "SoundSerialiser: Invalid number of traffic entities when compressing replay data.");
                BaseSerialiser::Write(&liCount, 4);

                if (mbIsKeyFrame)
                {
                    u8* lpRec = lpBase + 0xA30;
                    for (s32 li = liCount; li > 0; --li, lpRec += 0x50)
                    {
                        WriteKeyFrame(lpRec);
                    }
                }
                else
                {
                    for (s32 li = 0; li < liCount; ++li)
                    {
                        WriteUpdateFrame(li, lpBase + 0xA30, lpBase + 0xC60);
                    }
                }

                // Trailing per-record flag/float pack (runs for BOTH the key-frame and update
                // path when liCount>0). For each gated traffic record (a2+0xA7E stride 0x50),
                // pack the five status lanes (base a2+0xC12 stride 0xC) into a byte -- lanes are
                // (byte != 0) for ALL FIVE bits (bit4 = (C14!=0)*0x10) -- then write the float.
                {
                    const u8* lpGate = lpBase + 0xA7E;
                    const u8* lpStat = lpBase + 0xC12;
                    for (s32 li = liCount; li > 0; --li, lpGate += 0x50, lpStat += 0xC)
                    {
                        if (*lpGate)
                        {
                            u8 luByte = static_cast<u8>(
                                  ((lpStat[-2] != 0) ? 1 : 0)
                                | ((lpStat[-1] != 0) ? 2 : 0)
                                | ((lpStat[0]  != 0) ? 4 : 0)
                                | ((lpStat[1]  != 0) ? 8 : 0)
                                | ((lpStat[2]  != 0) ? 0x10 : 0));
                            BaseSerialiser::WriteByte(&luByte);
                            BaseSerialiser::WriteFloat(lpStat + 6);
                        }
                    }
                }

                if (mbIsKeyFrame)
                {
                    *reinterpret_cast<SoundSerialiserFrame*>(lpBase + 0xC60) =
                        *reinterpret_cast<const SoundSerialiserFrame*>(lpBase + 0xA30);
                    mfAccumulatedFrameTime = 0.0f;   // stfs 0x5C (key-frame reset)
                }
            }
        }
        return Unlock() ? 1 : 0;
    }
}
