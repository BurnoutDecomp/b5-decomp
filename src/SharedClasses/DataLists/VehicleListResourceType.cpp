#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnResource::VehicleListResourceType::FixDown   @ 0x8267DF10
//   BrnResource::VehicleListResourceType::FixUp     @ 0x8267DD60
//   BrnResource::VehicleListResourceType::GetTypeID @ 0x82675798

namespace CgsSceneManager
{
namespace CgsCollision
{
    struct BaseCollisionGenerator
    {
        void Destruct();

        u8 maStorage[8];
    };
}
}

namespace BrnResource
{
    static const u32 KI_VEHICLE_LIST_RESOURCE_TYPE_ID = 65541;

    template <typename T>
    static T* PointerFromU32(u32 luAddress)
    {
        return reinterpret_cast<T*>(static_cast<uintptr_t>(luAddress));
    }

    struct VehicleListEntry
    {
        void FixUp();

        u8 maPad0[160];
        CgsSceneManager::CgsCollision::BaseCollisionGenerator mAttribCollectionKey;
        u8 maPad168[8];
        CgsSceneManager::CgsCollision::BaseCollisionGenerator mExhaustEntityKey;
        CgsSceneManager::CgsCollision::BaseCollisionGenerator mEngineEntityKey;
        u8 maPad192[16];
        CgsSceneManager::CgsCollision::BaseCollisionGenerator mWonCarVoiceOverKey;
        CgsSceneManager::CgsCollision::BaseCollisionGenerator mRivalReleasedVoiceOverKey;
        u8 maPad224[16];
    };

    struct VehicleListResource
    {
        VehicleListEntry* GetEntries() const;

        u32 muNumVehicles;
        u32 mpEntries;
        u64 mu16BytePad;
    };

    class VehicleListResourceType
    {
    public:
        VehicleListResource* FixDown(VehicleListResource* lpResource, const s32* lpiDelta) const;
        VehicleListResource* FixUp(void* lpResourceTypeData, VehicleListResource* lpResource, const s32* lpiDelta) const;
        u32 GetTypeID() const;
    };

    void VehicleListEntry::FixUp()
    {
        mExhaustEntityKey.Destruct();
        mEngineEntityKey.Destruct();
        mWonCarVoiceOverKey.Destruct();
        mRivalReleasedVoiceOverKey.Destruct();
        mAttribCollectionKey.Destruct();
    }

    VehicleListEntry* VehicleListResource::GetEntries() const
    {
        return PointerFromU32<VehicleListEntry>(mpEntries);
    }

    VehicleListResource* VehicleListResourceType::FixDown(VehicleListResource* lpResource, const s32* lpiDelta) const
    {
        lpResource->mpEntries -= static_cast<u32>(*lpiDelta);
        return lpResource;
    }

    VehicleListResource* VehicleListResourceType::FixUp(
        void*,
        VehicleListResource* lpResource,
        const s32* lpiDelta) const
    {
        lpResource->mpEntries += static_cast<u32>(*lpiDelta);
        VehicleListEntry* lpEntries = lpResource->GetEntries();

        for (u32 luVehicle = 0; luVehicle < lpResource->muNumVehicles; ++luVehicle)
        {
            lpEntries[luVehicle].FixUp();
        }

        return lpResource;
    }

    u32 VehicleListResourceType::GetTypeID() const
    {
        return KI_VEHICLE_LIST_RESOURCE_TYPE_ID;
    }
}
