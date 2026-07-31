#include "SharedClasses/World/BrnVehicleGraphicsSpecResourceType.h"
#include "SharedClasses/World/BrnVehicleGraphicsSpec.h"   // BrnVehicle::GraphicsSpec (the real, DWARF-named home)

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

    // The GraphicsSpec layout is no longer TU-local: it is the DWARF-named home
    // SharedClasses/World/BrnVehicleGraphicsSpec.h, whose member NAMES this file's
    // rebase arithmetic independently confirms three ways --
    //   +12 muShatteredGlassPartsCount : NOT rebased, and the descriptor charges
    //       12 bytes per entry == sizeof(ShatteredGlassPart)
    //   +20 mpPartLocators             : muPartsCount << 6 == 64 B per part (Matrix44Affine)
    //   +24 mpPartVolumeIDs / +28 mpNumRigidBodiesForPart : 1 byte per part each
    //   +28 mpNumRigidBodiesForPart    : each entry drives (count << 6) bytes of
    //       rigid-body -> skin matrices, which is what +32 points at
    //   +32 mppRigidBodyToSkinMatrixTransforms : 4 bytes per entry, and it is the
    //       one table FixUp rebases ELEMENT-WISE
    // (and the whole layout is runtime-probe-verified -- see that header's banner).

    // (KU_VEHICLE_GRAPHICS_SPEC_VERSION -- the X360 `*a2 != 3` gate -- now lives with the
    //  type in BrnVehicleGraphicsSpec.h, where the runtime probe confirmed the shipped
    //  VEH_PUSMC01_GR.bin really does carry 3.)

    // Resource registry type id for the vehicle graphics-spec resource (0x10006).
    // Recovered verbatim from GetTypeID @ 0x82676480.
    static const uint32_t KU_VEHICLE_GRAPHICS_SPEC_RESOURCE_TYPE_ID = 65542;

    uint32_t GraphicsSpecResourceType::GetTypeID() const
    {
        return KU_VEHICLE_GRAPHICS_SPEC_RESOURCE_TYPE_ID;
    }

    // Round a running size up to a 16-byte boundary after adding the next sub-block's
    // requirement: the X360 emits `addi N,0xF ; clrrwi N,4` for each step
    // (== (size + 15) & ~0xF).
    static u32 AlignUp16(u32 luSize)
    {
        return (luSize + 0xFu) & ~0xFu;
    }

    // GetSerialisedResourceDescriptor @ 0x8267D380 (store-for-store). The vehicle
    // graphics-spec serialises into one 16-byte-aligned block whose size accumulates
    // several sub-allocations, each rounded up to 16 bytes. The X360 reads:
    //   v3 = miCount (a3[1], +4) ; muField3 (a3[3], +12) ; mpField7 (a3[7], +0x1C, a byte array).
    // Running size (each `& ~0xF` is a 16-byte align-up of the accumulated total):
    //   s  = (4*miCount + 0x3F) & ~0xF             ; pointer table + slack
    //   s  = (s + 12*muField3 + 0xF) & ~0xF
    //   s  = (s + (miCount << 6) + 0xF) & ~0xF
    //   s  = (s + miCount + 0xF) & ~0xF
    //   s  = (s + miCount + 0xF) & ~0xF
    //   s  = (s + 4*miCount + 0xF) & ~0xF
    //   for (i = 0; i < miCount; ++i)            // skipped entirely when miCount == 0
    //       if (mpField7 && mpField7[i]) s += mpField7[i] << 6;
    //   size = (s + 0xF) & ~0xF
    // entry0 = {size, align = 0x10}; entries 1..4 = {0,1}.
    CgsResource::ResourceDescriptor
    GraphicsSpecResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const GraphicsSpec* lpSpec = static_cast<const GraphicsSpec*>(lpResource);

        const u32 luCount = static_cast<u32>(lpSpec->muPartsCount);   // v3 (a3[1])

        u32 luSize = (4u * luCount + 0x3Fu) & ~0xFu;             // (4*v3 + 63) & ~0xF
        luSize = AlignUp16(luSize + 12u * lpSpec->muShatteredGlassPartsCount);     // + 12*a3[3]
        luSize = AlignUp16(luSize + (luCount << 6));             // + (v3 << 6)
        luSize = AlignUp16(luSize + luCount);                    // + v3
        luSize = AlignUp16(luSize + luCount);                    // + v3
        luSize = AlignUp16(luSize + 4u * luCount);               // + 4*v3

        if (luCount != 0u)
        {
            const u8* lpBytes = PointerFromU32<u8>(lpSpec->mpNumRigidBodiesForPart);   // a3[7]
            for (u32 luEntry = 0; luEntry < luCount; ++luEntry)
            {
                if (lpBytes != nullptr && lpBytes[luEntry] != 0u)
                    luSize += static_cast<u32>(lpBytes[luEntry]) << 6;  // byte << 6
            }
        }

        luSize = AlignUp16(luSize);                              // final (s + 15) & ~0xF

        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;  // entry0 size
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 0x10u;   // entry0 align
        for (u32 luBlock = 1; luBlock < 5u; ++luBlock)
        {
            lDescriptor.m_baseResourceDescriptors[luBlock].m_size      = 0u;   // entry1..4 {0,1}
            lDescriptor.m_baseResourceDescriptors[luBlock].m_alignment = 1u;
        }
        return lDescriptor;
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

        lpSpec->mppPartModels += luDelta;   // a2[2] (+8)
        lpSpec->mpShatteredGlassParts += luDelta;   // a2[4] (+16)
        lpSpec->mpPartLocators += luDelta;   // a2[5] (+20)
        lpSpec->mpNumRigidBodiesForPart += luDelta;   // a2[7] (+28)
        lpSpec->mppRigidBodyToSkinMatrixTransforms  += luDelta;   // a2[8] (+32)

        if (lpSpec->muPartsCount)           // if (v13 = a2[1])
        {
            u32* lpArray = PointerFromU32<u32>(lpSpec->mppRigidBodyToSkinMatrixTransforms);
            for (u32 luEntry = 0; luEntry < static_cast<u32>(lpSpec->muPartsCount); ++luEntry)
                lpArray[luEntry] += luDelta;
        }

        lpSpec->mpPartVolumeIDs += luDelta;   // a2[6] (+24)
    }

    // FixDown @ 0x8267E338. The inverse of FixUp: subtract the load-base delta
    // from the array entries (guarded as in the asm) then from each pointer field.
    // void by contract.
    void GraphicsSpecResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        GraphicsSpec* lpSpec = static_cast<GraphicsSpec*>(lpResource);

        const u32 luDelta = CgsResource::GetLoadBase(lrResource);

        lpSpec->mpPartVolumeIDs -= luDelta;   // *(result + 24) -= *a2

        if (lpSpec->muPartsCount)           // if (v3 = *(result + 4))
        {
            // FLAG: the asm guards each array-entry write on *(result + 28)
            //       (mpField7), NOT on the array base *(result + 32). The array
            //       base it dereferences is mpArray (+32). Reproduced verbatim.
            u32* lpArray = PointerFromU32<u32>(lpSpec->mppRigidBodyToSkinMatrixTransforms);
            for (u32 luEntry = 0; luEntry < static_cast<u32>(lpSpec->muPartsCount); ++luEntry)
            {
                if (lpSpec->mpNumRigidBodiesForPart)
                    lpArray[luEntry] -= luDelta;
            }
        }

        lpSpec->mppRigidBodyToSkinMatrixTransforms  -= luDelta;   // *(result + 32) -= *a2
        lpSpec->mpNumRigidBodiesForPart -= luDelta;   // *(result + 28) -= *a2
        lpSpec->mpPartLocators -= luDelta;   // *(result + 20) -= *a2
        lpSpec->mppPartModels -= luDelta;   // *(result + 8)  -= *a2
        lpSpec->mpShatteredGlassParts -= luDelta;   // *(result + 16) -= *a2
    }
}
