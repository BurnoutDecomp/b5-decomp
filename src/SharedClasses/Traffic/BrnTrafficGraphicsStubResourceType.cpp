#include "SharedClasses/Traffic/BrnTrafficGraphicsStubResourceType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"

#include <cstddef>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTraffic::GraphicsStubResourceType::GetTypeID                        @ 0x82752770
//   BrnTraffic::GraphicsStubResourceType::GetSerialisedResourceDescriptor  @ 0x82760708
//   BrnTraffic::GraphicsStubResourceType::GetImportPointer                 @ 0x82752780
// FixDown / FixUp / GetImportCount are ICF-folded on the console (empty bodies and a
// `return 2` fold into shared thunks, so they carry no distinct export); their shapes come
// from the DecFIGS DWARF (BrnTrafficGraphicsStubResourceType.h:38) and the leaked Feb-2007
// SharedClasses/Traffic/BrnTrafficGraphicsStubResourceType.cpp, which agree exactly:
// both Fix* are EMPTY and GetImportCount returns 2 -- which the shipped bundles corroborate
// (importCount 2 with offsets {0x0, 0x4} in 42/42 retail VEH_T*_GR.BIN).

namespace BrnResource
{
    // The one enumerator this handler needs out of BrnResource's resource-type id enum.
    // The leaked source spells it BrnResource::E_BRN_TRAFFIC_GFX_STUB_RESOURCE_TYPE at
    // GLOBAL scope -- it was previously declared here as BrnTraffic::BrnResource, a
    // namespace that does not exist in the game and that would have silently shadowed the
    // real ::BrnResource for any later code written inside namespace BrnTraffic.
    // PARK: the full BrnResource resource-type id enum has no reconstructed home in this
    // tree yet (every other handler carries a file-local id constant the same way).
    enum { E_BRN_TRAFFIC_GFX_STUB_RESOURCE_TYPE = 65557 };   // 0x10015
}

namespace BrnTraffic
{
    // GetTypeID @ 0x82752770 -- `return 65557;`
    uint32_t GraphicsStubResourceType::GetTypeID() const
    {
        return BrnResource::E_BRN_TRAFFIC_GFX_STUB_RESOURCE_TYPE;
    }

    // GetSerialisedResourceDescriptor @ 0x82760708 (store-for-store). A CONSTANT
    // descriptor: the X360 stores the 64-bit literal 0x800000004 at +0 -- the `std`
    // packs {size = 8, alignment = 4} -- then writes {0, 1} into entries 1..4.
    //
    // The size is the SERIALISED footprint: two 4-byte slots. Because GraphicsStub's
    // members are CgsGraphics::Ptr32<T>, host sizeof(GraphicsStub) is also 8 and the two
    // agree (pinned by GraphicsStub::_AssertLayout) -- but the literal is written out
    // here as the console's own constant, exactly as the asm has it.
    CgsResource::ResourceDescriptor GraphicsStubResourceType::GetSerialisedResourceDescriptor(const void*) const
    {
        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = 8u;   // entry0 size  (two 4-byte slots)
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4u;   // entry0 align
        for (uint32_t luBlock = 1; luBlock < 5u; ++luBlock)
        {
            lDescriptor.m_baseResourceDescriptors[luBlock].m_size      = 0u;   // entry1..4 {0,1}
            lDescriptor.m_baseResourceDescriptors[luBlock].m_alignment = 1u;
        }
        return lDescriptor;
    }

    // Empty on the console and in the leak: both of this resource's words are import
    // slots, which CgsResource::Pool::ResolveImportForEntry overwrites outright, so
    // there is nothing for the type to relocate or byte-swap.
    void GraphicsStubResourceType::FixDown(void*, const rw::Resource&) const
    {
    }

    void GraphicsStubResourceType::FixUp(void*, const rw::Resource&) const
    {
    }

    uint32_t GraphicsStubResourceType::GetImportCount(const void*) const
    {
        return 2;
    }

    // GetImportPointer @ 0x82752780. The console constant-folds the two offsets to the
    // literals 0 and 4 (`*a4 = 0` / `*a4 = 4`); the leaked source computes them as the
    // member's address minus the resource base, which is what is written here. The two
    // are the same number on this host ONLY because GraphicsStub's slots are Ptr32<T>
    // (4 bytes), and GraphicsStub::_AssertLayout is the assert that keeps it that way.
    void GraphicsStubResourceType::GetImportPointer(const void* lpResource, uint32_t luImportIndex, uint32_t* lpuOutOffset, const void** lppOutResource) const
    {
        const GraphicsStub* lpStub = static_cast<const GraphicsStub*>(lpResource);

        switch (luImportIndex)
        {
            case 0:
                *lppOutResource = lpStub->mpVehicleGraphics;
                *lpuOutOffset   = static_cast<uint32_t>(offsetof(GraphicsStub, mpVehicleGraphics));   // == 0
                break;

            case 1:
                *lppOutResource = lpStub->mpWheelGraphics;
                *lpuOutOffset   = static_cast<uint32_t>(offsetof(GraphicsStub, mpWheelGraphics));     // == 4
                break;

            default:
                // "Tried to access out-of-range import <i> on TrafficGraphicsStub resource"
                // (BrnTrafficGraphicsStubResourceType.cpp:175 in the console's baked path).
                CGS_ASSERT(false, "Tried to access out-of-range import on TrafficGraphicsStub resource");
                break;
        }
    }
}
