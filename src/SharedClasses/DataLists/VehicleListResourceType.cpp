#include "SharedClasses/DataLists/VehicleListResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnResource::VehicleListResourceType::FixDown   @ 0x8267DF10
//   BrnResource::VehicleListResourceType::FixUp     @ 0x8267DD60
//   BrnResource::VehicleListResourceType::GetTypeID @ 0x82675798
//
// FixDown/FixUp rebase the entry-array pointer by the delta (the rw::Resource's load
// base); FixUp then destructs the embedded attrib/voice-over keys of each entry.

namespace CgsSceneManager { namespace CgsCollision
{
    struct BaseCollisionGenerator
    {
        void Destruct();
        u8   maStorage[8];
    };
}}

namespace BrnResource
{
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
        VehicleListEntry* GetEntries() const { return PointerFromU32<VehicleListEntry>(mpEntries); }

        u32 muNumVehicles;
        u32 mpEntries;
        u64 mu16BytePad;
    };

    void VehicleListEntry::FixUp()
    {
        mExhaustEntityKey.Destruct();
        mEngineEntityKey.Destruct();
        mWonCarVoiceOverKey.Destruct();
        mRivalReleasedVoiceOverKey.Destruct();
        mAttribCollectionKey.Destruct();
    }

    static const uint32_t KU_VEHICLE_LIST_RESOURCE_TYPE_ID = 65541;

    uint32_t VehicleListResourceType::GetTypeID() const
    {
        return KU_VEHICLE_LIST_RESOURCE_TYPE_ID;
    }

    void VehicleListResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<VehicleListResource*>(lpResource)->mpEntries -= CgsResource::GetLoadBase(lrResource);
    }

    void VehicleListResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        VehicleListResource* lpVehicleList = static_cast<VehicleListResource*>(lpResource);
        lpVehicleList->mpEntries += CgsResource::GetLoadBase(lrResource);

        VehicleListEntry* lpEntries = lpVehicleList->GetEntries();
        for (u32 luVehicle = 0; luVehicle < lpVehicleList->muNumVehicles; ++luVehicle)
            lpEntries[luVehicle].FixUp();
    }
}
