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
#include "GameSource/Replays/Serialisers/BrnReplayTrafficEntitySerialiser.h"
#include "GameSource/Replays/BrnReplayQuantisedQuatPos.h"
#include "GameSource/Replays/Serialisers/BrnReplayTrafficEntitySerialiserFrame.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"
#include "vendor/renderware/physics/JointFrames.hpp" // QuaternionFromMatrix33

#include <cstring>

namespace BrnReplays
{
    // -------------------------------------------------------------------------

    void TrafficEntitySerialiser::Construct()
    {
        // X360: ori r7,0xD480 over `lis 3` -> 0x3D480 static; `lis 1`|0x4000 -> 0x14000
        // dynamic; id 7; mode 0; "TrafficEntity"; flag 1.
        BaseSerialiser::Construct(static_cast<ESerialiserId>(7), E_CONTEXT_NORMAL,
                                 0x14000, 0x3D480, "TrafficEntity", true);
        mbStaticLayoutCleared = false;
        mbDeltaActive         = false;
    }

    TrafficEntitySerialiserStaticLayout* TrafficEntitySerialiser::GetStaticLayout()
    {
        // asm: lwz 0x24(this) (miStaticBufferSize) compared `< 0x3D480`; then assert the
        // static buffer pointer is non-null; return it.
        CGS_ASSERT(GetStaticBufferSize() >= static_cast<s32>(KU_STATIC_BUFFER_SIZE),
                   "Static buffer size is too small\n");
        CGS_ASSERT(mpStaticBuffer != 0, "GetStaticBuffer()");
        return reinterpret_cast<TrafficEntitySerialiserStaticLayout*>(mpStaticBuffer);
    }

    // @0x82653418
    // On the first frame after (re)start, when the static layout has not yet been
    // cleared and the static buffer exists, zero the whole layout, reset the frame's
    // bit arrays / info-valid words and mark it cleared.
    void TrafficEntitySerialiser::CheckStaticLayoutCleared()
    {
        if (!mbStaticLayoutCleared && mpStaticBuffer != 0)
        {
            TrafficEntitySerialiserStaticLayout* lpLayout = GetStaticLayout();
            std::memset(lpLayout, 0, KU_STATIC_BUFFER_SIZE);

            lpLayout->Clear();
            lpLayout->mInfoValid = 0;

            mbStaticLayoutCleared = true;
        }
    }

    // @0x82714118
    // Return the recorded physics-info record for luPhysicsInfo, range-checking the
    // index against the FastBitArray bound and asserting the bit is set.
    BrnTraffic::TrafficPhysicsInfo* TrafficEntitySerialiser::GetPhysicsInfo(u32 luPhysicsInfo)
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
    void TrafficEntitySerialiser::SetActiveHullsForLocalPlayer(const TrafficSerialiserFrame::ActiveHullSet* lpActiveHulls)
    {
        CGS_ASSERT(lpActiveHulls != 0, "lpActiveHulls");
        std::memcpy(&GetStaticLayout()->mActiveHullsForLocalPlayer, lpActiveHulls, sizeof(*lpActiveHulls));
    }

    // @0x82707438
    // Store luNumBonusAssets streamed-asset ids into the static layout (count at +150,
    // ids at +151..).
    void TrafficEntitySerialiser::SetBonusAssets(const u8* lpauBonusAssets, u32 luNumBonusAssets)
    {
        CGS_ASSERT(lpauBonusAssets != 0, "lpauBonusAssets");
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
    void TrafficEntitySerialiser::SetPhysicsData(const CgsContainers::BitArray<KU_NUM_PHYSICS_INFO>* lpBits,
                                                  const BrnTraffic::TrafficPhysicsInfo* lpaPhysicsInfo)
    {
        CGS_ASSERT(lpBits != 0, "lpBits");
        CGS_ASSERT(lpaPhysicsInfo != 0, "lpaPhysicsInfo");

        TrafficEntitySerialiserStaticLayout* lpLayout = GetStaticLayout();
        // asm: ld r10,0(a2) ; std r10,0xF0(layout) -- copy all 8 bytes of *lpBits as-is.
        CGS_STATIC_ASSERT((sizeof(lpLayout->mPhysicsBits) == sizeof(*lpBits)),
                          "ARTIST traffic replay physics mask extent");
        std::memcpy(&lpLayout->mPhysicsBits, lpBits, sizeof(*lpBits));

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
            liPhysicalPartsIndex = static_cast<s8>(lpVehicle->GetPhysicalPartsIndex());
        }

        TrafficVehicleDataRecord* lpRecord = GetVehicleData(luVehicle);
        TrafficVehicleData::SetFromVehicle(lpRecord, lpVehicle, liPhysicalPartsIndex);
    }

