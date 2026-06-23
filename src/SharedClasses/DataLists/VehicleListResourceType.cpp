#include "SharedClasses/DataLists/VehicleListResourceType.h"
#include "SharedClasses/DataLists/VehicleListEntry.h"   // BrnResource::VehicleListEntry (single home)
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnResource::VehicleListResourceType::FixDown                         @ 0x8267DF10
//   BrnResource::VehicleListResourceType::FixUp                           @ 0x8267DD60
//   BrnResource::VehicleListResourceType::GetTypeID                       @ 0x82675798
//   BrnResource::VehicleListResourceType::GetSerialisedResourceDescriptor @ 0x8267B540
//
// FixDown/FixUp rebase the entry-array pointer by the delta (the rw::Resource's load
// base); FixUp then destructs the embedded attrib/voice-over keys of each entry.

namespace BrnResource
{
    template <typename T>
    static T* PointerFromU32(u32 luAddress)
    {
        return reinterpret_cast<T*>(static_cast<uintptr_t>(luAddress));
    }

    // VehicleListEntry (incl. its embedded BaseCollisionGenerator keys) now lives in the
    // shared SharedClasses/DataLists/VehicleListEntry.h home (included above).

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

    // sizeof(VehicleListEntry) == 0xF0 (240): GetSerialisedResourceDescriptor multiplies the
    // vehicle count by 240 then adds the 16-byte header (muNumVehicles + mpEntries + 8-byte pad).
    static const u32 KU_VEHICLE_LIST_ENTRY_SIZE = 240;
    static const u32 KU_VEHICLE_LIST_HEADER_SIZE = 16;

    uint32_t VehicleListResourceType::GetTypeID() const
    {
        return KU_VEHICLE_LIST_RESOURCE_TYPE_ID;
    }

    // X360 0x8267B540 (store-for-store). Builds the five-entry serialised descriptor for a
    // VehicleListResource. The total serialised size is the 16-byte header plus 240 bytes per
    // vehicle entry (*lpResource is muNumVehicles, read at offset 0):
    //   entry0    = { size = 16 + 240 * numVehicles, align = 16 }
    //   entry1..4 = { size = 0, align = 1 }
    // The X360 first writes all five alignments (1) and four trailing sizes (0), then a single
    // 64-bit store of {size = 16 + 240*n, align = 16} overwrites entry0 -- so entry0 carries the
    // real size/alignment and the remaining four stay {0,1}. (The Hex-Rays pseudocode's
    // "result[2] = v3+16 ..." is a misread of that final std overwriting only entry0; the asm
    // stores r10 = 0 to entries 1-4's sizes.)
    CgsResource::ResourceDescriptor
    VehicleListResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const VehicleListResource* lpVehicleList =
            static_cast<const VehicleListResource*>(lpResource);
        const u32 luSize =
            KU_VEHICLE_LIST_HEADER_SIZE + KU_VEHICLE_LIST_ENTRY_SIZE * lpVehicleList->muNumVehicles;

        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16u;
        for (u32 luEntry = 1; luEntry < 5; ++luEntry)
        {
            lDescriptor.m_baseResourceDescriptors[luEntry].m_size      = 0u;
            lDescriptor.m_baseResourceDescriptors[luEntry].m_alignment = 1u;
        }
        return lDescriptor;
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
