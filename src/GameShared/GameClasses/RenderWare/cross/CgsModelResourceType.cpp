#include "GameShared/GameClasses/RenderWare/cross/CgsModelResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::ModelResourceType::FixDown                       @ 0x828A8548
//   CgsResource::ModelResourceType::GetImportCount                @ 0x828A7D48
//   CgsResource::ModelResourceType::GetImportPointer              @ 0x828A7D08
//   CgsResource::ModelResourceType::GetSerialisedResourceDescriptor @ 0x828A9418
//   CgsResource::ModelResourceType::GetTypeID                     @ 0x828A7A60
//
// FixDown rebases the three pointers at 0/4/8 of the model resource by the
// relocation delta (the rw::Resource's load base). GetImportCount/GetImportPointer
// read the import table (count at +16, pointer base at +0).
// GetSerialisedResourceDescriptor returns a five-entry descriptor whose first entry's
// size is the rounded sum of three sub-block sizes (from the counts at +16/+18) + 20.

namespace CgsResource
{
    namespace
    {
        // Rotate-left by 2, matching the X360 __ROL4__ idiom (== *4 for in-range counts).
        inline u32 RotL2(u32 luValue)
        {
            return (luValue << 2) | (luValue >> 30);
        }

        inline u32 Align4(u32 luValue)
        {
            return (luValue + 3) & ~static_cast<u32>(3);
        }
    }

    static const uint32_t KU_MODEL_RESOURCE_TYPE_ID = 42;

    uint32_t ModelResourceType::GetTypeID() const
    {
        return KU_MODEL_RESOURCE_TYPE_ID;
    }

    void ModelResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(lpResource);
        const u32 luDelta = CgsResource::GetLoadBase(lrResource);

        u32 luP1 = *reinterpret_cast<u32*>(lBase + 4) - luDelta;
        u32 luP2 = *reinterpret_cast<u32*>(lBase + 8) - luDelta;
        *reinterpret_cast<u32*>(lBase + 0) -= luDelta;
        *reinterpret_cast<u32*>(lBase + 4) = luP1;
        *reinterpret_cast<u32*>(lBase + 8) = luP2;
    }

    uint32_t ModelResourceType::GetImportCount(const void* lpResource) const
    {
        return *reinterpret_cast<const u16*>(reinterpret_cast<uintptr_t>(lpResource) + 16);
    }

    void ModelResourceType::GetImportPointer(const void* lpResource, uint32_t luIndex,
                                             uint32_t* lpuOffset, const void** lppValue) const
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(lpResource);
        u32 luCount = *reinterpret_cast<const u16*>(lBase + 16);

        if (luIndex >= luCount)
        {
            *lppValue = nullptr;
            *lpuOffset = 0;
        }
        else
        {
            uintptr_t lTable = *reinterpret_cast<const u32*>(lBase + 0);
            *lppValue = reinterpret_cast<const void*>(*reinterpret_cast<const u32*>(lTable + 4 * luIndex));
            *lpuOffset = 4 * (luIndex + 5);
        }
    }

    ResourceDescriptor ModelResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(lpResource);

        u32 luField18   = *reinterpret_cast<const u16*>(lRes + 18);
        u32 luField16x4 = RotL2(*reinterpret_cast<const u16*>(lRes + 16));

        // First entry: total rounded size (alignment 4); the remaining four empty.
        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = Align4(luField16x4)
                                                + Align4(RotL2(luField18))
                                                + Align4(luField18)
                                                + 20;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4;
        for (int li = 1; li < 5; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size      = 0;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1;
        }
        return lDescriptor;
    }
}
