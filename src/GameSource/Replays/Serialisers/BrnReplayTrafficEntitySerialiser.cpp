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
// Vehicle) live in their own TUs; the physics record codec is recovered below for the
// compile-only gate.

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Replays/Serialisers/BrnReplayTrafficEntitySerialiser.h"
#include "GameSource/Replays/BrnReplayQuantisedQuatPos.h"
#include "GameSource/Replays/Serialisers/BrnReplayTrafficEntitySerialiserFrame.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"

#include <cstring>
#include <cmath>
#include "vendor/renderware/physics/JointFrames.hpp"
#include "GameSource/Replays/BrnReplayQuantisedVector4.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"

namespace BrnReplays
{
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
    void TrafficEntitySerialiser::CheckStaticLayoutCleared()
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
            lpLayout->muLocalPlayerHull = 0xFFFF;

            mbStaticLayoutCleared = true;
        }
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
    void TrafficEntitySerialiser::SetActiveHullsForLocalPlayer(const ::Set<u16, 72>* lpActiveHulls)
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
    void TrafficEntitySerialiser::SetPhysicsData(const CgsContainers::BitArray<25>* lpBits, const BrnTraffic::TrafficPhysicsInfo* lpaPhysicsInfo)
    {
        CGS_ASSERT(lpBits != nullptr, "lpBits");
        CGS_ASSERT(lpaPhysicsInfo != nullptr, "lpaPhysicsInfo");

        TrafficEntitySerialiserStaticLayout* lpLayout = GetStaticLayout();
        // asm: ld r10,0(a2) ; std r10,0xF0(layout) -- copy all 8 bytes of *lpBits as-is.
        u64 lu64Bits;
        std::memcpy(&lu64Bits, lpBits, sizeof(lu64Bits));
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
        alignas(16) u8 lauPacked[16];
        BaseSerialiser::Read(lauPacked, 12);
        float lafUnpacked[8];
        QuantisedQuatPos::UnPack(lafUnpacked, lauPacked);
        // ARTIST 82658E34..82658EA8: the rwmath scaled-quaternion builder,
        // identical to the inline in RigidBody::DynamicUpdate. Keep fused neg_qq
        // and the three original permutation leftovers in basis W lanes.
        const float lfSX = 1.4142135381698608f * lafUnpacked[0];
        const float lfSY = 1.4142135381698608f * lafUnpacked[1];
        const float lfSZ = 1.4142135381698608f * lafUnpacked[2];
        const float lfSW = 1.4142135381698608f * lafUnpacked[3];
        const float lfDX = std::fmaf(-lfSX, lfSX, 0.5f);
        const float lfDY = std::fmaf(-lfSY, lfSY, 0.5f);
        const float lfDZ = std::fmaf(-lfSZ, lfSZ, 0.5f);
        const float lfXY = lfSX * lfSY, lfYZ = lfSY * lfSZ, lfZX = lfSZ * lfSX;
        const float lfWZ = lfSW * lfSZ, lfWX = lfSW * lfSX, lfWY = lfSW * lfSY;
        auto& lrTransform = *static_cast<Matrix44Affine*>(lpTransform);
        lrTransform.xAxis = {lfDY + lfDZ, lfXY + lfWZ, lfZX - lfWY, lfDX + lfDY};
        lrTransform.yAxis = {lfXY - lfWZ, lfDZ + lfDX, lfYZ + lfWX, lfXY - lfWZ};
        lrTransform.zAxis = {lfZX + lfWY, lfYZ - lfWX, lfDX + lfDY, lfXY + lfWZ};
        lrTransform.wAxis = {lafUnpacked[4], lafUnpacked[5], lafUnpacked[6], 0.0f};
    }

    // @0x82658B50
    // Pack the 64-byte source transform (quat orientation + position) into a 12-byte
    // quantised record and write it to the stream.
    void TrafficEntitySerialiser::WriteAsQuatPos(const void* lpTransform)
    {
        const auto& lrTransform = *static_cast<const Matrix44Affine*>(lpTransform);
        rw::math::vpu::Matrix33 lBasis;
        lBasis.xAxis = lrTransform.xAxis;
        lBasis.yAxis = lrTransform.yAxis;
        lBasis.zAxis = lrTransform.zAxis;
        const rw::math::vpu::Quaternion lQuat = rw::math::vpu::QuaternionFromMatrix33(lBasis, 0.0f);
        float lafScratch[8] = {lQuat.x, lQuat.y, lQuat.z, lQuat.w,
                              lrTransform.Pos().x, 0.0f, lrTransform.Pos().z, 0.0f};
        // ARTIST 82658D58..82658D6C, flt_8206CEB4=-900. Unordered selects the floor.
        const float lfY = lrTransform.Pos().y;
        lafScratch[5] = (lfY - -900.0f >= 0.0f) ? lfY : -900.0f;
        alignas(16) u8 lauPacked[16];
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
        CGS_ASSERT((reinterpret_cast<uintptr_t>(&lpFrame->mHeader) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->mHeader) & 0xf) == 0");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(&lpFrame->mHeader) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->mHeader) & 0xf) == 0");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpFrame->maPhysicsInfo) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->maPhysicsInfo) & 0xf) == 0");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpFrame->maTrafficLightManager) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->mTrafficLightManager) & 0xf) == 0");

        BaseSerialiser::Read(&lpFrame->mHeader, 148);
        BaseSerialiser::Read(&lpFrame->muLocalPlayerHull, 2);
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
        u8* lpSlot = (lpFrame->maTrafficLightManager + 5);
        for (u32 luSlot = 0; luSlot < KU_NUM_SLOTS; ++luSlot)
        {
            BaseSerialiser::ReadByte(lpSlot);
            lpSlot += KU_SLOT_STRIDE;
        }
        u8 luByte = 0;
        BaseSerialiser::ReadByte(&luByte);
        // asm 0x8265E7C8-0x8265E7DC: cntlzw(byte); extrwi ,1,26 (== (clz>>5)&1 == (byte==0));
        // xori ,1  -> stored value is (byte != 0) ? 1 : 0.
        lpFrame->maTrafficLightManager[4800] = (luByte != 0) ? 1u : 0u; // ((cntlzw(byte)>>5)&1) ^ 1
        BaseSerialiser::Read(&lpFrame->maTrafficLightManager[4804], 4);
    }

    // @0x8265D288
    // Record one whole frame buffer (the write mirror of ReadFrame).
    void TrafficEntitySerialiser::WriteFrame(TrafficEntitySerialiserStaticLayout* lpFrame, s32 liArg3)
    {
        CGS_ASSERT(lpFrame != nullptr, "lpFrame");
        // The X360 fires FOUR alignment asserts under the single (ptr & 0xF) != 0 guard
        // (0x8265D2EC-0x8265D368), each citing a distinct member.
        CGS_ASSERT((reinterpret_cast<uintptr_t>(&lpFrame->mHeader) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->mHeader) & 0xf) == 0");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(&lpFrame->mHeader) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->mHeader) & 0xf) == 0");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpFrame->maPhysicsInfo) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->maPhysicsInfo) & 0xf) == 0");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpFrame->maTrafficLightManager) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->mTrafficLightManager) & 0xf) == 0");

        BaseSerialiser::Write(&lpFrame->mHeader, 148);
        BaseSerialiser::Write(&lpFrame->muLocalPlayerHull, 2);
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
        const u8* lpSlot = (lpFrame->maTrafficLightManager + 5);
        for (u32 luSlot = 0; luSlot < KU_NUM_SLOTS; ++luSlot)
        {
            BaseSerialiser::Write(lpSlot, 1);
            lpSlot += KU_SLOT_STRIDE;
        }
        BaseSerialiser::Write(&lpFrame->maTrafficLightManager[4800], 1);  // +148080
        BaseSerialiser::Write(&lpFrame->maTrafficLightManager[4804], 4);   // +148084
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


