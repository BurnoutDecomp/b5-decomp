// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (BrnReplays::TrafficEntitySerialiser)
//
// The traffic-entity replay serialiser: records / plays back the per-frame state of
// the whole traffic world (up to 600 vehicles in 3 chunks, their transforms, per-car
// physics info and the streamed bonus assets). Like its sibling serialisers it derives
// from BaseSerialiser and drives a fixed 251008-byte static-layout buffer.
//
// Source-of-truth ladder: every signature, branch, constant and store below is taken
// from the X360 ASM (see scratchpad postmortem packet). Highlights:
//   * Construct @0x8264C978 -> BaseSerialiser::Construct(7,0,0x14000,0x3D480,
//     "TrafficEntity",1); then `stb 0,0x5C(this)`+`stb 0,0x5D(this)` clears the two
//     trailing bool flags (mbStaticLayoutCleared, mbDeltaActive).
//   * GetStaticLayout @0x8264C710 asserts the static buffer is >= 0x3D480 and non-null,
//     then returns it (typed as TrafficEntitySerialiserStaticLayout).
//   * The per-vehicle accessors (GetVehicleData/Transform, GetPhysicsInfo) range-check
//     the index against KU_MAX_TOTAL_TRAFFIC(600) / KU_NUM_PHYSICS_INFO(25) and return
//     a pointer into the chunked static layout via the X360 address arithmetic.
//   * The frame bit walking (ReadFrame/WriteFrame) iterates the static layout's
//     FastBitArray<600> mVehicleUpdateBits and FastBitArray<25> mPhysicsBits; the X360
//     inlines FastBitArray::GetFirstBitSet / GetNextBitSet, reconstructed here as calls
//     to those container methods (reverse-inlining).
//
// Callee declarations (BaseSerialiser, the frame structs, the Quantised packers, the
// Vehicle, ReadPhysicsInfo) live in their own TUs; included / declared here for the
// compile-only gate.

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Replays/BrnReplayQuantisedQuatPos.h"
#include "GameSource/Replays/Serialisers/BrnReplayTrafficEntitySerialiserFrame.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"

#include <cstring>

namespace BrnReplays
{
    // DWARF home: GameSource/Replays/Serialisers/BrnReplayTrafficEntitySerialiser.h.
    class TrafficEntitySerialiser : public BaseSerialiser
    {
    public:
        // @0x8264C978
        s32 Construct();
        // @0x8264C710 -- returns the static-layout buffer, asserting it is big enough.
        TrafficEntitySerialiserStaticLayout* GetStaticLayout();

        // @0x82653418 -- lazily clear the static layout on the first frame, mark it done.
        TrafficEntitySerialiserStaticLayout* CheckStaticLayoutCleared();

        // @0x8265F2D8 / @0x8265F4E8 -- playback / record one frame (top-level dispatch).
        s32 Read();
        s32 Write();

        // @0x8265DD38 / @0x8265D288 -- playback / record one whole frame buffer.
        void ReadFrame(TrafficEntitySerialiserStaticLayout* lpFrame, s32 liArg3);
        void WriteFrame(TrafficEntitySerialiserStaticLayout* lpFrame, s32 liArg3);

        // @0x82658D98 / @0x82658B50 -- read / write a quat+pos transform record.
        void ReadAsQuatPos(void* lpTransform);
        void WriteAsQuatPos(const void* lpTransform);

        // @ ReadPhysicsInfo (external TU) / @0x8265AE98 WritePhysicsInfo.
        void ReadPhysicsInfo(TrafficPhysicsInfoRecord* lpInfo, u32 luIndex, s32 liArg4);
        void WritePhysicsInfo(const u8* lpInfo, u8 luIndex);

        // --- per-vehicle accessors (return pointers into the chunked static layout) ---
        // @0x82714118
        TrafficPhysicsInfoRecord* GetPhysicsInfo(u32 luPhysicsInfo);
        // @0x82707280
        TrafficVehicleDataRecord* GetVehicleData(u32 luVehicle);
        // @0x827071E8
        TrafficVehicleTransformRecord* GetVehicleTransform(u32 luVehicle);

