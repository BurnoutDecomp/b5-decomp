#pragma once

#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Replays/Serialisers/BrnReplayTrafficEntitySerialiserFrame.h"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"
namespace BrnTraffic { struct TrafficPhysicsInfo; }

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

        // @0x82653418 -- void; no getter/assertion on the unallocated or already-cleared paths.
        void CheckStaticLayoutCleared();

        // @0x8265F2D8 / @0x8265F4E8 -- playback / record one frame (top-level dispatch).
        s32 Read();
        s32 Write();

        // @0x8265DD38 / @0x8265D288 -- playback / record one whole frame buffer.
        void ReadFrame(TrafficEntitySerialiserStaticLayout* lpFrame, s32 liArg3);
        void WriteFrame(TrafficEntitySerialiserStaticLayout* lpFrame, s32 liArg3);

        // @0x82658D98 / @0x82658B50 -- read / write a quat+pos transform record.
        void ReadAsQuatPos(void* lpTransform);
        void WriteAsQuatPos(const void* lpTransform);

        // @0x8265B710 ReadPhysicsInfo (raw-image export hole) / @0x8265AE98 WritePhysicsInfo.
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
        void SetActiveHullsForLocalPlayer(const ::Set<u16, 72>* lpActiveHulls);
        // @0x82707438
        void SetBonusAssets(const u8* lpauBonusAssets, u32 luNumBonusAssets);
        // @0x82707320
        void SetPhysicsData(const CgsContainers::BitArray<25>* lpBits, const BrnTraffic::TrafficPhysicsInfo* lpaPhysicsInfo);
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

}
