#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::HudMessageListResourceType::FixDown   @ 0x82846648
//   CgsResource::HudMessageListResourceType::GetTypeID @ 0x828465D0
//
// FixDown is the inverse (save-time) relocation: it un-rebases the entry table by
// subtracting the delta from every one of the `count` (offset 4) entry pointers,
// then un-rebases the table base pointer (offset 8) itself.

namespace CgsResource
{
    class HudMessageListResourceType
    {
    public:
        void* FixDown(void* pList, int* pDelta);
        int   GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 45;
    };

    void* HudMessageListResourceType::FixDown(void* pList, int* pDelta)
    {
        int delta = *pDelta;
        uintptr_t base = reinterpret_cast<uintptr_t>(pList);

        int count = *reinterpret_cast<int*>(base + 4);
        if (count > 0)
        {
            uintptr_t* table = *reinterpret_cast<uintptr_t**>(base + 8);
            for (int i = 0; i < count; ++i)
                table[i] -= delta;
        }

        *reinterpret_cast<uintptr_t*>(base + 8) -= delta;
        return pList;
    }
}
