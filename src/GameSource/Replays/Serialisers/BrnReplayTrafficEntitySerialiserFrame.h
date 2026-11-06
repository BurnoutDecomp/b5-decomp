#pragma once

// BrnReplays::TrafficEntitySerialiser static-layout frame types.
//
// DWARF/assert home path: GameSource/Replays/Serialisers/BrnReplayTrafficEntitySerialiser.h
// (the X360 accessor asserts cite this file at lines 422/423/457/476/489/507/520/...).
// No DWARF or Feb-2007 source was recovered for the frame structs, so every offset
// below is taken from the X360 ASM address arithmetic in the serialiser's accessor /
// frame functions. All access is BY NAME; undecoded gaps are explicit padding so the
// named regions land at their X360-attested byte offsets.
//
// SIZE: BaseSerialiser::Construct is called with a 0x3D480 (251008) static buffer
// (TrafficEntitySerialiser::Construct @0x8264C978 -> `ori r7,r7,0xD480`+`lis 3`), and
// GetStaticLayout @0x8264C710 asserts miStaticBufferSize >= 0x3D480. The buffer holds
// one recorded TrafficEntityFrame at +0 followed by a previous-physics-info snapshot
// region at +148112.
//
// REGION MAP (static-base offsets, all from the accessor ASM):
//   +0x00000 (0)       mActiveHullsForLocalPlayer 148 bytes  (Read/Write a2,148)
//   +0x00094 (148)     muLocalPlayerHull    2 bytes    (local-player hull marker; Read/Write a2+148,2)
//   +0x00096 (150)     muNumBonusAssets     1 byte     (SetBonusAssets stores count here)
//   +0x00097 (151)     maBonusAssets[ . ]   2 bytes    (SetBonusAssets stores assets +151+i; Read/Write a2+151,2)
//   +0x000A0 (160)     mVehicleUpdateBits   FastBitArray<600> (80 bytes / 10 u64; Read/Write a2+160,80)
//   +0x000F0 (240)     mPhysicsBits         FastBitArray<25>  (8 bytes / 1 u64;  Read/Write a2+240,8)
//   +0x00100 (256)     maVehicleChunks[3]   3 vehicle chunks, stride 13408
//   +0x24280 (148096)  mInfoValid (key/restore flags word region used by Read/Write/Check)
//   +0x24290 (148112)  maPreviousPhysicsInfo  102800 bytes (25*4112) prev-frame physics copy
//
//   Within a vehicle chunk (stride 13408, base +256):
//     +0    maVehicleData[200]    3 bytes each   (GetVehicleData: +13408*chunk + 3*local + 256)
//     +608  maVehicleTransform[200] 64 bytes each (GetVehicleTransform: +13408*chunk + (local<<6) + 864)
//
//   Physics-info array (base +40480, 25 records of 4112 bytes):
//     GetPhysicsInfo: 4112*index + base + 40480.

#include "types.hpp"
#include "GameSource/Replays/Serialisers/BrnReplayTrafficSerialiserFrame.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficPhysicsInfo.h"
#include <cstddef>

namespace BrnTraffic { class Vehicle; }

namespace BrnReplays
{
    class BaseSerialiser;

    // --- traffic-replay sizing constants (X360 asm authoritative) ---

    // KU_MAX_TOTAL_TRAFFIC -- the 0x258 (600) bound the per-vehicle accessors range-check
    // against ("luVehicle < BrnTraffic::KU_MAX_TOTAL_TRAFFIC").
    static const u32 KU_MAX_TOTAL_TRAFFIC = 600;

    // KU_NUM_VEHICLE_CHUNKS -- the 3 the chunk index is checked against
    // ("liCurrentChunk < (int32_t)KU_NUM_VEHICLE_CHUNKS").
    static const u32 KU_NUM_VEHICLE_CHUNKS = 3;

    // Vehicles per chunk: 600 / 3 == 200 (the `a2 / 0xC8`, `a2 % 0xC8` divisor).
    static const u32 KU_VEHICLES_PER_CHUNK = 200;

    // Byte stride between chunks (the 13408 multiplier).
    static const u32 KU_VEHICLE_CHUNK_STRIDE = 13408;

    // Number of recorded physics-info records (the 0x19 / 25 bound on the physics bits).
    static const u32 KU_NUM_PHYSICS_INFO = 25;

    // Byte size of one recorded physics-info record (the 4112 multiplier).
    static const u32 KU_PHYSICS_INFO_STRIDE = 4112;

    // Total recorded physics-info bytes copied by Set/Read/Write (102800 == 25 * 4112).
    static const u32 KU_PHYSICS_INFO_TOTAL = KU_NUM_PHYSICS_INFO * KU_PHYSICS_INFO_STRIDE;

    // Max detached parts WritePhysicsInfo asserts (muNumDetachedParts <= 20).
    static const u32 KU_MAX_DETACHED_PARTS = 20;

    // Static-base byte offsets of the named regions (see REGION MAP above).
    static const u32 KU_STATIC_BUFFER_SIZE       = 0x3D480; // 251008
    static const u32 KU_OFFSET_VEHICLE_DATA      = 256;     // +0x100
    static const u32 KU_OFFSET_VEHICLE_TRANSFORM = 864;     // +0x360
    static const u32 KU_OFFSET_PHYSICS_INFO      = 40480;   // +0x9E20
    static const u32 KU_OFFSET_INFO_VALID        = 148096;  // +0x24280
    static const u32 KU_OFFSET_PREV_PHYSICS_INFO = 148112;  // +0x24290

