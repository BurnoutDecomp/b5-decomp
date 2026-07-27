#include "GameShared/GameClasses/Graphics/Instances/CgsInstanceListResourceType.h"

#include <cstddef>               // offsetof
#include "types.hpp"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/Instances/CgsInstance.h"   // the on-disc InstanceList/Instance layout

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsGraphics::InstanceListResourceType::GetTypeID  @ 0x827E6EE8  (EXECUTED in goal trace)
//   CgsGraphics::InstanceListResourceType::FixDown    @ 0x827F94C8
//   CgsGraphics::InstanceListResourceType::PostFixUp  @ 0x827E6EF0
//
// GetTypeID returns the resource registry id 35 (0x23). FixDown delegates to the
// committed CgsGraphics::InstanceList::FixDown member (the relocation rebase).
// PostFixUp walks the instance array and pre-computes each instance's squared
// LOD radius from its model's coarsest LOD.
//
// The instance array is an ON-DISK resource (relocated by FixUp/FixDown), so it has
// the X360 32-bit layout: 80-byte stride, 4-byte pointer slots. Pointer fields are
// therefore modelled as load-relative u32 + PointerFromU32 (the committed
// VehicleListResourceType / CgsInstance.cpp convention) so the struct offsets and
// the stride-80 walk are correct on the 64-bit host.

namespace CgsGraphics
{
    template <typename T>
    static T* PointerFromU32(u32 luAddress)
    {
        return reinterpret_cast<T*>(static_cast<uintptr_t>(luAddress));
    }

    // The on-disc InstanceList / Instance layout now lives in the committed
    // CgsInstance.h (32-bit slots, 16-byte header, 80-byte element). The TU-local
    // mirrors this file used to carry are gone -- they had drifted to host-width
    // pointers, which put muArraySize at byte +8 and spliced the instance-buffer slot
    // with it.

    // ---------------------------------------------------------------------------
    // FLAG: minimal slice of the (NOT committed) CgsModel body. Only the two members
    // PostFixUp reads are modelled, at their X360 BYTE offsets -- the asm loads are
    // byte-displacement, NOT dword-scaled (the Hex-Rays `*(LODWORD(v6)+8/+18)` render
    // is a byte-pointer index, so the bytes are +0x08 and +0x12, not +0x20/+0x48):
    //   muLodRadii  @ +0x08 (asm `lwz r11,8(r31)`)  load-relative u32 -> const f32* radius table
    //   muNumLods   @ +0x12 (asm `lbz r11,0x12(r31)`) 8-bit LOD count
    // The X360 inlines GetNumLods()/GetLodRadius(); modelled here as inline accessors
    // over those fields (the radius table is itself an on-disk u32 ptr -> PointerFromU32).
    // Padding/field names INFERRED; offsets verified vs the asm.
    // ---------------------------------------------------------------------------
    struct SerialisedModel
    {
        u8  maPad0[8];      // +0x00..+0x08
        u32 muLodRadii;     // +0x08  (lwz 8) coarsest-first radius table (u32 ptr)
        u8  maPad12[6];     // +0x0C..+0x12
        u8  muNumLods;      // +0x12  (lbz 0x12) number of LODs (BYTE)

        u32 GetNumLods() const { return muNumLods; }
        f32 GetLodRadius(u32 luIndex) const { return PointerFromU32<f32>(muLodRadii)[luIndex]; }
    };

    static_assert(offsetof(SerialisedModel, muLodRadii) == 8,  "muLodRadii must be at +0x08 (lwz 8)");
    static_assert(offsetof(SerialisedModel, muNumLods)  == 18, "muNumLods must be at +0x12 (lbz 0x12)");

    static const u32 KU_INSTANCE_STRIDE = 80;   // bytes per Instance (X360 v4 += 80)

    static_assert(sizeof(Instance) == KU_INSTANCE_STRIDE, "Instance must be 80 bytes (X360 stride)");
    static_assert(offsetof(Instance, mfMaxDrawDistanceSq) == 12, "mfMaxDrawDistanceSq must be at +12 (v5[3])");

    // GetTypeID @ 0x827E6EE8 (EXECUTED): return 35.
    static const uint32_t KU_INSTANCE_LIST_RESOURCE_TYPE_ID = 35;   // 0x23

    uint32_t InstanceListResourceType::GetTypeID() const
    {
        return KU_INSTANCE_LIST_RESOURCE_TYPE_ID;
    }

