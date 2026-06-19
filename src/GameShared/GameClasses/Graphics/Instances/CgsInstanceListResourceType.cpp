#include "GameShared/GameClasses/Graphics/Instances/CgsInstanceListResourceType.h"

#include <cstddef>               // offsetof
#include "types.hpp"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

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

    // ---------------------------------------------------------------------------
    // FLAG: TU-local MIRROR of the COMMITTED-but-HEADERLESS CgsGraphics::InstanceList
    // (defined TU-local in CgsInstance.cpp). Layout is byte-identical to that home
    // (mpaInstances/muArraySize/muNumInstances/muVersionNumber) so it is ODR-safe;
    // FixDown(int) is declared (not defined) here and links to the committed symbol.
    // BaseCollisionGenerator / VehicleListResourceType precedent.
    // ---------------------------------------------------------------------------
    struct InstanceList
    {
        uintptr_t mpaInstances;    // +0x00 Instance* (base of the instance buffer)
        u32       muArraySize;     // +0x04 total Instance entries (PostFixUp loop count)
        u32       muNumInstances;  // +0x08 complete Instance entries
        u32       muVersionNumber; // +0x0C

        InstanceList* FixDown(int delta);   // committed symbol (CgsInstance.cpp)
    };

    struct CgsModel;

    // ---------------------------------------------------------------------------
    // FLAG: minimal slice of the (NOT committed) 80-byte on-disk Instance element.
    // Only the two fields PostFixUp touches are modelled; padding preserves the
    // X360 stride-80 / offset layout (4-byte pointer slot). Offsets verified vs the
    // X360 pseudocode: model @+0 (v6 = *v5), mfLodRadiusSquared @+12 (v5[3]). The
    // model pointer is a load-relative u32 (PointerFromU32). Field names INFERRED.
    // ---------------------------------------------------------------------------
    struct Instance
    {
        u32 muModel;              // +0x00  v6 = *v5 (the instance's model, u32 ptr)
        u8  maPad4[8];            // +0x04..+0x0C
        f32 mfLodRadiusSquared;   // +0x0C  v5[3] = radius * radius
        u8  maPad16[64];          // +0x10..+0x50 (stride = 80 bytes)

        CgsModel* GetModel() const { return PointerFromU32<CgsModel>(muModel); }
    };

    static const u32 KU_INSTANCE_STRIDE = 80;   // bytes per Instance (X360 v4 += 80)

    static_assert(sizeof(Instance) == KU_INSTANCE_STRIDE, "Instance must be 80 bytes (X360 stride)");
    static_assert(offsetof(Instance, mfLodRadiusSquared) == 12, "mfLodRadiusSquared must be at +12 (v5[3])");

    // ---------------------------------------------------------------------------
    // FLAG: minimal slice of the (NOT committed) CgsModel. Only the two members
    // PostFixUp reads are modelled, at their X360 dword offsets:
    //   muLodRadii  @ dword 8  = +0x20 (*(LODWORD(v6) + 8)), load-relative u32 -> const f32* table
    //   muNumLods   @ dword 18 = +0x48 (*(LODWORD(v6) + 18))
    // The X360 inlines GetNumLods()/GetLodRadius(); modelled here as inline accessors
    // over those fields (the radius table is itself an on-disk u32 ptr -> PointerFromU32).
    // Padding/field names INFERRED; offsets verified.
    // ---------------------------------------------------------------------------
    struct CgsModel
    {
        u8  maPad0[32];     // +0x00..+0x20
        u32 muLodRadii;     // +0x20  (dword 8) coarsest-first radius table (u32 ptr)
        u8  maPad36[36];    // +0x24..+0x48
        u32 muNumLods;      // +0x48  (dword 18) number of LODs

        u32 GetNumLods() const { return muNumLods; }
        f32 GetLodRadius(u32 luIndex) const { return PointerFromU32<f32>(muLodRadii)[luIndex]; }
    };

    static_assert(offsetof(CgsModel, muLodRadii) == 32, "muLodRadii must be at +0x20 (dword 8)");
    static_assert(offsetof(CgsModel, muNumLods) == 72, "muNumLods must be at +0x48 (dword 18)");

    // GetTypeID @ 0x827E6EE8 (EXECUTED): return 35.
    static const uint32_t KU_INSTANCE_LIST_RESOURCE_TYPE_ID = 35;   // 0x23

    uint32_t InstanceListResourceType::GetTypeID() const
    {
        return KU_INSTANCE_LIST_RESOURCE_TYPE_ID;
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
                Instance* lpInstance =
                    reinterpret_cast<Instance*>(lpList->mpaInstances + luI * KU_INSTANCE_STRIDE);
                CgsModel* lpModel = lpInstance->GetModel();

                CGS_ASSERT(lpModel != nullptr, "lpModel");                          // :245
                CGS_ASSERT(lpModel->GetNumLods() > 0, "lpModel->GetNumLods() > 0"); // :246

                u32 luLastLod = lpModel->GetNumLods() - 1;          // v8 = dword18 - 1

                // Inlined CgsModel LOD-accessor bounds assert (X360 re-tests dword18).
                CGS_ASSERT(lpModel->GetNumLods() != 0, "Invalid LOD index");        // CgsModel.h:367

                f32 lfRadius = lpModel->GetLodRadius(luLastLod);    // table[v8]
                lpInstance->mfLodRadiusSquared = lfRadius * lfRadius;   // v5[3]
            }
        }
    }
}