namespace BrnReplays
{
namespace
{
    // FLAG recovered role name. ARTIST static descriptor 82FFA5C0, initializer
    // 82C5F3F8..82C5F47C: r4/r5/r6/r7=9/9/9/5, v1=(-2,-2,-2,0), v2=(2,2,2,1).
    const QuantisedVector4Compression sTrafficSkinningCompression(
        Vector4{-2.0f,-2.0f,-2.0f,0.0f}, Vector4{2.0f,2.0f,2.0f,1.0f}, 9,9,9,5);

    bool SameTrafficTransformXYZ(const Matrix44Affine& a, const Matrix44Affine& b)
    {
        const Vector3* ap = &a.xAxis; const Vector3* bp = &b.xAxis;
        for (u32 row=0;row<4;++row)
            if (ap[row].x != bp[row].x || ap[row].y != bp[row].y || ap[row].z != bp[row].z)
                return false;
        return true;
    }
    template<class T> bool TrafficXYZDeltaExceeds(const T& a, const T& b)
    {
        // flt_8209D7B0. Strict vcmpgt: NaN is false, unlike an inverted <= test.
        const float epsilon=0.0000152587890625f;
        return std::fabs(a.x-b.x)>epsilon || std::fabs(a.y-b.y)>epsilon || std::fabs(a.z-b.z)>epsilon;
    }
}