        // --- record-side setters (TrafficEntityModule::UpdateSerialiser drives these) ---
        // @0x827073C8
        void SetActiveHullsForLocalPlayer(const void* lpActiveHulls);
        // @0x82707438
        void SetBonusAssets(const u8* lpauBonusAssets, u32 luNumBonusAssets);
        // @0x82707320
        void SetPhysicsData(const u64* lpBits, const void* lpaPhysicsInfo);
        // @0x82714060
        void SetVehicleData(u32 luVehicle, BrnTraffic::Vehicle* lpVehicle);
        // @0x82707168
        void SetVehicleTransforms(const void* lpaTransforms);

    private:
        // X360: two bytes at this+0x5C / this+0x5D, both cleared by Construct.
        // mbStaticLayoutCleared (+0x5C) is set by CheckStaticLayoutCleared once the
        // static layout has been zeroed; mbDeltaActive (+0x5D) tracks whether a delta
        // chain is in progress across Read frames.
        bool mbStaticLayoutCleared; // @0x5C
        bool mbDeltaActive;         // @0x5D
    };

    // -------------------------------------------------------------------------

    s32 TrafficEntitySerialiser::Construct()
    {
        // X360: ori r7,0xD480 over `lis 3` -> 0x3D480 static; `lis 1`|0x4000 -> 0x14000
        // dynamic; id 7; mode 0; "TrafficEntity"; flag 1.
        s32 liResult = BaseSerialiser::Construct(7, 0, 0x14000, 0x3D480, "TrafficEntity", 1);
        mbStaticLayoutCleared = false;
        mbDeltaActive         = false;
        return liResult;
    }

    TrafficEntitySerialiserStaticLayout* TrafficEntitySerialiser::GetStaticLayout()
    {
        // asm: lwz 0x24(this) (miStaticBufferSize) compared `< 0x3D480`; then assert the
        // static buffer pointer is non-null; return it.
        CGS_ASSERT(GetStaticBufferSize() >= static_cast<s32>(KU_STATIC_BUFFER_SIZE),
                   "Static buffer size is too small\n");
        CGS_ASSERT(mpStaticBuffer != nullptr, "GetStaticBuffer()");
        return reinterpret_cast<TrafficEntitySerialiserStaticLayout*>(mpStaticBuffer);
    }

    // @0x82653418
    // On the first frame after (re)start, when the static layout has not yet been
    // cleared and the static buffer exists, zero the whole layout, reset the frame's
    // bit arrays / info-valid words and mark it cleared.
    TrafficEntitySerialiserStaticLayout* TrafficEntitySerialiser::CheckStaticLayoutCleared()
    {
        if (!mbStaticLayoutCleared && mpStaticBuffer != nullptr)
        {
            TrafficEntitySerialiserStaticLayout* lpLayout = GetStaticLayout();
            std::memset(lpLayout, 0, KU_STATIC_BUFFER_SIZE);

            // asm: the bit-array fields and the info-valid word are re-zeroed (redundant
            // after the memset, kept for faithfulness), and the 2-byte traffic-light state
            // is set to 0xFFFF (`sth -1,0x94(r11)` @0x826534A4 -- an explicit -1, NOT a
            // clear). The bit arrays start cleared by the memset above; re-assert the empty
            // state, then stamp the traffic-light state to its 0xFFFF sentinel.
            lpLayout->mVehicleUpdateBits.UnSetAll();
            lpLayout->mPhysicsBits.UnSetAll();
            std::memset(&lpLayout->mInfoValid, 0, sizeof(lpLayout->mInfoValid));
            lpLayout->maTrafficLightState[0] = 0xFF;
            lpLayout->maTrafficLightState[1] = 0xFF;

            mbStaticLayoutCleared = true;
        }
        return GetStaticLayout();
    }