    // @0x82707168
    // Copy the per-chunk transform blocks (12800 bytes each) into the three vehicle
    // chunks' transform regions.
    void TrafficEntitySerialiser::SetVehicleTransforms(const void* lpaTransforms)
    {
        CGS_ASSERT(lpaTransforms != 0, "lpaTransforms");

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
    // Read the packed quaternion and position, then reproduce the VMX matrix expansion.
    void TrafficEntitySerialiser::ReadAsQuatPos(void* lpTransform)
    {
        u8 lauPacked[12];
        BaseSerialiser::Read(lauPacked, 12);

        CGS_ALIGN(16) float lafUnpacked[8];
        QuantisedQuatPos::UnPack(lafUnpacked, lauPacked);

        // ARTIST 0x82658E34..B0: sqrt(2) scaling, fused diagonal and the
        // original three byte-permutation controls, including the row W lanes.
        static const CGS_ALIGN(16) f32 kSqrt2[4] =
            { 1.41421354f, 1.41421354f, 1.41421354f, 1.41421354f };
        static const CGS_ALIGN(16) u32 kX[4] = { 0x04050607, 0x10111213, 0x00010203, 0x00010203 };
        static const CGS_ALIGN(16) u32 kY[4] = { 0x00010203, 0x18191A1B, 0x00010203, 0x00010203 };
        static const CGS_ALIGN(16) u32 kZ[4] = { 0x08090A0B, 0x14151617, 0x00010203, 0x00010203 };
        const __vector4 s = __vmulfp(__lvx(lafUnpacked, 0), __lvx(kSqrt2, 0));
        const __vector4 half = __vcfsx(__vspltisw(1), 1);
        const __vector4 d = __vnmsubfp(s, s, half);
        const __vector4 p = __vmulfp(s, __vpermwi(s, 0x60));
        const __vector4 r = __vmulfp(__vspltw(s, 3), __vpermwi(s, 0x84));
        const __vector4 diagonal = __vaddfp(d, __vpermwi(d, 0x60));
        const __vector4 add = __vaddfp(p, r);
        const __vector4 sub = __vsubfp(p, r);
        const __vector4 x = __vrlimi(__vperm(diagonal, add, __lvx(kX, 0)), sub, 2, 0);
        const __vector4 y = __vrlimi(__vperm(sub, diagonal, __lvx(kY, 0)), add, 2, 3);
        const __vector4 z = __vrlimi(__vperm(add, sub, __lvx(kZ, 0)), diagonal, 2, 2);
        const __vector4 position = { lafUnpacked[4], lafUnpacked[5], lafUnpacked[6], 0.0f };
        Matrix44Affine* lpOut = static_cast<Matrix44Affine*>(lpTransform);
        __stvx(position, &lpOut->wAxis, 0);
        __stvx(x, &lpOut->xAxis, 0);
        __stvx(y, &lpOut->yAxis, 0);
        __stvx(z, &lpOut->zAxis, 0);
    }

    // @0x82658B50
    // Convert the basis to a quaternion and pack the position and orientation.
    void TrafficEntitySerialiser::WriteAsQuatPos(const void* lpTransform)
    {
        const Matrix44Affine& lrTransform = *static_cast<const Matrix44Affine*>(lpTransform);
        rw::math::vpu::Matrix33 lBasis;
        lBasis.xAxis = lrTransform.xAxis;
        lBasis.yAxis = lrTransform.yAxis;
        lBasis.zAxis = lrTransform.zAxis;
        const rw::math::vpu::Quaternion lQuat = rw::math::vpu::QuaternionFromMatrix33(lBasis, 0.0f);

        CGS_ALIGN(16) float lafScratch[8];
        lafScratch[0] = lQuat.x;
        lafScratch[1] = lQuat.y;
        lafScratch[2] = lQuat.z;
        lafScratch[3] = lQuat.w;
        lafScratch[4] = lrTransform.wAxis.mV.x;
        const f32 lfY = lrTransform.wAxis.mV.y;
        // 0x82658D58..68: fsel(y - (-900), y, -900), including unordered behavior.
        lafScratch[5] = static_cast<f32>(__fsel(lfY - (-900.0f), lfY, -900.0f));
        lafScratch[6] = lrTransform.wAxis.mV.z;

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
        CGS_ASSERT(lpFrame != 0, "lpFrame");
        // The X360 fires FOUR alignment asserts under the single (ptr & 0xF) != 0 guard
        // (0x8265DD9C-0x8265DE18), each citing a distinct member.
        CGS_ASSERT((reinterpret_cast<uintptr_t>(&lpFrame->mActiveHullsForLocalPlayer) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->mHeader) & 0xf) == 0");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(&lpFrame->mActiveHullsForLocalPlayer) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->mHeader) & 0xf) == 0");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpFrame->maPhysicsInfo) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->maPhysicsInfo) & 0xf) == 0");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(&lpFrame->mTrafficLightManager) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->mTrafficLightManager) & 0xf) == 0");

        BaseSerialiser::Read(&lpFrame->mActiveHullsForLocalPlayer, 148);
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

        // ARTIST 0x8265E780: only each native light's flags byte is serialized.
        BrnTraffic::TrafficLightManager& lrLights = lpFrame->mTrafficLightManager;
        for (u32 i = 0; i < BrnTraffic::KU_MAX_TRAFFIC_LIGHT_INSTANCES; ++i)
            BaseSerialiser::ReadByte(&lrLights.maLightStates[i].mxStateBits);
        u8 luCountdown = 0;
        BaseSerialiser::ReadByte(&luCountdown);
        lrLights.mbCountdownLights = luCountdown != 0;
        BaseSerialiser::Read(&lrLights.meCountdownState, 4);
    }

    // @0x8265D288
    // Record one whole frame buffer (the write mirror of ReadFrame).
    void TrafficEntitySerialiser::WriteFrame(TrafficEntitySerialiserStaticLayout* lpFrame, s32 liArg3)
    {
        CGS_ASSERT(lpFrame != 0, "lpFrame");
        // The X360 fires FOUR alignment asserts under the single (ptr & 0xF) != 0 guard
        // (0x8265D2EC-0x8265D368), each citing a distinct member.
        CGS_ASSERT((reinterpret_cast<uintptr_t>(&lpFrame->mActiveHullsForLocalPlayer) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->mHeader) & 0xf) == 0");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(&lpFrame->mActiveHullsForLocalPlayer) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->mHeader) & 0xf) == 0");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpFrame->maPhysicsInfo) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->maPhysicsInfo) & 0xf) == 0");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(&lpFrame->mTrafficLightManager) & 0xf) == 0,
                   "(reinterpret_cast<uintptr_t>(&lpFrame->mTrafficLightManager) & 0xf) == 0");

        BaseSerialiser::Write(&lpFrame->mActiveHullsForLocalPlayer, 148);
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
            WritePhysicsInfo(&lpFrame->maPhysicsInfo[luInfo],
                             static_cast<u8>(luInfo));
        }

        // ARTIST 0x8265DCD4: flags bytes, countdown bool and state, in that order.
        const BrnTraffic::TrafficLightManager& lrLights = lpFrame->mTrafficLightManager;
        for (u32 i = 0; i < BrnTraffic::KU_MAX_TRAFFIC_LIGHT_INSTANCES; ++i)
            BaseSerialiser::Write(&lrLights.maLightStates[i].mxStateBits, 1);
        BaseSerialiser::Write(&lrLights.mbCountdownLights, 1);
        BaseSerialiser::Write(&lrLights.meCountdownState, 4);
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
        CGS_ASSERT(GetStaticLayout() != 0, "GetStaticLayout()");

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
        CGS_ASSERT(GetStaticLayout() != 0, "GetStaticLayout()");

        if (mpStaticBuffer != 0)
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
        CGS_ASSERT(lpVehicle != 0, "lpVehicle");
        CGS_ASSERT(lpVehicle->IsAlive(), "lpVehicle->IsAlive()");
        CGS_ASSERT(lpVehicle->IsAlive(), "IsAlive()");

        // record[0] = vehicle type byte (`lbz 0(r31); stb 0(r29)`), NOT the flags byte.
        lpRecord->muVehicleType = lpVehicle->GetVehicleType();

        // a3 != -1 && !lpVehicle->IsPhysical()  ->  assert (asm 0x82713EEC-0x82713EFC).
        if (liPhysicalPartsIndex != -1)
        {
            CGS_ASSERT(lpVehicle->IsPhysical(),
                       "liPhysicsIndex == -1 || lpVehicle->IsPhysical()");
        }

        // record[2] = physical-parts index; record[1] starts cleared.
        lpRecord->miPhysicsIndex = static_cast<s8>(liPhysicalPartsIndex);
        lpRecord->muFlags = 0;

        // Bit 0x01: headlights on when warmth >= 0.5 (flt_820BA62C; `fcmpu; blt`).
        if (lpVehicle->GetHeadlightWarmth() >= KF_HEADLIGHT_ON_THRESHOLD)
        {
            lpRecord->muFlags |= TrafficVehicleDataRecord::E_FLAG_HEADLIGHTS;
        }

        // Bit 0x02: brakelights (`clrlwi r11,r3,24; cmplwi; beq`).
        if (lpVehicle->AreBrakelightsOn())
        {
            lpRecord->muFlags |= TrafficVehicleDataRecord::E_FLAG_BRAKELIGHTS;
        }

        // The next three mxEffectState (+7) reads each re-assert IsAlive() first (the
        // inlined predicate guards at asm 0x6C1/0x6C8/0x6BA), in this exact order.
        const u8 luEffectState = lpVehicle->GetEffectState();

        CGS_ASSERT(lpVehicle->IsAlive(), "IsAlive()"); // asm 0x6C1
        if ((luEffectState & 0x02u) != 0)              // a2[7] & 0x02 (left-indicator bit)
        {
            lpRecord->muFlags |= TrafficVehicleDataRecord::E_FLAG_LEFT_INDICATOR;
        }

        CGS_ASSERT(lpVehicle->IsAlive(), "IsAlive()"); // asm 0x6C8
        if ((luEffectState & 0x04u) != 0)              // a2[7] & 0x04 (right-indicator bit)
        {
            lpRecord->muFlags |= TrafficVehicleDataRecord::E_FLAG_RIGHT_INDICATOR;
        }

        CGS_ASSERT(lpVehicle->IsAlive(), "IsAlive()"); // asm 0x6BA
        if ((luEffectState & 0x10u) != 0)              // a2[7] & 0x10 (alarm bit)
        {
            lpRecord->muFlags |= TrafficVehicleDataRecord::E_FLAG_ALARM;
        }

        // Bit 0x20: set when the vehicle is NOT physical (`(a2[5] & 8) == 0`).
        if ((lpVehicle->GetFlags() & BrnTraffic::Vehicle::E_FLAG_PHYSICAL) == 0)
        {
            lpRecord->muFlags |= TrafficVehicleDataRecord::E_FLAG_NOT_PHYSICAL;
        }
    }
}
