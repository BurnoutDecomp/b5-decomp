#include "SharedClasses/Physics/Props/BrnPropGraphicsListResourceType.h"
#include "SharedClasses/Physics/Props/BrnPropGraphicsList.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"   // rw::BaseResourceDescriptors<5> complete for the descriptor fill
#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnPhysics::Props::PropGraphicsListResourceType::GetTypeID          @ 0x82675628
//   BrnPhysics::Props::PropGraphicsListResourceType::GetSerialisedResourceDescriptor @ 0x82846698
//   BrnPhysics::Props::PropGraphicsListResourceType::FixUp              @ 0x8267F780
//   BrnPhysics::Props::PropGraphicsListResourceType::FixDown            @ 0x8267F770
//   BrnPhysics::Props::PropGraphicsListResourceType::GetImportCount     @ 0x82677710
//   BrnPhysics::Props::PropGraphicsListResourceType::GetImportPointer   @ 0x82677720
//
// The serialised payload is a BrnPhysics::Props::PropGraphicsList whose first word is
// muSizeInBytes (the precomputed total byte size of the streamed resource; DWARF
// BrnPropGraphicsList.h:163). The fix-ups forward to the data type's own FixUp/FixDown; the
// import surface exposes one import per Model slot (whole-prop models first, then part models),
// exactly the X360 arithmetic at the x64-widened member offsets (the porter contract pinned in
// BrnPropGraphicsList.h).

namespace BrnPhysics
{
namespace Props
{
    static const uint32_t KU_PROP_GRAPHICS_LIST_RESOURCE_TYPE_ID = 65552;  // 0x10010

    // GetTypeID @ 0x82675628 -- `return 65552;`.
    uint32_t PropGraphicsListResourceType::GetTypeID() const
    {
        return KU_PROP_GRAPHICS_LIST_RESOURCE_TYPE_ID;
    }

    // FixUp @ 0x8267F780 -- forwards to PropGraphicsList::FixUp (0x8267DB38).
    void PropGraphicsListResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<PropGraphicsList*>(lpResource)->FixUp(lrResource);
    }

    // FixDown @ 0x8267F770 -- forwards to PropGraphicsList::FixDown (0x8267DCD0).
    void PropGraphicsListResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<PropGraphicsList*>(lpResource)->FixDown(lrResource);
    }

    // GetImportCount @ 0x82677710 -- one Model import per whole prop plus one per part:
    // `return *(a2 + 12) + *(a2 + 8)` (muNumberOfPropPartModels + muNumberOfPropModels).
    uint32_t PropGraphicsListResourceType::GetImportCount(const void* lpResource) const
    {
        const PropGraphicsList* lpList = static_cast<const PropGraphicsList*>(lpResource);
        return lpList->muNumberOfPropPartModels + lpList->muNumberOfPropModels;
    }

    // GetImportPointer @ 0x82677720. Import index space: [0, numModels) = the whole-prop
    // Model slots (PropGraphics::mpPropModel), [numModels, numModels+numParts) = the part
    // Model slots (PropPartGraphics::mpPropModel, resolved through GetPartGraphics exactly
    // as the X360). Reports the slot's current value and its byte offset within the blob.
    // The out-of-range assert is a non-gating tripwire (the X360 builds a StrStream
    // diagnostic with the index; execution continues into the range split).
    void PropGraphicsListResourceType::GetImportPointer(const void* lpResource, uint32_t luIndex,
                                                        uint32_t* lpuOffset, const void** lppValue) const
    {
        PropGraphicsList* lpList =
            const_cast<PropGraphicsList*>(static_cast<const PropGraphicsList*>(lpResource));

        CGS_ASSERT(luIndex < lpList->muNumberOfPropPartModels + lpList->muNumberOfPropModels,
                   "Tried to access out-of-range import on PropGraphicsList resource");

        if ( luIndex >= lpList->muNumberOfPropModels )
        {
            PropPartGraphics* lpPart = lpList->GetPartGraphics(luIndex - lpList->muNumberOfPropModels);
            *lppValue = lpPart->mpPropModel;
            *lpuOffset = static_cast<uint32_t>(
                reinterpret_cast<const u8*>(&lpPart->mpPropModel) - reinterpret_cast<const u8*>(lpList));
        }
        else
        {
            PropGraphics* lpElement = &lpList->mpaPropGraphics[luIndex];
            *lppValue = lpElement->mpPropModel;
            *lpuOffset = static_cast<uint32_t>(
                reinterpret_cast<const u8*>(&lpElement->mpPropModel) - reinterpret_cast<const u8*>(lpList));
        }
    }
    // GetSerialisedResourceDescriptor @ 0x82846698 (store-for-store). The X360:
    //   r9 = *a3 (PropGraphicsList::muSizeInBytes); entry0 = { m_size = muSizeInBytes, m_alignment = 16 }
    // It writes all five alignments (entry0 = 16 via the final 64-bit store; entry1..4 = 1) and the
    // four trailing sizes (0), then one 64-bit store of { muSizeInBytes, 16 } overwrites entry0. The
    // payload size word is read by its serialised dword offset (muSizeInBytes is the 1st dword).
    CgsResource::ResourceDescriptor
    PropGraphicsListResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const u32* lpList = static_cast<const u32*>(lpResource);
        const u32  luSize = lpList[0];   // PropGraphicsList::muSizeInBytes @ +0

        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;   // entry0 size
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16u;      // entry0 align
        for (u32 luBlock = 1; luBlock < 5u; ++luBlock)
        {
            lDescriptor.m_baseResourceDescriptors[luBlock].m_size      = 0u;   // entry1..4 {0,1}
            lDescriptor.m_baseResourceDescriptors[luBlock].m_alignment = 1u;
        }
        return lDescriptor;
    }
}
}