    // ARTIST 8265AE98..8265B70C. A four-bit mask selects detached parts,
    // skinning, light locators and the wheel/deformation block respectively.
    void TrafficEntitySerialiser::WritePhysicsInfo(const u8* lpInfo, u8 luIndex)
    {
        const auto& info=*reinterpret_cast<const BrnTraffic::TrafficPhysicsInfo*>(lpInfo);
        CGS_ASSERT(info.mDetachedPartQueue.muNumParts<=20,"lPhysicsInfo.muNumDetachedParts <= 20");
        auto* layout=GetStaticLayout();
        const BrnTraffic::TrafficPhysicsInfo* previous=nullptr;
        if (!IsKeyFrame())
        {
            CGS_ASSERT(luIndex<25,"invalid index");
            if ((layout->mInfoValid & (u64(1)<<luIndex))!=0)
                previous=reinterpret_cast<const BrnTraffic::TrafficPhysicsInfo*>(layout->maPreviousPhysicsInfo)+luIndex;
        }
        u8 flags=15;
        if (previous)
        {
            CGS_ASSERT(!IsKeyFrame(),"!IsKeyFrame()");
            u32 part=0;
            if(info.mDetachedPartQueue.muNumParts==previous->mDetachedPartQueue.muNumParts)
            {
                for(;part<info.mDetachedPartQueue.muNumParts;++part)
                    if(info.mDetachedPartQueue.mau8PartIndex[part]!=previous->mDetachedPartQueue.mau8PartIndex[part] ||
                       !SameTrafficTransformXYZ(info.mDetachedPartQueue.maTransforms[part],previous->mDetachedPartQueue.maTransforms[part])) break;
                if(part==info.mDetachedPartQueue.muNumParts) flags&=~1u;
            }
            if(std::memcmp(info.maSkinningOffsets_Scratch,previous->maSkinningOffsets_Scratch,sizeof(info.maSkinningOffsets_Scratch))==0) flags&=~2u;
            if(info.miNumLightLocators==previous->miNumLightLocators)
            {
                s32 light=0;
                // These two counterintuitive branches are in the shipped code:
                // 8265B1EC beq exits on EQUAL tags; 8265B22C bne exits when
                // every XYZ delta is <= epsilon. Do not replace with equality.
                for(;light<info.miNumLightLocators;++light)
                    if(info.maLightTagPointTypes[light]==previous->maLightTagPointTypes[light] ||
                       !TrafficXYZDeltaExceeds(info.maLightLocatorPositions[light],previous->maLightLocatorPositions[light])) break;
                if(light==info.miNumLightLocators) flags&=~4u;
            }
            bool sameWheels=std::memcmp(info.mabWheelExists,previous->mabWheelExists,sizeof(info.mabWheelExists))==0;
            bool sameRoad=!TrafficXYZDeltaExceeds(info.mvRoadTestNormal_HeightAboveRoad,previous->mvRoadTestNormal_HeightAboveRoad) &&
                !(std::fabs(info.mvRoadTestNormal_HeightAboveRoad.w-previous->mvRoadTestNormal_HeightAboveRoad.w)>0.0000152587890625f);
            if(sameWheels && sameRoad && info.mbIsDeforming==previous->mbIsDeforming &&
               info.mbIsFatallyCrashing==previous->mbIsFatallyCrashing && info.mu8RenderDamageFlags==previous->mu8RenderDamageFlags)
            {
                u32 glass=0;
                for(;glass<8;++glass)
                    if(info.mafGlassPaneFractureAmounts[glass]!=previous->mafGlassPaneFractureAmounts[glass]) break;
                // Original 8265B39C branches away when ALL glass entries match.
                // The following wheel loop subtracts each loaded row from ITSELF
                // (8265B3B8/C4/CC/D0), so its strict comparisons never exceed epsilon.
                if(glass!=8) flags&=~8u;
            }
        }
        BaseSerialiser::WriteByte(&flags);
        if(flags&1)
        {
            BaseSerialiser::Write(&info.mDetachedPartQueue.muNumParts,1);
            for(u32 part=0;part<info.mDetachedPartQueue.muNumParts;++part)
            {
                BaseSerialiser::Write(&info.mDetachedPartQueue.mau8PartIndex[part],1);
                WriteAsQuatPos(&info.mDetachedPartQueue.maTransforms[part]);
            }
        }
        if(flags&2)
            for(u32 point=0;point<128;++point)
            {
                alignas(16) u8 packed[16]; const auto& v=info.maSkinningOffsets_Scratch[point];
                QuantisedVector4::Pack(packed,&sTrafficSkinningCompression,Vector4{v.x,v.y,v.z,v.w});
                BaseSerialiser::Write(packed,4);
            }
        if(flags&4)
        {
            BaseSerialiser::Write(&info.miNumLightLocators,1);
            for(s32 light=0;light<info.miNumLightLocators;++light)
            {
                BaseSerialiser::WriteFloat(&info.maLightLocatorPositions[light].x);
                BaseSerialiser::WriteFloat(&info.maLightLocatorPositions[light].y);
                BaseSerialiser::WriteFloat(&info.maLightLocatorPositions[light].z);
                BaseSerialiser::Write(&info.maLightTagPointTypes[light],1);
            }
        }
        if(flags&8)
        {
            BaseSerialiser::Write(info.mabWheelExists,4);
            BaseSerialiser::Write(&info.mvRoadTestNormal_HeightAboveRoad,16);
            for(u32 wheel=0;wheel<4;++wheel)
            {
                float packed[12]; const Vector3* rows=&info.maWheelTransforms[wheel].xAxis;
                for(u32 row=0;row<4;++row) {packed[3*row]=rows[row].x;packed[3*row+1]=rows[row].y;packed[3*row+2]=rows[row].z;}
                BaseSerialiser::Write(packed,48);
            }
            BaseSerialiser::Write(&info.mbIsDeforming,1);
            BaseSerialiser::Write(&info.mbIsFatallyCrashing,1);
            BaseSerialiser::Write(&info.mu8RenderDamageFlags,1);
            BaseSerialiser::Write(info.mafGlassPaneFractureAmounts,32);
        }
    }

