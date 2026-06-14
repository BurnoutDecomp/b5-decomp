#include "GameShared/GameClasses/RenderWare/PS3/materialstates/CgsRwShaderParameterResourceTypePS3.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::RwShaderParameterResourceType::FixDown  @ 0x828A8C30
//   CgsResource::RwShaderParameterResourceType::FixUp    @ 0x828A8C98
//   CgsResource::RwShaderParameterResourceType::GetTypeID@ 0x82876410
//
// FixUp/FixDown relocate a serialised shader-parameter resource by the delta (the
// rw::Resource's load base). The resource holds two relocatable pointers at +0/+4;
// the first points to a header { u32 muCount; u32; void* mpParams } whose mpParams
// is a packed array of muCount 28-byte entries (starting 4 bytes in) with relocatable
// words at +0/+8. FixUp relocates the header/pointers then walks the entries; FixDown
// walks the entries then relocates the pointers. Serialised blob -> offset access.

namespace CgsResource
{
    static const uint32_t KU_RW_SHADER_PARAMETER_RESOURCE_TYPE_ID = 20;

    uint32_t RwShaderParameterResourceType::GetTypeID() const
    {
        return KU_RW_SHADER_PARAMETER_RESOURCE_TYPE_ID;
    }

    void RwShaderParameterResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        const int liDelta = static_cast<int>(CgsResource::GetLoadBase(lrResource));
        uintptr_t lBase = reinterpret_cast<uintptr_t>(lpResource);

        uintptr_t lHeader = *reinterpret_cast<u32*>(lBase + 0);
        *reinterpret_cast<u32*>(lBase + 4) -= liDelta;

        int       liCount = static_cast<int>(*reinterpret_cast<u32*>(lHeader + 0));
        uintptr_t lParams = *reinterpret_cast<u32*>(lHeader + 8);

        if (*reinterpret_cast<u32*>(lHeader + 0))
        {
            uintptr_t lEntry = lParams + 4;
            do
            {
                --liCount;
                *reinterpret_cast<u32*>(lEntry + 8) -= liDelta;
                *reinterpret_cast<u32*>(lEntry + 0) -= liDelta;
                lEntry += 28;
            } while (liCount);
        }

        *reinterpret_cast<u32*>(lHeader + 8) = static_cast<u32>(lParams - liDelta);
        *reinterpret_cast<u32*>(lBase + 0)   = static_cast<u32>(lHeader - liDelta);
    }

    void RwShaderParameterResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        const int liDelta = static_cast<int>(CgsResource::GetLoadBase(lrResource));
        uintptr_t lBase = reinterpret_cast<uintptr_t>(lpResource);

        uintptr_t lHeader = *reinterpret_cast<u32*>(lBase + 0) + liDelta;
        u32 luField4 = *reinterpret_cast<u32*>(lBase + 4) + liDelta;
        *reinterpret_cast<u32*>(lBase + 0) = static_cast<u32>(lHeader);
        *reinterpret_cast<u32*>(lBase + 4) = luField4;

        int       liCount = static_cast<int>(*reinterpret_cast<u32*>(lHeader + 0));
        uintptr_t lParams = *reinterpret_cast<u32*>(lHeader + 8) + liDelta;
        bool      lbEmpty = (*reinterpret_cast<u32*>(lHeader + 0) == 0);
        *reinterpret_cast<u32*>(lHeader + 8) = static_cast<u32>(lParams);

        if (!lbEmpty)
        {
            uintptr_t lEntry = lParams + 4;
            do
            {
                --liCount;
                *reinterpret_cast<u32*>(lEntry + 8) += liDelta;
                *reinterpret_cast<u32*>(lEntry + 0) += liDelta;
                lEntry += 28;
            } while (liCount);
        }
    }
}