    // GetSerialisedResourceDescriptor @ 0x827F09B0 (store-for-store). Builds the
    // five-entry serialised descriptor (rw::BaseResourceDescriptors<5>):
    //   entry0 = { size = 80*muArraySize + 16, align = 16 }
    //   entry1..4 = { size = 0, align = 1 }
    // muArraySize is *(resource + 4) (the InstanceList's total entry count); the block
    // is the 16-byte InstanceList header plus muArraySize * 80-byte Instance records.
    // The asm writes all five alignments (four 1's via stw, entry0's 16 via the 64-bit
    // store) and the four trailing zero sizes, then the single std of {size, 16} sets
    // entry0. (Hex-Rays renders the 8-byte {size,align} entry stride as a spurious
    // 12-byte stride via the HIDWORD/LODWORD qword-half misread; the asm offsets prove
    // a flat 8-byte stride: +0/+4, +8/+12, +16/+20, +24/+28, +32/+36.)
    CgsResource::ResourceDescriptor
    InstanceListResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const InstanceList* lpList = static_cast<const InstanceList*>(lpResource);

        CgsResource::ResourceDescriptor lDescriptor;
        rw::BaseResourceDescriptor* lpEntries = lDescriptor.m_baseResourceDescriptors;

        lpEntries[0].m_size      = KU_INSTANCE_STRIDE * lpList->muArraySize + 16u; // 80*n + header
        lpEntries[0].m_alignment = 16u;
        for (u32 luEntry = 1u; luEntry < 5u; ++luEntry)
        {
            lpEntries[luEntry].m_size      = 0u;
            lpEntries[luEntry].m_alignment = 1u;
        }
        return lDescriptor;
    }

    // FixDown @ 0x827F94C8. The X360 is `return CgsGraphics::InstanceList::FixDown(a2, *a3)`
    // — a member call on the InstanceList (a2) with the load-base delta (*a3). The
    // ResourceType::FixDown contract is void, so the member's InstanceList* return is
    // discarded.
    void InstanceListResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<InstanceList*>(lpResource)->FixDown(static_cast<int>(CgsResource::GetLoadBase(lrResource)));
    }

    // PostFixUp @ 0x827E6EF0. lrResource is unused. For each of the muArraySize
    // (a2[1]) instances (stride 80), validate the model and pre-compute the squared
    // LOD radius from the coarsest LOD: mfLodRadiusSquared = radius*radius where
    // radius = mpaLodRadii[numLods - 1].
    //
    // The X360 fires three asserts (lpModel, GetNumLods() > 0, Invalid LOD index).
    // The third is the inlined CgsModel LOD-accessor's own bounds check (its baked
    // path points at CgsModel.h:367, but it is a plain CGS_ASSERT here — no baked
    // line numbers). The X360 `result` is the EndAssert() artifact; void by contract.
    void InstanceListResourceType::PostFixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        (void)lrResource;

        InstanceList* lpList = static_cast<InstanceList*>(lpResource);

        if (lpList->muArraySize)   // if ( a2[1] )
        {
            for (u32 luI = 0; luI < lpList->muArraySize; ++luI)
            {
                // v5 = mpaInstances + luI*80 (Instance*); v6 = *v5 = mpModel.
                Instance* lpInstance = lpList->GetInstance(luI);
                SerialisedModel* lpModel = PointerFromU32<SerialisedModel>(lpInstance->mpModel.muSlot);

                // [FLAG PC boot gate] the model is a bundle IMPORT; an unresolved import
                // leaves the slot null and the console-faithful walk below dereferences it.
                // Skip the entry (its LOD radius stays as serialised) and let the load
                // continue. DELETE once every world bundle resolves its imports.
                if (lpModel == 0)
                {
                    continue;
                }

                CGS_ASSERT(lpModel != nullptr, "lpModel");                          // :245
                CGS_ASSERT(lpModel->GetNumLods() > 0, "lpModel->GetNumLods() > 0"); // :246
                if (lpModel->GetNumLods() == 0)
                {
                    continue;
                }

                u32 luLastLod = lpModel->GetNumLods() - 1;          // v8 = dword18 - 1

                // Inlined CgsModel LOD-accessor bounds assert (X360 re-tests dword18).
                CGS_ASSERT(lpModel->GetNumLods() != 0, "Invalid LOD index");        // CgsModel.h:367

                f32 lfRadius = lpModel->GetLodRadius(luLastLod);    // table[v8]
                lpInstance->mfMaxDrawDistanceSq = lfRadius * lfRadius;   // v5[3]
            }
        }
    }

    // FixUp @ CgsInstanceListResourceType.cpp:203 (the X360 override; no ARTIST export
    // address in the name index, but the member it forwards to IS exported).
    // The X360 body is the mirror of FixDown: `CgsGraphics::InstanceList::FixUp(a2, *a3)`
    // -- rebase the instance-buffer slot by the load base and tripwire the data version.
    // Without it the streamed list keeps its file-relative buffer offset and PostFixUp
    // walks from address 0x10.
    void InstanceListResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<InstanceList*>(lpResource)->FixUp(static_cast<int>(CgsResource::GetLoadBase(lrResource)));
    }
}
