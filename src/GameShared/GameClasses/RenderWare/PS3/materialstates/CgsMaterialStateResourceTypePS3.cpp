#include "GameShared/GameClasses/RenderWare/PS3/materialstates/CgsMaterialStateResourceTypePS3.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::MaterialStateResourceType::FixDown   @ 0x828A8A80
//   CgsResource::MaterialStateResourceType::FixUp     @ 0x828A8AB8
//   CgsResource::MaterialStateResourceType::GetTypeID @ 0x828A8108
//
// FixUp rebases three pointer fields (offsets 0/4/8) by the delta (the rw::Resource's
// load base). FixDown reverses that and flags the pointed-to material-state object
// (read from +8 before it is un-rebased) as needing a rebuild (byte at +40).

namespace CgsResource
{
    static const uint32_t KU_MATERIAL_STATE_RESOURCE_TYPE_ID = 15;

    uint32_t MaterialStateResourceType::GetTypeID() const
    {
        return KU_MATERIAL_STATE_RESOURCE_TYPE_ID;
    }

    void MaterialStateResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        int       liDelta = static_cast<int>(CgsResource::GetLoadBase(lrResource));
        uintptr_t lBase   = reinterpret_cast<uintptr_t>(lpResource);

        uintptr_t lMaterialState = *reinterpret_cast<uintptr_t*>(lBase + 8);

        *reinterpret_cast<uintptr_t*>(lBase + 0) -= liDelta;
        *reinterpret_cast<uintptr_t*>(lBase + 4) -= liDelta;
        *reinterpret_cast<u8*>(lMaterialState + 40) = 1;
        *reinterpret_cast<uintptr_t*>(lBase + 8) -= liDelta;
    }

    void MaterialStateResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        int       liDelta = static_cast<int>(CgsResource::GetLoadBase(lrResource));
        uintptr_t lBase   = reinterpret_cast<uintptr_t>(lpResource);

        *reinterpret_cast<uintptr_t*>(lBase + 0) += liDelta;
        *reinterpret_cast<uintptr_t*>(lBase + 4) += liDelta;
        *reinterpret_cast<uintptr_t*>(lBase + 8) += liDelta;
    }
}
