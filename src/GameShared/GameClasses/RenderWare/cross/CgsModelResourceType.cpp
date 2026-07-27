#include "GameShared/GameClasses/RenderWare/cross/CgsModelResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (FixUp version tripwire)

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

    // FixUp @ 0x828A8578 -- the serialise-IN counterpart of FixDown. Tripwires the model
    // data version (a BYTE at +19; the X360 streams "Model data version mismatch. Code
    // version = 2 Data version = <n>" at CgsModelResourceType.cpp:219), then rebases the
    // three u32 pointer slots at +0/+4/+8 by the load base -- reading +4 and +8 into
    // temporaries BEFORE storing +0, exactly like FixDown. Without this override the model
    // keeps its file-relative LOD-radius / renderable tables and every consumer
    // (InstanceListResourceType::PostFixUp first) walks from a raw file offset.
    void ModelResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(lpResource);
        const u32 luDelta = CgsResource::GetLoadBase(lrResource);

        CGS_ASSERT(*reinterpret_cast<const u8*>(lBase + 19) == 2,   // serialised blob
                   "Model data version mismatch. Code version = ");

        u32 luP1 = *reinterpret_cast<u32*>(lBase + 4) + luDelta;   // serialised blob
        u32 luP2 = *reinterpret_cast<u32*>(lBase + 8) + luDelta;   // serialised blob
        *reinterpret_cast<u32*>(lBase + 0) += luDelta;   // serialised blob
        *reinterpret_cast<u32*>(lBase + 4) = luP1;
        *reinterpret_cast<u32*>(lBase + 8) = luP2;
    }

    void ModelResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(lpResource);
        const u32 luDelta = CgsResource::GetLoadBase(lrResource);

        u32 luP1 = *reinterpret_cast<u32*>(lBase + 4) - luDelta;
        u32 luP2 = *reinterpret_cast<u32*>(lBase + 8) - luDelta;
        *reinterpret_cast<u32*>(lBase + 0) -= luDelta;
        *reinterpret_cast<u32*>(lBase + 4) = luP1;   // serialised blob
        *reinterpret_cast<u32*>(lBase + 8) = luP2;   // serialised blob
    }

    uint32_t ModelResourceType::GetImportCount(const void* lpResource) const
    {
        // FLAG: ARTIST 0x828A7D48 `lbz r3,0x10(r4)` (and DecFIGS 0xC44A00 `lbz`) -- the
        // import count at +16 is a BYTE field (u8), not u16. Was incorrectly read as u16.
        return *reinterpret_cast<const u8*>(reinterpret_cast<uintptr_t>(lpResource) + 16);
    }

    void ModelResourceType::GetImportPointer(const void* lpResource, uint32_t luIndex,
                                             uint32_t* lpuOffset, const void** lppValue) const
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(lpResource);
        // FLAG: ARTIST 0x828A7D08 `lbz r11,0x10(r4)` -- count at +16 is a BYTE (u8), not u16.
        u32 luCount = *reinterpret_cast<const u8*>(lBase + 16);

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

        // FLAG: ARTIST 0x828A9418 `lbz r9,0x12(r5)` / `lbz r8,0x10(r5)` (DecFIGS 0xC4705C
        // same) -- fields at +16 and +18 are BYTE fields (u8), not u16. Were read as u16.
        u32 luField18   = *reinterpret_cast<const u8*>(lRes + 18);
        u32 luField16x4 = RotL2(*reinterpret_cast<const u8*>(lRes + 16));

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
