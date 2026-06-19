#include "SharedClasses/World/BrnVehicleGraphicsSpecResourceType.h"

#include "types.hpp"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnVehicle::GraphicsSpecResourceType::GetTypeID @ 0x82676480
//   BrnVehicle::GraphicsSpecResourceType::FixDown   @ 0x8267E338
//   BrnVehicle::GraphicsSpecResourceType::FixUp     @ 0x8267E3E8
//
// FixUp/FixDown rebase the vehicle graphics-spec's pointer fields and its
// miCount-entry pointer array by the rw::Resource's load base (the relocation
// delta). FixUp validates the on-disk version first.

namespace BrnVehicle
{
    template <typename T>
    static T* PointerFromU32(u32 luAddress)
    {
        return reinterpret_cast<T*>(static_cast<uintptr_t>(luAddress));
    }

    // The vehicle graphics-spec resource layout is not committed; DWARF gives no
    // member names. Only the relocation slice is modelled here — the fields the
    // FixUp/FixDown rebase arithmetic touches, named u32 (load-relative pointers
    // stored as u32 offsets, rebased by += / -= delta like VehicleList::mpEntries).
    //
    // FLAG: every member name except muVersion/miCount is INFERRED from the
    //       rebase pattern. The true semantics of the +8/+16/+20/+24/+28/+32
    //       pointers (materials / meshes / parts tables) are deferred.
    // FLAG: muField3 (+12) is NOT rebased by either function — it is an inline
    //       value, not a load-relative pointer.
    struct GraphicsSpec
    {
        u32 muVersion;   // +0   on-disk version == 3
        s32 miCount;     // +4   number of entries in the mpArray (+32) table
        u32 mpField2;    // +8   rebased pointer (stored as u32 offset)
        u32 muField3;    // +12  NOT rebased (inline value) — FLAG
        u32 mpField4;    // +16  rebased
        u32 mpField5;    // +20  rebased
        u32 mpField6;    // +24  rebased
        u32 mpField7;    // +28  rebased
        u32 mpArray;     // +32  rebased; points to miCount rebased u32 entries
    };

    // FLAG: value 3 taken from the X360 `*a2 != 3` version check in FixUp.
    static const u32 KU_VEHICLE_GRAPHICS_SPEC_VERSION = 3;

    // Resource registry type id for the vehicle graphics-spec resource (0x10006).
    // Recovered verbatim from GetTypeID @ 0x82676480.
    static const uint32_t KU_VEHICLE_GRAPHICS_SPEC_RESOURCE_TYPE_ID = 65542;

    uint32_t GraphicsSpecResourceType::GetTypeID() const
    {
        return KU_VEHICLE_GRAPHICS_SPEC_RESOURCE_TYPE_ID;
    }

    // FixUp @ 0x8267E3E8. Validate version, then add the load-base delta to each
    // load-relative pointer field and to every entry of the +32 array.
    // The X360 `result` is the EndAssert() artifact (return value of the assert
    // path); the function is void by contract.
    void GraphicsSpecResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        GraphicsSpec* lpSpec = static_cast<GraphicsSpec*>(lpResource);

        CGS_ASSERT(lpSpec->muVersion == KU_VEHICLE_GRAPHICS_SPEC_VERSION,
                   "Incorrect version for Vehicle Graphics Spec; get latest code/tools and rebuild data \n");

        const u32 luDelta = CgsResource::GetLoadBase(lrResource);

        lpSpec->mpField2 += luDelta;   // a2[2] (+8)
        lpSpec->mpField4 += luDelta;   // a2[4] (+16)
        lpSpec->mpField5 += luDelta;   // a2[5] (+20)
        lpSpec->mpField7 += luDelta;   // a2[7] (+28)
        lpSpec->mpArray  += luDelta;   // a2[8] (+32)

        if (lpSpec->miCount)           // if (v13 = a2[1])
        {
            u32* lpArray = PointerFromU32<u32>(lpSpec->mpArray);
            for (u32 luEntry = 0; luEntry < static_cast<u32>(lpSpec->miCount); ++luEntry)
                lpArray[luEntry] += luDelta;
        }

        lpSpec->mpField6 += luDelta;   // a2[6] (+24)
    }

    // FixDown @ 0x8267E338. The inverse of FixUp: subtract the load-base delta
    // from the array entries (guarded as in the asm) then from each pointer field.
    // void by contract.
    void GraphicsSpecResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        GraphicsSpec* lpSpec = static_cast<GraphicsSpec*>(lpResource);

        const u32 luDelta = CgsResource::GetLoadBase(lrResource);

        lpSpec->mpField6 -= luDelta;   // *(result + 24) -= *a2

        if (lpSpec->miCount)           // if (v3 = *(result + 4))
        {
            // FLAG: the asm guards each array-entry write on *(result + 28)
            //       (mpField7), NOT on the array base *(result + 32). The array
            //       base it dereferences is mpArray (+32). Reproduced verbatim.
            u32* lpArray = PointerFromU32<u32>(lpSpec->mpArray);
            for (u32 luEntry = 0; luEntry < static_cast<u32>(lpSpec->miCount); ++luEntry)
            {
                if (lpSpec->mpField7)
                    lpArray[luEntry] -= luDelta;
            }
        }

        lpSpec->mpArray  -= luDelta;   // *(result + 32) -= *a2
        lpSpec->mpField7 -= luDelta;   // *(result + 28) -= *a2
        lpSpec->mpField5 -= luDelta;   // *(result + 20) -= *a2
        lpSpec->mpField2 -= luDelta;   // *(result + 8)  -= *a2
        lpSpec->mpField4 -= luDelta;   // *(result + 16) -= *a2
    }
}
