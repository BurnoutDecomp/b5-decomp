#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::GuiPopupResourceType::FixDown                       @ 0x82851FA8
//   CgsResource::GuiPopupResourceType::FixUp                         @ 0x82851F98
//   CgsResource::GuiPopupResourceType::GetSerialisedResourceDescriptor @ 0x82855A18
//   CgsResource::GuiPopupResourceType::GetTypeID                     @ 0x8284BC90
//
// FixUp/FixDown forward to CgsGui::GuiPopupResource (reconstructed in
// CgsGuiPopupResource.cpp); FixDown passes the "deep" flag (1). GetSerialisedResource-
// Descriptor builds the standard five-entry rw descriptor table: entry 0 is
// {size=16, count=N}, the remaining four are {size=N, align=1}, where N is read from
// the resource. The first pair is written by a single 64-bit store.

namespace CgsGui
{
    // Forward declaration; defined in CgsGuiPopupResource.cpp.
    struct GuiPopupResource
    {
        GuiPopupResource* FixUp(int liDelta);
        GuiPopupResource* FixDown(int liDelta, bool lbDeep);
    };
}

namespace CgsResource
{
    class GuiPopupResourceType
    {
    public:
        CgsGui::GuiPopupResource* FixDown(CgsGui::GuiPopupResource* pResource, int* pDelta)
        {
            return pResource->FixDown(*pDelta, true);
        }

        CgsGui::GuiPopupResource* FixUp(CgsGui::GuiPopupResource* pResource, int* pDelta)
        {
            return pResource->FixUp(*pDelta);
        }

        void* GetSerialisedResourceDescriptor(void* pOut, const void* pResource);

        int GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 31;
    };

    void* GuiPopupResourceType::GetSerialisedResourceDescriptor(void* pOut, const void* pResource)
    {
        uintptr_t out = reinterpret_cast<uintptr_t>(pOut);
        // Count is read from the resource (byte offset 6 in the X360 build).
        int liCount = *reinterpret_cast<const int*>(reinterpret_cast<uintptr_t>(pResource) + 6);

        *reinterpret_cast<int*>(out + 12) = 1;
        *reinterpret_cast<int*>(out + 20) = 1;
        *reinterpret_cast<int*>(out + 28) = 1;
        *reinterpret_cast<int*>(out + 36) = 1;

        *reinterpret_cast<int*>(out + 8)  = liCount;
        *reinterpret_cast<int*>(out + 16) = liCount;
        *reinterpret_cast<int*>(out + 24) = liCount;
        *reinterpret_cast<int*>(out + 32) = liCount;

        // 64-bit store of {high=count, low=16} over the first two words.
        *reinterpret_cast<int*>(out + 0) = liCount;
        *reinterpret_cast<int*>(out + 4) = 16;

        return pOut;
    }
}