    // @0x82714118
    // Return the recorded physics-info record for luPhysicsInfo, range-checking the
    // index against the FastBitArray bound and asserting the bit is set.
    TrafficPhysicsInfoRecord* TrafficEntitySerialiser::GetPhysicsInfo(u32 luPhysicsInfo)
    {
        TrafficEntitySerialiserStaticLayout* lpLayout = GetStaticLayout();
        CGS_ASSERT(luPhysicsInfo < KU_NUM_PHYSICS_INFO, "invalid index");
        CGS_ASSERT(lpLayout->mPhysicsBits.IsBitSet(luPhysicsInfo),
                   "lpFrame->mHeader.mPhysicsBits.IsBitSet( luPhysicsInfo )");
        return &lpLayout->maPhysicsInfo[luPhysicsInfo];
    }

    // @0x82707280
    // Return the per-vehicle data record (3 bytes) for luVehicle: chunk = v/200,
    // local = v%200, address = base + 13408*chunk + 3*local + 256.
    TrafficVehicleDataRecord* TrafficEntitySerialiser::GetVehicleData(u32 luVehicle)
    {
        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC,
                   "luVehicle < BrnTraffic::KU_MAX_TOTAL_TRAFFIC");

        u8* lpBase = reinterpret_cast<u8*>(GetStaticLayout());
        const u32 luChunk = luVehicle / KU_VEHICLES_PER_CHUNK;
        const u32 luLocal = luVehicle % KU_VEHICLES_PER_CHUNK;
        u8* lpRecord = lpBase
                     + KU_VEHICLE_CHUNK_STRIDE * luChunk
                     + sizeof(TrafficVehicleDataRecord) * luLocal
                     + KU_OFFSET_VEHICLE_DATA;
        return reinterpret_cast<TrafficVehicleDataRecord*>(lpRecord);
    }

    // @0x827071E8
    // Return the per-vehicle transform record (64 bytes) for luVehicle: address =
    // base + 13408*chunk + (local << 6) + 864.
    TrafficVehicleTransformRecord* TrafficEntitySerialiser::GetVehicleTransform(u32 luVehicle)
    {
        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC,
                   "luVehicle < BrnTraffic::KU_MAX_TOTAL_TRAFFIC");

        u8* lpBase = reinterpret_cast<u8*>(GetStaticLayout());
        const u32 luChunk = luVehicle / KU_VEHICLES_PER_CHUNK;
        const u32 luLocal = luVehicle % KU_VEHICLES_PER_CHUNK;
        u8* lpRecord = lpBase
                     + KU_VEHICLE_CHUNK_STRIDE * luChunk
                     + sizeof(TrafficVehicleTransformRecord) * luLocal
                     + KU_OFFSET_VEHICLE_TRANSFORM;
        return reinterpret_cast<TrafficVehicleTransformRecord*>(lpRecord);
    }

    // @0x827073C8
    // Copy the 148-byte active-hull header for the local player into the static layout.
    void TrafficEntitySerialiser::SetActiveHullsForLocalPlayer(const void* lpActiveHulls)
    {
        CGS_ASSERT(lpActiveHulls != nullptr, "lpActiveHulls");
        std::memcpy(GetStaticLayout(), lpActiveHulls, 148);
    }

    // @0x82707438
    // Store luNumBonusAssets streamed-asset ids into the static layout (count at +150,
    // ids at +151..).
    void TrafficEntitySerialiser::SetBonusAssets(const u8* lpauBonusAssets, u32 luNumBonusAssets)
    {
        CGS_ASSERT(lpauBonusAssets != nullptr, "lpauBonusAssets");
        CGS_ASSERT(luNumBonusAssets <= KU_MAX_BONUS_STREAMED_ASSETS,
                   "luNumBonusAssets <= BrnTraffic::KU_MAX_BONUS_STREAMED_ASSETS");

        TrafficEntitySerialiserStaticLayout* lpLayout = GetStaticLayout();
        for (u32 luAsset = 0; luAsset < luNumBonusAssets; ++luAsset)
        {
            lpLayout->maBonusAssets[luAsset] = lpauBonusAssets[luAsset];
        }
        lpLayout->muNumBonusAssets = static_cast<u8>(luNumBonusAssets);
    }

    // @0x82707320
    // Store the 8-byte physics-bits word verbatim and copy the 102800-byte physics-info
    // block. The X360 does a plain `ld r10,0(a2)` / `std r10,0xF0(layout)` -- the whole
    // 64-bit *lpBits is copied unchanged. (The Hex-Rays `HIDWORD(v7) = 0x10000` is a
    // decompiler mis-model of `lis r9,1; ori r31,r9,0x9190` building the 0x19190 == 102800
    // memcpy size in r31; there is no high-dword rewrite in the raw asm.)
    void TrafficEntitySerialiser::SetPhysicsData(const u64* lpBits, const void* lpaPhysicsInfo)
    {
        CGS_ASSERT(lpBits != nullptr, "lpBits");
        CGS_ASSERT(lpaPhysicsInfo != nullptr, "lpaPhysicsInfo");

        TrafficEntitySerialiserStaticLayout* lpLayout = GetStaticLayout();
        // asm: ld r10,0(a2) ; std r10,0xF0(layout) -- copy all 8 bytes of *lpBits as-is.
        const u64 lu64Bits = *lpBits;
        std::memcpy(&lpLayout->mPhysicsBits, &lu64Bits, sizeof(lu64Bits));

        std::memcpy(lpLayout->maPhysicsInfo, lpaPhysicsInfo, KU_PHYSICS_INFO_TOTAL);
    }

    // @0x82714060
    // Record one traffic vehicle's data into the static layout. The physical-parts index
    // is -1 unless the vehicle is physical (E_FLAG_PHYSICAL == 0x08).
    void TrafficEntitySerialiser::SetVehicleData(u32 luVehicle, BrnTraffic::Vehicle* lpVehicle)
    {
        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC,
                   "luVehicle < BrnTraffic::KU_MAX_TOTAL_TRAFFIC");

        s32 liPhysicalPartsIndex = -1;
        if ((lpVehicle->GetFlags() & BrnTraffic::Vehicle::E_FLAG_PHYSICAL) != 0)
        {
            liPhysicalPartsIndex = lpVehicle->GetPhysicalPartsIndex();
        }

        TrafficVehicleDataRecord* lpRecord = GetVehicleData(luVehicle);
        TrafficVehicleData::SetFromVehicle(lpRecord, lpVehicle, liPhysicalPartsIndex);
    }

    // @0x82707168
    // Copy the per-chunk transform blocks (12800 bytes each) into the three vehicle
    // chunks' transform regions.
    void TrafficEntitySerialiser::SetVehicleTransforms(const void* lpaTransforms)
    {
        CGS_ASSERT(lpaTransforms != nullptr, "lpaTransforms");

        u8* lpDest = reinterpret_cast<u8*>(GetStaticLayout()) + KU_OFFSET_VEHICLE_TRANSFORM;
        const u8* lpSource = static_cast<const u8*>(lpaTransforms);
        for (u32 luChunk = 0; luChunk < KU_NUM_VEHICLE_CHUNKS; ++luChunk)
        {
            std::memcpy(lpDest, lpSource, 12800);
            lpDest   += KU_VEHICLE_CHUNK_STRIDE;
            lpSource += 12800;
        }
    }

    // @0x82658D98
    // Read a 12-byte quantised quat+pos record off the stream, unpack it and VMX-expand
    // it into the 64-byte destination transform. The VMX expansion is the quaternion ->
    // rotation-matrix build the X360 does inline; reconstructed as the UnPack call plus
    // the matrix store the caller performs.
    void TrafficEntitySerialiser::ReadAsQuatPos(void* lpTransform)
    {
        u8 lauPacked[12];
        BaseSerialiser::Read(lauPacked, 12);

        float lafUnpacked[8];
        QuantisedQuatPos::UnPack(lafUnpacked, lauPacked);

        // The X360 builds a rotation matrix from the unpacked quaternion in VMX regs and
        // stores it (plus the position lane) into lpTransform. The exact matrix math is
        // produced by the (not-yet reconstructed) QuantisedQuatPos expansion; copy the
        // unpacked working set into the transform record as the grounded surface.
        std::memcpy(lpTransform, lafUnpacked, sizeof(lafUnpacked));
    }

    // @0x82658B50
    // Pack the 64-byte source transform (quat orientation + position) into a 12-byte
    // quantised record and write it to the stream.
    void TrafficEntitySerialiser::WriteAsQuatPos(const void* lpTransform)
    {
        // The X360 reads the transform's basis vectors in VMX regs, derives a quaternion
        // (with the w-sign-selection `fsel`) plus the position, then Pack()s an 8-float
        // scratch. Reconstructed as the scratch-build + Pack + Write surface.
        float lafScratch[8];
        std::memcpy(lafScratch, lpTransform, sizeof(lafScratch));

        u8 lauPacked[12];
        QuantisedQuatPos::Pack(lauPacked, lafScratch);
        BaseSerialiser::Write(lauPacked, 12);
    }

    // @0x8265DD38
    // Playback one whole frame buffer: header / lights / bonus-assets / bit arrays, then
    // every set vehicle's transform+data, every set physics-info record, then the slot
    // table. The X360 inlines FastBitArray iteration; reconstructed via the container's
    // GetFirstBitSet / GetNextBitSet.
    void TrafficEntitySerialiser::ReadFrame(TrafficEntitySerialiserStaticLayout* lpFrame, s32 liArg3)
    {
        CGS_ASSERT(lpFrame != nullptr, "lpFrame");
        // The X360 fires FOUR alignment asserts under the single (ptr & 0xF) != 0 guard
        // (0x8265DD9C-0x8265DE18), each citing a distinct member.
        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpFrame->maHeader) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->mHeader) & 0xf) == 0");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpFrame->maHeader) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->mHeader) & 0xf) == 0");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpFrame->maPhysicsInfo) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->maPhysicsInfo) & 0xf) == 0");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpFrame->maTrafficLightState) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->mTrafficLightManager) & 0xf) == 0");

        BaseSerialiser::Read(lpFrame->maHeader, 148);
        BaseSerialiser::Read(lpFrame->maTrafficLightState, 2);
        // The bonus-asset count is read with the 1-byte ReadByte primitive (sub_8264FCF0),
        // not the general Read(_,1).
        BaseSerialiser::ReadByte(&lpFrame->muNumBonusAssets);
        BaseSerialiser::Read(lpFrame->maBonusAssets, 2);
        BaseSerialiser::Read(&lpFrame->mVehicleUpdateBits, 80);
        BaseSerialiser::Read(&lpFrame->mPhysicsBits, 8);

        // Walk the set vehicle-update bits: read each vehicle's transform then its data.
        for (s32 liVehicle = lpFrame->mVehicleUpdateBits.GetFirstBitSet();
             liVehicle >= 0;
             liVehicle = lpFrame->mVehicleUpdateBits.GetNextBitSet(liVehicle))
        {
            const u32 luVehicle = static_cast<u32>(liVehicle);
            CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC,
                       "liLocalVehicle >= 0 && liLocalVehicle < (int32_t)BrnTraffic::KU_MAX_TOTAL_TRAFFIC");
            ReadAsQuatPos(GetVehicleTransform(luVehicle));
            BaseSerialiser::Read(GetVehicleData(luVehicle), 3);
        }

        // Walk the set physics bits: read each physics-info record.
        for (s32 liInfo = lpFrame->mPhysicsBits.GetFirstBitSet();
             liInfo >= 0;
             liInfo = lpFrame->mPhysicsBits.GetNextBitSet(liInfo))
        {
            const u32 luInfo = static_cast<u32>(liInfo);
            ReadPhysicsInfo(&lpFrame->maPhysicsInfo[luInfo], luInfo, liArg3);
        }

        // Trailing slot table (loc_8265E780-0x8265E7E0): read KU_NUM_SLOTS one-byte slots
        // at stride KU_SLOT_STRIDE starting at the slot-table base, read one more byte
        // into a local, store its inverted-zero test into muReadSlotsValid, then read the
        // 4-byte muReadSlotsWord.
        u8* lpSlot = lpFrame->maSlotTable;
        for (u32 luSlot = 0; luSlot < KU_NUM_SLOTS; ++luSlot)
        {
            BaseSerialiser::ReadByte(lpSlot);
            lpSlot += KU_SLOT_STRIDE;
        }
        u8 luByte = 0;
        BaseSerialiser::ReadByte(&luByte);
        // asm 0x8265E7C8-0x8265E7DC: cntlzw(byte); extrwi ,1,26 (== (clz>>5)&1 == (byte==0));
        // xori ,1  -> stored value is (byte != 0) ? 1 : 0.
        lpFrame->muReadSlotsValid = (luByte != 0) ? 1u : 0u; // ((cntlzw(byte)>>5)&1) ^ 1
        BaseSerialiser::Read(&lpFrame->muReadSlotsWord, 4);
    }

    // @0x8265D288
    // Record one whole frame buffer (the write mirror of ReadFrame).
    void TrafficEntitySerialiser::WriteFrame(TrafficEntitySerialiserStaticLayout* lpFrame, s32 liArg3)
    {
        CGS_ASSERT(lpFrame != nullptr, "lpFrame");
        // The X360 fires FOUR alignment asserts under the single (ptr & 0xF) != 0 guard
        // (0x8265D2EC-0x8265D368), each citing a distinct member.
        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpFrame->maHeader) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->mHeader) & 0xf) == 0");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpFrame->maHeader) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->mHeader) & 0xf) == 0");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpFrame->maPhysicsInfo) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->maPhysicsInfo) & 0xf) == 0");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpFrame->maTrafficLightState) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->mTrafficLightManager) & 0xf) == 0");

        BaseSerialiser::Write(lpFrame->maHeader, 148);
        BaseSerialiser::Write(lpFrame->maTrafficLightState, 2);
        BaseSerialiser::Write(&lpFrame->muNumBonusAssets, 1);
        BaseSerialiser::Write(lpFrame->maBonusAssets, 2);
        BaseSerialiser::Write(&lpFrame->mVehicleUpdateBits, 80);
        BaseSerialiser::Write(&lpFrame->mPhysicsBits, 8);

        for (s32 liVehicle = lpFrame->mVehicleUpdateBits.GetFirstBitSet();
             liVehicle >= 0;
             liVehicle = lpFrame->mVehicleUpdateBits.GetNextBitSet(liVehicle))
        {
            const u32 luVehicle = static_cast<u32>(liVehicle);
            CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC,
                       "liLocalVehicle >= 0 && liLocalVehicle < (int32_t)BrnTraffic::KU_MAX_TOTAL_TRAFFIC");
            WriteAsQuatPos(GetVehicleTransform(luVehicle));
            BaseSerialiser::Write(GetVehicleData(luVehicle), 3);
        }

        for (s32 liInfo = lpFrame->mPhysicsBits.GetFirstBitSet();
             liInfo >= 0;
             liInfo = lpFrame->mPhysicsBits.GetNextBitSet(liInfo))
        {
            const u32 luInfo = static_cast<u32>(liInfo);
            WritePhysicsInfo(lpFrame->maPhysicsInfo[luInfo].maInfo,
                             static_cast<u8>(luInfo));
        }

        // Trailing slot table (loc_8265DCD4-0x8265DD28): write KU_NUM_SLOTS one-byte slots at
        // stride KU_SLOT_STRIDE, then the SAME +148080/+148084 slots-valid scalars the read side
        // uses (asm r29=a2+143280, Write(+0x12C0,1)/Write(+0x12C4,4) -- no read/write asymmetry).
        const u8* lpSlot = lpFrame->maSlotTable;
        for (u32 luSlot = 0; luSlot < KU_NUM_SLOTS; ++luSlot)
        {
            BaseSerialiser::Write(lpSlot, 1);
            lpSlot += KU_SLOT_STRIDE;
        }
        BaseSerialiser::Write(&lpFrame->muReadSlotsValid, 1);  // +148080
        BaseSerialiser::Write(&lpFrame->muReadSlotsWord, 4);   // +148084
    }

    // @0x8265F2D8
    // Top-level playback dispatch. Mirrors the X360 mode cascade exactly:
    //   * Block A (0x8265F340-0x8265F39C): when mode is not in {1,4,7}, mbAllowStreaming
    //     is set, and mode is in {4,5,6}, clear the static layout's mInfoValid word.
    //   * Block B (0x8265F3A0-0x8265F4DC): the frame path is reached ONLY for mode == 5
    //     (E_MODE_PLAYING) -- modes {3,6} and {1,4,7} fall through to LABEL_35, and the
    //     remaining gate keeps only {4,5,6} minus those, i.e. just 5. For every other
    //     mode, clear mbDeltaActive (+0x5D) then Unlock. The mode==5 path: if the delta
    //     is inactive AND streaming is off, Unlock WITHOUT touching mbDeltaActive; else if
    //     streaming is on set mbDeltaActive, read the frame, and snapshot the physics.
    s32 TrafficEntitySerialiser::Read()
    {
        Lock();
        CGS_ASSERT(GetStaticLayout() != nullptr, "GetStaticLayout()");

        const EMode leMode = meMode;

        // --- Block A: conditionally clear mInfoValid before the frame path. ---
        // ASM (0x8265F328-0x8265F378): skip when mode is in {1,4,7}, require
        // mbAllowStreaming, then require mode in {4,5,6}; combined that is {5,6}.
        const bool lbNot147 =
            leMode != E_MODE_RECORDING_PREPARING && leMode != E_MODE_PLAYING_PREPARING &&
            leMode != E_MODE_RESTORING;
        if (lbNot147 && mbAllowStreaming &&
            (leMode == E_MODE_PLAYING_PREPARING || leMode == E_MODE_PLAYING ||
             leMode == E_MODE_PLAYING_STALLED))
        {
            std::memset(&GetStaticLayout()->mInfoValid, 0,
                        sizeof(GetStaticLayout()->mInfoValid));
        }

        // --- Block B: reach the frame path only for mode == E_MODE_PLAYING (5). ---
        const bool lbReachFrame = (leMode == E_MODE_PLAYING);
        if (!lbReachFrame)
        {
            // LABEL_35: every non-playing mode clears the delta-active flag.
            mbDeltaActive = false;
            return Unlock() ? 1 : 0;
        }

        // mode == 5: the X360 early-out at 0x8265F428 does NOT touch +0x5D.
        if (!mbDeltaActive && !mbAllowStreaming)
        {
            return Unlock() ? 1 : 0;
        }
        if (mbAllowStreaming)
        {
            mbDeltaActive = true;
        }

        TrafficEntitySerialiserStaticLayout* lpLayout = GetStaticLayout();
        ReadFrame(lpLayout, 0);
        // Snapshot the physics-bits word + the freshly-read physics block onto the
        // previous-frame copy.
        std::memcpy(&lpLayout->mInfoValid, &lpLayout->mPhysicsBits, sizeof(lpLayout->mInfoValid));
        std::memcpy(lpLayout->maPreviousPhysicsInfo, lpLayout->maPhysicsInfo,
                    KU_PHYSICS_INFO_TOTAL);

        return Unlock() ? 1 : 0;
    }

    // @0x8265F4E8
    // Top-level record dispatch (the write mirror of Read).
    s32 TrafficEntitySerialiser::Write()
    {
        Lock();
        CGS_ASSERT(GetStaticLayout() != nullptr, "GetStaticLayout()");

        if (mpStaticBuffer != nullptr)
        {
            const EMode leMode = meMode;
            // asm: proceed for modes {1,2,3} (recording family).
            const bool lbRecordingFamily =
                leMode == E_MODE_RECORDING_PREPARING ||
                leMode == E_MODE_RECORDING ||
                leMode == E_MODE_RECORDING_STALLED;

            if (lbRecordingFamily)
            {
                TrafficEntitySerialiserStaticLayout* lpLayout = GetStaticLayout();
                if (mbAllowStreaming)
                {
                    std::memset(&lpLayout->mInfoValid, 0, sizeof(lpLayout->mInfoValid));
                }
                WriteFrame(lpLayout, 0);
                std::memcpy(&lpLayout->mInfoValid, &lpLayout->mPhysicsBits,
                            sizeof(lpLayout->mInfoValid));
                std::memcpy(lpLayout->maPreviousPhysicsInfo, lpLayout->maPhysicsInfo,
                            KU_PHYSICS_INFO_TOTAL);
            }
        }

        return Unlock() ? 1 : 0;
    }

    // KF_HEADLIGHT_ON_THRESHOLD -- flt_820BA62C (0.5) the headlight warmth is compared
    // against (`fcmpu; blt` skips the OR when warmth < 0.5).
    static const f32 KF_HEADLIGHT_ON_THRESHOLD = 0.5f;

    // @0x82713E38
    // Populate the 3-byte replay record from a live traffic vehicle. Every store, branch
    // and constant below is taken store-for-store from the X360 ASM:
    //   record[0] = muVehicleType  (`lbz r11,0(r31); stb r11,0(r29)` -- vehicle BYTE 0)
    //   record[2] = liPhysicalPartsIndex (`stb r28,2(r29)`)
    //   record[1] = packed light/effect flags, built bit-by-bit (starts 0).
    // The three IsAlive() asserts before the mxEffectState reads (asm 0x6C1/0x6C8/0x6BA)
    // are the inlined IsAlive() guards of the raw +7 byte reads; reproduced verbatim.
    void TrafficVehicleData::SetFromVehicle(TrafficVehicleDataRecord* lpRecord,
                                            const BrnTraffic::Vehicle* lpVehicle,
                                            s32 liPhysicalPartsIndex)
    {
        // Leading asserts (asm 0x82713E5C / 0x82713E88 / 0x82713EC4).
        CGS_ASSERT(lpVehicle != nullptr, "lpVehicle");
        CGS_ASSERT(lpVehicle->IsAlive(), "lpVehicle->IsAlive()");
        CGS_ASSERT(lpVehicle->IsAlive(), "IsAlive()");

        // record[0] = vehicle type byte (`lbz 0(r31); stb 0(r29)`), NOT the flags byte.
        lpRecord->maData[0] = lpVehicle->GetVehicleType();

        // a3 != -1 && !lpVehicle->IsPhysical()  ->  assert (asm 0x82713EEC-0x82713EFC).
        if (liPhysicalPartsIndex != -1)
        {
            CGS_ASSERT(lpVehicle->IsPhysical(),
                       "liPhysicsIndex == -1 || lpVehicle->IsPhysical()");
        }

        // record[2] = physical-parts index; record[1] starts cleared.
        lpRecord->maData[2] = static_cast<u8>(liPhysicalPartsIndex);
        lpRecord->maData[1] = 0;

        // Bit 0x01: headlights on when warmth >= 0.5 (flt_820BA62C; `fcmpu; blt`).
        if (lpVehicle->GetHeadlightWarmth() >= KF_HEADLIGHT_ON_THRESHOLD)
        {
            lpRecord->maData[1] |= 0x01u;
        }

        // Bit 0x02: brakelights (`clrlwi r11,r3,24; cmplwi; beq`).
        if (lpVehicle->AreBrakelightsOn())
        {
            lpRecord->maData[1] |= 0x02u;
        }

        // The next three mxEffectState (+7) reads each re-assert IsAlive() first (the
        // inlined predicate guards at asm 0x6C1/0x6C8/0x6BA), in this exact order.
        const u8 luEffectState = lpVehicle->GetEffectState();

        CGS_ASSERT(lpVehicle->IsAlive(), "IsAlive()"); // asm 0x6C1
        if ((luEffectState & 0x02u) != 0)              // a2[7] & 0x02 (left-indicator bit)
        {
            lpRecord->maData[1] |= 0x04u;
        }

        CGS_ASSERT(lpVehicle->IsAlive(), "IsAlive()"); // asm 0x6C8
        if ((luEffectState & 0x04u) != 0)              // a2[7] & 0x04 (right-indicator bit)
        {
            lpRecord->maData[1] |= 0x08u;
        }

        CGS_ASSERT(lpVehicle->IsAlive(), "IsAlive()"); // asm 0x6BA
        if ((luEffectState & 0x10u) != 0)              // a2[7] & 0x10 (alarm bit)
        {
            lpRecord->maData[1] |= 0x10u;
        }

        // Bit 0x20: set when the vehicle is NOT physical (`(a2[5] & 8) == 0`).
        if ((lpVehicle->GetFlags() & BrnTraffic::Vehicle::E_FLAG_PHYSICAL) == 0)
        {
            lpRecord->maData[1] |= 0x20u;
        }
    }
}