    // ARTIST 0x82713E38 writes these three bytes. RenderTrafficCar loads
    // the physics index signed, independently of the recorded physical flag.
    struct TrafficVehicleDataRecord
    {
        enum EFlag
        {
            E_FLAG_HEADLIGHTS = 0x01,
            E_FLAG_BRAKELIGHTS = 0x02,
            E_FLAG_LEFT_INDICATOR = 0x04,
            E_FLAG_RIGHT_INDICATOR = 0x08,
            E_FLAG_ALARM = 0x10,
            E_FLAG_NOT_PHYSICAL = 0x20,
        };

        u8 muVehicleType;
        u8 muFlags;
        s8 miPhysicsIndex;
    };

    CGS_STATIC_ASSERT((sizeof(TrafficVehicleDataRecord) == 3), "ARTIST traffic replay vehicle stride");

    // BrnReplays::TrafficVehicleData -- the (de)serialiser for one TrafficVehicleDataRecord.
    // SetVehicleData @0x82714060 calls SetFromVehicle(record, vehicle, physicalPartsIndex)
    // to populate the 3-byte record from a live BrnTraffic::Vehicle. No DWARF recovered;
    // defined in BrnReplayTrafficEntitySerialiser.cpp.
    namespace TrafficVehicleData
    {
        void SetFromVehicle(TrafficVehicleDataRecord* lpRecord,
                            const BrnTraffic::Vehicle* lpVehicle,
                            s32 liPhysicalPartsIndex);
    }

    // One per-vehicle transform record (64 bytes; SetVehicleTransforms memcpys it, the
    // record is read as a quat+pos at +608 within the chunk).
    struct TrafficVehicleTransformRecord
    {
        Matrix44Affine mTransform;
    };
    CGS_STATIC_ASSERT((sizeof(TrafficVehicleTransformRecord) == 64), "ARTIST replay transform extent");

    // The 251008-byte static-layout buffer GetStaticLayout returns. Only the regions the
    // accessors prove are named; the spans between them are explicit padding.
    struct TrafficEntitySerialiserStaticLayout : TrafficSerialiserFrame
    {
        // @0x000F8 .. @0x00100 : 8 undecoded bytes before the vehicle chunks.
        u8                       maPad00F8[256 - 248];        // @0x000F8
        // 3 vehicle chunks, each KU_VEHICLE_CHUNK_STRIDE (13408) bytes.
        u8                       maVehicleChunks[KU_NUM_VEHICLE_CHUNKS * KU_VEHICLE_CHUNK_STRIDE]; // @0x00100
        // @0x100 + 3*13408 == 40480 == KU_OFFSET_PHYSICS_INFO (the chunks run right up to it).
        BrnTraffic::TrafficPhysicsInfo maPhysicsInfo[KU_NUM_PHYSICS_INFO]; // @0x09E20  25 * 4112 == 102800
        // ARTIST UpdateSerialiser 0x8272E1F8..E240 copies the native manager here.
        BrnTraffic::TrafficLightManager mTrafficLightManager; // @0x22FB0, 0x12D0 bytes
        u64                      mInfoValid;          // @0x24280  (8-byte snapshot word)
        // @0x24288 .. @0x24290: 8 undecoded bytes before the previous-physics-info copy.
        u8                       maPad24288[KU_OFFSET_PREV_PHYSICS_INFO - (KU_OFFSET_INFO_VALID + 8)];
        BrnTraffic::TrafficPhysicsInfo maPreviousPhysicsInfo[KU_NUM_PHYSICS_INFO]; // @0x24290  102800 bytes
        // @0x24290 + 102800 == 250912 .. 251008 : trailing undecoded bytes.
        u8                       maTail[KU_STATIC_BUFFER_SIZE - (KU_OFFSET_PREV_PHYSICS_INFO + KU_PHYSICS_INFO_TOTAL)];
    };
#if defined(_XBOX)
    CGS_STATIC_ASSERT((sizeof(BrnTraffic::TrafficLightRuntimeState) == 8), "ARTIST traffic-light state stride");
    CGS_STATIC_ASSERT((offsetof(BrnTraffic::TrafficLightRuntimeState, mxStateBits) == 5), "ARTIST traffic-light flags offset");
    CGS_STATIC_ASSERT((sizeof(BrnTraffic::TrafficLightManager) == 0x12D0), "ARTIST traffic-light manager size");
    CGS_STATIC_ASSERT((offsetof(TrafficEntitySerialiserStaticLayout, maPhysicsInfo) == 0x9E20), "ARTIST recorded physics offset");
    CGS_STATIC_ASSERT((offsetof(TrafficEntitySerialiserStaticLayout, mTrafficLightManager) == 0x22FB0), "ARTIST traffic-light snapshot");
    CGS_STATIC_ASSERT((offsetof(TrafficEntitySerialiserStaticLayout, mInfoValid) == 0x24280), "ARTIST info-valid offset");
    CGS_STATIC_ASSERT((offsetof(TrafficEntitySerialiserStaticLayout, maPreviousPhysicsInfo) == 0x24290), "ARTIST previous-physics offset");
    CGS_STATIC_ASSERT((sizeof(TrafficEntitySerialiserStaticLayout) == 0x3D480), "ARTIST traffic replay static size");
#endif
}
