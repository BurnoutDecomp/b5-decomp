#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::HudMessageResourceType::FixDown   @ 0x828465E8
//   CgsResource::HudMessageResourceType::FixUp     @ 0x828465D8
//   CgsResource::HudMessageResourceType::GetTypeID @ 0x82846578
//
// FixDown un-rebases the message pointer table (count at offset 8, base pointer at
// offset 0) and then the base pointer itself. FixUp forwards to the message
// resource's own FixUp (separate TU; forward-declared).

namespace CgsGui
{
    struct GuiHudMessageResource
    {
        int FixUp(int delta);
    };
}

namespace CgsResource
{
    class HudMessageResourceType
    {
    public:
        void* FixDown(void* pResource, int* pDelta);
        int   FixUp(CgsGui::GuiHudMessageResource* pResource, int* pDelta)
        {
            return pResource->FixUp(*pDelta);
        }
        int GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 44;
    };

    void* HudMessageResourceType::FixDown(void* pResource, int* pDelta)
    {
        int delta = *pDelta;
        uintptr_t base = reinterpret_cast<uintptr_t>(pResource);

        int count = *reinterpret_cast<int*>(base + 8);
        if (count > 0)
        {
            uintptr_t* table = *reinterpret_cast<uintptr_t**>(base);
            for (int i = 0; i < count; ++i)
                table[i] -= delta;
        }

        *reinterpret_cast<uintptr_t*>(base) -= delta;
        return pResource;
    }
}
