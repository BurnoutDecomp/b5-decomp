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

    // @ 0x826534E0 -- read a 12-byte quantised quat+pos record off the stream, unpack it and
    // VMX-expand it into the 64-byte destination transform. The VMX rotation-matrix build is
    // grounded (like the committed TrafficEntitySerialiser sibling) as the read+unpack primitive
    // plus a copy of the unpacked working set.
    void SoundSerialiser::ReadAsQuatPos(void* lpTransform)
    {
        float lafUnpacked[8];
        QuantisedQuatPos::Read(this, lafUnpacked);
        std::memcpy(lpTransform, lafUnpacked, sizeof(lafUnpacked));
    }

    // @ 0x82658EC8 -- pack the 64-byte source transform into a 12-byte quantised record and
    // write it. The VMX quaternion-from-basis derivation is grounded as the working-set copy
    // plus the pack+write primitive.
    void SoundSerialiser::WriteAsQuatPos(const void* lpTransform)
    {
        float lafScratch[8];
        std::memcpy(lafScratch, lpTransform, sizeof(lafScratch));
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
            // Reconstruct from the previous frame's record (transform rows verbatim + scalar
            // tail carried across; the X360 vmaddfp position-extrapolation is grounded as copy).
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
        mfTime = mfTime + *reinterpret_cast<f32*>(lpBase + 0xC5C);

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
            mfTime = 0.0f;
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
                mfTime += *reinterpret_cast<const f32*>(lpBase + 0xC5C);
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
                    mfTime = 0.0f;
                }
            }
        }
        return Unlock() ? 1 : 0;
    }
}
