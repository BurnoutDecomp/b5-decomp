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
//   +0x00000 (0)       mHeader              148 bytes  (Read/Write a2,148)
//   +0x00094 (148)     mTrafficLightState   2 bytes    (Read/Write a2+148,2)
//   +0x00096 (150)     muNumBonusAssets     1 byte     (SetBonusAssets stores count here)
//   +0x00097 (151)     maBonusAssets[ . ]   2 bytes    (SetBonusAssets stores assets +151+i; Read/Write a2+151,2)
//   +0x000A0 (160)     mVehicleUpdateBits   FastBitArray<600> (80 bytes / 10 u64; Read/Write a2+160,80)
//   +0x000F0 (240)     mPhysicsBits         FastBitArray<25>  (8 bytes / 1 u64;  Read/Write a2+240,8)
//   +0x00100 (256)     maVehicleChunks[3]   3 vehicle chunks, stride 13408
//   +0x242C0 (148096)  mInfoValid (key/restore flags word region used by Read/Write/Check)
//   +0x242D0 (148112)  maPreviousPhysicsInfo  102800 bytes (25*4112) prev-frame physics copy
//
//   Within a vehicle chunk (stride 13408, base +256):
//     +0    maVehicleData[200]    3 bytes each   (GetVehicleData: +13408*chunk + 3*local + 256)
//     +608  maVehicleTransform[200] 64 bytes each (GetVehicleTransform: +13408*chunk + (local<<6) + 864)
//
//   Physics-info array (base +40480, 25 records of 4112 bytes):
//     GetPhysicsInfo: 4112*index + base + 40480.

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"

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

    // KU_MAX_BONUS_STREAMED_ASSETS -- SetBonusAssets asserts the count <= 2.
    static const u32 KU_MAX_BONUS_STREAMED_ASSETS = 2;

    // Max detached parts WritePhysicsInfo asserts (muNumDetachedParts <= 20).
    static const u32 KU_MAX_DETACHED_PARTS = 20;

    // Static-base byte offsets of the named regions (see REGION MAP above).
    static const u32 KU_STATIC_BUFFER_SIZE       = 0x3D480; // 251008
    static const u32 KU_OFFSET_VEHICLE_DATA      = 256;     // +0x100
    static const u32 KU_OFFSET_VEHICLE_TRANSFORM = 864;     // +0x360
    static const u32 KU_OFFSET_PHYSICS_INFO      = 40480;   // +0x9E20
    static const u32 KU_OFFSET_SLOT_TABLE        = 143285;  // +0x22FB5  (== phys-info end +5)
    static const u32 KU_OFFSET_INFO_VALID        = 148096;  // +0x242C0
    static const u32 KU_OFFSET_PREV_PHYSICS_INFO = 148112;  // +0x242D0

    // The trailing slot table (ReadFrame loc_8265E780 / WriteFrame loc_8265DCD4):
    // KU_NUM_SLOTS one-byte entries at stride KU_SLOT_STRIDE, the first at +143285.
    static const u32 KU_NUM_SLOTS                = 600;     // 0x258
    static const u32 KU_SLOT_STRIDE              = 8;       // bytes between slot entries
    // The trailing slots-valid scalars (sub_8264FCF0-derived byte + a 4-byte word). BOTH the
    // read (ReadFrame 0x8265E7DC) and write (WriteFrame 0x8265DCD4: r29=a2+143280, +0x12C0/+0x12C4)
    // sides use these SAME offsets -- there is no read/write asymmetry.
    static const u32 KU_OFFSET_READ_SLOTS_VALID  = 148080;  // +0x242B0  (r29 + 0x12C0)
    static const u32 KU_OFFSET_READ_SLOTS_WORD   = 148084;  // +0x242B4  (r29 + 0x12C4)

    // One per-vehicle data record (3 bytes; GetVehicleData / SetVehicleData operate on it).
    // The internal field breakdown is not separately attested, so it is held as bytes.
    struct TrafficVehicleDataRecord
    {
        u8 maData[3];
    };

    // BrnReplays::TrafficVehicleData -- the (de)serialiser for one TrafficVehicleDataRecord.
    // SetVehicleData @0x82714060 calls SetFromVehicle(record, vehicle, physicalPartsIndex)
    // to populate the 3-byte record from a live BrnTraffic::Vehicle. No DWARF recovered;
    // declared here (its home), body in the (not-yet reconstructed) TrafficVehicleData TU.
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
        u8 maTransform[64];
    };

    // One recorded physics-info record (4112 bytes). Its internal layout is exercised by
    // ReadPhysicsInfo / WritePhysicsInfo via byte offsets that are not separately
    // attested as named fields; held as a sized opaque record so the 25-entry array and
    // the 102800-byte block copies land at the right size.
    struct TrafficPhysicsInfoRecord
    {
        u8 maInfo[4112];
    };

    // The 251008-byte static-layout buffer GetStaticLayout returns. Only the regions the
    // accessors prove are named; the spans between them are explicit padding.
    struct TrafficEntitySerialiserStaticLayout
    {
        u8                       maHeader[148];               // @0x00000  mHeader
        u8                       maTrafficLightState[2];      // @0x00094
        u8                       muNumBonusAssets;            // @0x00096
        u8                       maBonusAssets[2];            // @0x00097  (count at +150, assets +151..)
        u8                       maPad0099[160 - 153];        // @0x00099  -> +0xA0
        CgsContainers::FastBitArray<600> mVehicleUpdateBits;  // @0x000A0  80 bytes
        CgsContainers::FastBitArray<25>  mPhysicsBits;        // @0x000F0  8 bytes
        // @0x000F8 .. @0x00100 : 8 undecoded bytes before the vehicle chunks.
        u8                       maPad00F8[256 - 248];        // @0x000F8
        // 3 vehicle chunks, each KU_VEHICLE_CHUNK_STRIDE (13408) bytes.
        u8                       maVehicleChunks[KU_NUM_VEHICLE_CHUNKS * KU_VEHICLE_CHUNK_STRIDE]; // @0x00100
        // @0x100 + 3*13408 == 40480 == KU_OFFSET_PHYSICS_INFO (the chunks run right up to it).
        TrafficPhysicsInfoRecord maPhysicsInfo[KU_NUM_PHYSICS_INFO]; // @0x09E20  25 * 4112 == 102800
        // @0x9E20 + 102800 == 143280; the slot table begins +5 bytes later, at +143285.
        u8                       maPad22FB0[KU_OFFSET_SLOT_TABLE - (KU_OFFSET_PHYSICS_INFO + KU_PHYSICS_INFO_TOTAL)]; // @0x22FB0  5 bytes
        // The slot table: KU_NUM_SLOTS one-byte entries at stride KU_SLOT_STRIDE
        // (only byte 0 of each 8-byte slot is used; ReadFrame/WriteFrame walk these).
        // Held as a flat byte region so the strided access lands at maSlotTable[i*8];
        // the read-side trailing scalars below reuse this region's tail bytes.
        u8                       maSlotTable[KU_OFFSET_READ_SLOTS_VALID - KU_OFFSET_SLOT_TABLE]; // @0x22FB5
        u8                       muReadSlotsValid;            // @0x242B0  read-side (cntlzw>>5 ^1)
        u8                       maPad242B1[KU_OFFSET_READ_SLOTS_WORD - (KU_OFFSET_READ_SLOTS_VALID + 1)]; // 3 bytes
        u32                      muReadSlotsWord;             // @0x242B4  read-side BaseSerialiser::Read(_,4)
        u8                       maPad242B8[KU_OFFSET_INFO_VALID - (KU_OFFSET_READ_SLOTS_WORD + 4)]; // -> +0x242C0
        // The info-valid qword (@0x242C0). Read() snapshots the whole 8 bytes from mPhysicsBits;
        // Block-A and the Write path memset it to 0. It is NOT a write-side trailing-scalar source:
        // WriteFrame writes the SAME +148080/+148084 slots-valid scalars the read side uses (the
        // earlier "asymmetric +148096/+148100" reading was a mis-read of the WriteFrame asm).
        u64                      mInfoValid;          // @0x242C0  (8-byte snapshot word)
        // @0x242C8 .. @0x242D0 : 8 undecoded bytes before the previous-physics-info copy.
        u8                       maPad242C8[KU_OFFSET_PREV_PHYSICS_INFO - (KU_OFFSET_INFO_VALID + 8)];
        u8                       maPreviousPhysicsInfo[KU_PHYSICS_INFO_TOTAL]; // @0x242D0  102800 bytes
        // @0x242D0 + 102800 == 250912 .. 251008 : trailing undecoded bytes.
        u8                       maTail[KU_STATIC_BUFFER_SIZE - (KU_OFFSET_PREV_PHYSICS_INFO + KU_PHYSICS_INFO_TOTAL)];
    };
}
