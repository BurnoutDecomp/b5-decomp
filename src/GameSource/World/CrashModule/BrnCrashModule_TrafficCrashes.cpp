#include "GameSource/World/CrashModule/BrnCrashModule.h"
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdlib>

namespace BrnWorld
{
    // ARTIST827C6BB0, DWARF const search. First matching record wins.
    u32 CrashModule::FindCrashForTrafficVehicle(u32 luVehicleIndex) const
    {
        for (u32 i = 0; i < mTrafficCrashes.GetLength(); ++i)
            if (mTrafficCrashes.GetItem(i).GetVehicleIndex() == luVehicleIndex)
                return i;
        return KU_INVALID_CRASH;
    }

    // ARTIST827CD098: r4 is a full VolumeInstanceId, r5 the crasher EntityId, r6 the type.
    void CrashModule::AddCrashingTrafficVehicle(CgsSceneManager::VolumeInstanceId lVehicle,
                                                EntityId lCrasher, BrnPhysics::Vehicle::eCrashTrafficType leType)
    {
        const u32 vehicle = lVehicle.GetEntityIDEntityIndex();
        CGS_ASSERT(static_cast<u32>(lVehicle.muId >> 32) != lCrasher.muValue,
                   "Traffic vehicle was crashed by itself");
        CGS_ASSERT(vehicle < 600, "Index is out of range (max bits: 600)");
        CGS_ASSERT(!mCrashingTraffic.IsBitSet(vehicle) || mCrashingNetworkTraffic.IsBitSet(vehicle),
                   "!mCrashingTraffic.IsBitSet( luVehicleIndex ) || mCrashingNetworkTraffic.IsBitSet( luVehicleIndex )");
        s32 owner = -1;
        if (maiSlammedTrafficOwners[vehicle] != -1)
        {
            owner = maiSlammedTrafficOwners[vehicle];
            CGS_ASSERT(static_cast<u32>(owner) < 8, "Got bonkers crash owner for traffic changing from slammed to crashed");
            CGS_ASSERT(static_cast<u32>(owner) < 0x4000, "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");
            lCrasher.muValue = 0x01000000u | (static_cast<u32>(owner) << 10);
            maiSlammedTrafficOwners[vehicle] = -1;
        }
        u32 type = lCrasher.muValue >> 24;
        CGS_ASSERT(type == 0 || type == 1 || type == 2, "Traffic vehicle crashed by something weird: entity id");
        if (mCrashingNetworkTraffic.IsBitSet(vehicle))
            return;
        bool trafficCrasher = type == 2;
        if (trafficCrasher)
        {
            const u32 other = (lCrasher.muValue >> 10) & 0x3fffu;
            if (maiSlammedTrafficOwners[other] != -1)
            {
                owner = maiSlammedTrafficOwners[other];
                CGS_ASSERT(static_cast<u32>(owner) < 8, "Got bonkers crash owner for traffic by slammed traffic");
                CGS_ASSERT(static_cast<u32>(owner) < 0x4000, "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");
                lCrasher.muValue = 0x01000000u | (static_cast<u32>(owner) << 10);
                trafficCrasher = false;
            }
        }
        if (trafficCrasher)
        {
            const u32 other = (lCrasher.muValue >> 10) & 0x3fffu;
            CGS_ASSERT(other < 600, "Index is out of range (max bits: 600)");
            if (!mCrashingTraffic.IsBitSet(other))
            {
                if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint)
                    *CgsDev::Log::gpDebugPrint << "TRAF_WARN: The traffic and crash modules seem to be out of sync - vehicle "
                        << vehicle << " was crashed by vehicle " << other << ", which isn't crashing or slammed\n";
                owner = meLocalActiveRaceCarIndex;
                CGS_ASSERT(static_cast<u32>(owner) < 8, "Got bonkers crash owner for traffic by out-of-sync player");
                CGS_ASSERT(static_cast<u32>(owner) < 0x4000, "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");
                lCrasher.muValue = 0x01000000u | (static_cast<u32>(owner) << 10);
            }
        }
        type = lCrasher.muValue >> 24;
        if (type == 0)
        {
            CGS_ASSERT(static_cast<u32>(meLocalActiveRaceCarIndex) < 0x4000, "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");
            lCrasher.muValue = 0x01000000u | (static_cast<u32>(meLocalActiveRaceCarIndex) << 10);
            type = 1;
        }
        if (type == 1)
        {
            owner = (lCrasher.muValue >> 10) & 0x3fff;
            CGS_ASSERT(static_cast<u32>(owner) < 8, "Got bonkers crash owner for traffic by racecar");
        }
        else if (type == 2)
        {
            const u32 index = FindCrashForTrafficVehicle((lCrasher.muValue >> 10) & 0x3fff);
            if (index != KU_INVALID_CRASH)
            {
                owner = mTrafficCrashes.GetItem(index).GetOwner();
                CGS_ASSERT(static_cast<u32>(owner) < 8, "Got bonkers crash owner for traffic by traffic");
            }
        }
        CGS_ASSERT(owner >= 0, "Failed to find owner for crashing traffic vehicle");
        f32 time = -1.0f;
        if (leType == 0) time = 3.5f;
        else if (static_cast<u32>(leType) <= 2) time = 8.0f;
        else CGS_ASSERT(false, "Bad crash traffic type");
        CGS_ASSERT(static_cast<u32>(owner) < 8, "Ended up without a crash owner for traffic");
        // 827CDAFC..827CDB1C: remote ownership in an online mode starts unconfirmed.
        const bool network = mbIsOnlineGameMode && owner != meLocalActiveRaceCarIndex;
        mTrafficCrashes.Grow()->Construct(owner, static_cast<u16>(vehicle), time, network);
        mCrashingTraffic.SetBit(vehicle);
        maCrashingTrafficForPlayers[owner].Insert(static_cast<u16>(vehicle));
        if (std::getenv("BRN_CRASH_ACTION_DIAG") && CgsDev::Log::gpDebugPrint)
            *CgsDev::Log::gpDebugPrint << "[traffic-crash] added vehicle=" << vehicle << " owner=" << owner << " timer=" << time << "\n";
    }

    // ARTIST827D0F80.
    void CrashModule::HandleNewCrashingTraffic(const CrashIO::InputBuffer_PostPhysics* lpInput)
    {
        CGS_ASSERT(lpInput, "lpInput != NULL");
        const auto& events = lpInput->GetTrafficInputInterface()->GetAddCrashingTrafficEventQueue();
        for (s32 i = 0; i < events.GetLength(); ++i)
        {
            const auto& event = events.GetEvent(i);
            AddCrashingTrafficVehicle(event.mVolumeInstanceId, event.mCrasherEntityId, event.meCrashTrafficType);
        }
    }

    // ARTIST827CB080: ownership follows a direct race car, a slammed car, or a crash chain.
    void CrashModule::ProcessSlammedTrafficEvents(const CrashIO::InputBuffer_PostPhysics* lpInput)
    {
        CGS_ASSERT(lpInput, "lpInput != NULL");
        const auto* output = lpInput->GetVehicleManagerOutputInterface();
        CGS_ASSERT(output, "lpVehicleManagerOutputInterface != NULL");
        const auto* events = output->GetSlammedTrafficEventQueue();
        CGS_ASSERT(events, "lpSlammedTrafficEvents");
        for (s32 i = 0; i < events->GetLength(); ++i)
        {
            const auto& event = events->GetEvent(i);
            CGS_ASSERT((event.mTrafficId.muValue >> 24) == 2, "Slammed traffic event not referring to traffic");
            const u32 vehicle = (event.mTrafficId.muValue >> 10) & 0x3fff;
            CGS_ASSERT(vehicle < 600, "Slammed traffic event referring to invalid traffic vehicle");
            if (mCrashingTraffic.IsBitSet(vehicle)) continue;
            s32 owner = meLocalActiveRaceCarIndex;
            const u32 type = event.mEntityThatSlammedIt.muValue >> 24;
            const u32 other = (event.mEntityThatSlammedIt.muValue >> 10) & 0x3fff;
            if (type == 2)
            {
                if (maiSlammedTrafficOwners[other] != -1)
                {
                    owner = maiSlammedTrafficOwners[other];
                    CGS_ASSERT(static_cast<u32>(owner) < 8, "Got bonkers slam owner by slammed traffic");
                }
                else
                {
                    CGS_ASSERT(other < 600, "Index is out of range (max bits: 600)");
                    if (mCrashingTraffic.IsBitSet(other))
                    {
                        const u32 index = FindCrashForTrafficVehicle(other);
                        if (index != KU_INVALID_CRASH)
                        {
                            owner = mTrafficCrashes.GetItem(index).GetOwner();
                            CGS_ASSERT(static_cast<u32>(owner) < 8, "Got bonkers slam owner by traffic");
                        }
                    }
                }
            }
            else
            {
                owner = other;
                CGS_ASSERT(type == 1, "Traffic vehicle got slammed by weird entity type");
            }
            maiSlammedTrafficOwners[vehicle] = static_cast<s8>(owner);
        }
    }

    // ARTIST827BF3F0.
    void CrashModule::HandleRecoveredSlammedTraffic(const CrashIO::InputBuffer_PostPhysics* lpInput)
    {
        CGS_ASSERT(lpInput, "lpInput != NULL");
        const auto* events = lpInput->GetTrafficInputInterface()->GetRemoveSlammedTrafficEventQueue();
        for (s32 i = 0; i < events->GetLength(); ++i)
        {
            const u32 vehicle = events->GetEvent(i).muVehicleId;
            CGS_ASSERT(vehicle < 600, "luVehicle < BrnTraffic::KU_MAX_TOTAL_TRAFFIC");
            maiSlammedTrafficOwners[vehicle] = -1;
        }
    }

    // ARTIST827CCB58: explicit removals, then stale records whose physics body has gone.
    void CrashModule::HandleCleanedUpTrafficEvents(const CrashIO::InputBuffer_PostPhysics* lpInput)
    {
        CGS_ASSERT(lpInput, "lpInput != NULL");
        const auto* traffic = lpInput->GetTrafficInputInterface();
        const auto* events = traffic->GetRemoveCrashedTrafficEventQueue();
        for (s32 i = 0; i < events->GetLength(); ++i)
        {
            const u32 vehicle = events->GetEvent(i).muVehicleId;
            CGS_ASSERT(vehicle < 600, "luVehicle < BrnTraffic::KU_MAX_TOTAL_TRAFFIC");
            const u32 index = FindCrashForTrafficVehicle(vehicle);
            EActiveRaceCarIndex owner = E_ACTIVE_RACE_CAR_INDEX_INVALID;
            if (index != KU_INVALID_CRASH)
            {
                owner = mTrafficCrashes.GetItem(index).GetOwner();
                mTrafficCrashes.EraseFast(index);
            }
            OnTrafficCarRemovedFromCrash(vehicle, owner);
            // FLAG PC witness: opt-in observation of the original removal path.
            if (std::getenv("BRN_CRASH_ACTION_DIAG") && CgsDev::Log::gpDebugPrint)
                *CgsDev::Log::gpDebugPrint << "[traffic-crash] removed vehicle=" << vehicle << " owner=" << static_cast<s32>(owner) << " reason=explicit\n";
        }
        const auto* physical = traffic->GetPhysicalBits();
        CGS_ASSERT(physical, "lpPhysicalBits != NULL");
        for (u32 i = 0; i < mTrafficCrashes.GetLength();)
        {
            const auto& crash = mTrafficCrashes.GetItem(i);
            const u32 vehicle = crash.GetVehicleIndex();
            CGS_ASSERT(vehicle < 600, "Index is out of range (max bits: 600)");
            if (!physical->IsBitSet(vehicle))
            {
                const auto owner = crash.GetOwner();
                mTrafficCrashes.EraseFast(i);
                OnTrafficCarRemovedFromCrash(vehicle, owner);
                // FLAG PC witness: opt-in observation of the original removal path.
                if (std::getenv("BRN_CRASH_ACTION_DIAG") && CgsDev::Log::gpDebugPrint)
                    *CgsDev::Log::gpDebugPrint << "[traffic-crash] removed vehicle=" << vehicle << " owner=" << static_cast<s32>(owner) << " reason=physical-or-recycled\n";
            }
            else ++i;
        }
    }

    // ARTIST827CE608 ignores the output argument (DWARF retains it).
    void CrashModule::ClearUpRecycledTraffic(CrashIO::OutputBuffer_PreScene* /*lpOutput*/)
    {
        for (s32 i = 0; i < mRecycledTrafficQueue.GetLength(); ++i)
        {
            const auto id = mRecycledTrafficQueue.GetEvent(i).mRemovedVehicleEntityId;
            CGS_ASSERT((id.muValue >> 24) == 2, "lRemovedVehicleEntityId.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");
            const u32 vehicle = (id.muValue >> 10) & 0x3fff;
            const u32 index = FindCrashForTrafficVehicle(vehicle);
            if (index != KU_INVALID_CRASH)
            {
                const auto owner = mTrafficCrashes.GetItem(index).GetOwner();
                mTrafficCrashes.EraseFast(index);
                OnTrafficCarRemovedFromCrash(vehicle, owner);
                // FLAG PC witness: opt-in observation of the original removal path.
                if (std::getenv("BRN_CRASH_ACTION_DIAG") && CgsDev::Log::gpDebugPrint)
                    *CgsDev::Log::gpDebugPrint << "[traffic-crash] removed vehicle=" << vehicle << " owner=" << static_cast<s32>(owner) << " reason=physical-or-recycled\n";
            }
        }
    }
}