    // ARTIST 8265B710..8265BDC8; exported callers retained the name but no
    // function JSON was emitted. Recovered directly from the raw image assembly.
    void TrafficEntitySerialiser::ReadPhysicsInfo(TrafficPhysicsInfoRecord* lpInfo,u32 luIndex,s32)
    {
        auto& info=*reinterpret_cast<BrnTraffic::TrafficPhysicsInfo*>(lpInfo);
        auto* layout=GetStaticLayout();
        const BrnTraffic::TrafficPhysicsInfo* previous=nullptr;
        if(!IsKeyFrame())
        {
            CGS_ASSERT(luIndex<25,"invalid index");
            if(layout->mInfoValid & (u64(1)<<luIndex))
                previous=reinterpret_cast<const BrnTraffic::TrafficPhysicsInfo*>(layout->maPreviousPhysicsInfo)+luIndex;
        }
        u8 flags; BaseSerialiser::ReadByte(&flags);
        CGS_ASSERT(flags==15 || previous!=nullptr,"previous physics info required for delta");
        if(flags&1)
        {
            BaseSerialiser::ReadByte(&info.mDetachedPartQueue.muNumParts);
            for(u32 part=0;part<info.mDetachedPartQueue.muNumParts;++part)
            {
                BaseSerialiser::ReadByte(&info.mDetachedPartQueue.mau8PartIndex[part]);
                ReadAsQuatPos(&info.mDetachedPartQueue.maTransforms[part]);
            }
        }
        else
        {
            CGS_ASSERT(!IsKeyFrame(),"!IsKeyFrame()");
            info.mDetachedPartQueue.muNumParts=previous->mDetachedPartQueue.muNumParts;
            for(u32 part=0;part<info.mDetachedPartQueue.muNumParts;++part)
            {
                info.mDetachedPartQueue.mau8PartIndex[part]=previous->mDetachedPartQueue.mau8PartIndex[part];
                info.mDetachedPartQueue.maTransforms[part]=previous->mDetachedPartQueue.maTransforms[part];
            }
        }
        if(flags&2)
            for(u32 point=0;point<128;++point)
            {
                alignas(16) u8 packed[16];BaseSerialiser::Read(packed,4);
                QuantisedVector4::UnPack(&info.maSkinningOffsets_Scratch[point],packed,&sTrafficSkinningCompression);
            }
        else {CGS_ASSERT(!IsKeyFrame(),"!IsKeyFrame()");std::memcpy(info.maSkinningOffsets_Scratch,previous->maSkinningOffsets_Scratch,sizeof(info.maSkinningOffsets_Scratch));}
        if(flags&4)
        {
            BaseSerialiser::ReadByte(&info.miNumLightLocators);
            for(s32 light=0;light<info.miNumLightLocators;++light)
            {
                BaseSerialiser::ReadFloat(&info.maLightLocatorPositions[light].x);
                BaseSerialiser::ReadFloat(&info.maLightLocatorPositions[light].y);
                BaseSerialiser::ReadFloat(&info.maLightLocatorPositions[light].z);
                info.maLightLocatorPositions[light].w=0.0f;
                BaseSerialiser::ReadByte(&info.maLightTagPointTypes[light]);
            }
        }
        else
        {
            CGS_ASSERT(!IsKeyFrame(),"!IsKeyFrame()");info.miNumLightLocators=previous->miNumLightLocators;
            for(s32 light=0;light<info.miNumLightLocators;++light)
            {
                info.maLightLocatorPositions[light]=previous->maLightLocatorPositions[light];
                info.maLightTagPointTypes[light]=previous->maLightTagPointTypes[light];
            }
        }
        if(flags&8)
        {
            BaseSerialiser::Read(info.mabWheelExists,4);
            BaseSerialiser::Read(&info.mvRoadTestNormal_HeightAboveRoad,16);
            for(u32 wheel=0;wheel<4;++wheel)
            {
                float packed[12];BaseSerialiser::Read(packed,48);
                Vector3* rows=&info.maWheelTransforms[wheel].xAxis;
                for(u32 row=0;row<4;++row) rows[row]={packed[3*row],packed[3*row+1],packed[3*row+2],0.0f};
            }
            u8 value;BaseSerialiser::ReadByte(&value);info.mbIsDeforming=value!=0;
            BaseSerialiser::ReadByte(&value);info.mbIsFatallyCrashing=value!=0;
            BaseSerialiser::ReadByte(&info.mu8RenderDamageFlags);
            BaseSerialiser::Read(info.mafGlassPaneFractureAmounts,32);
        }
        else
        {
            CGS_ASSERT(!IsKeyFrame(),"!IsKeyFrame()");
            std::memcpy(info.mabWheelExists,previous->mabWheelExists,4);
            info.mvRoadTestNormal_HeightAboveRoad=previous->mvRoadTestNormal_HeightAboveRoad;
            std::memcpy(info.maWheelTransforms,previous->maWheelTransforms,sizeof(info.maWheelTransforms));
            info.mbIsDeforming=previous->mbIsDeforming;info.mbIsFatallyCrashing=previous->mbIsFatallyCrashing;
            info.mu8RenderDamageFlags=previous->mu8RenderDamageFlags;
            std::memcpy(info.mafGlassPaneFractureAmounts,previous->mafGlassPaneFractureAmounts,32);
        }
        CGS_ASSERT(info.mDetachedPartQueue.muNumParts<=20,"lPhysicsInfo.muNumDetachedParts <= 20");
    }
}
