#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::RwShaderParameterResourceType::FixDown  @ 0x828A8C30
//   CgsResource::RwShaderParameterResourceType::FixUp    @ 0x828A8C98
//   CgsResource::RwShaderParameterResourceType::GetTypeID@ 0x82876410
//
// FixUp/FixDown are pointer-relocation passes run when a serialised shader-parameter
// resource is mapped in/out. The resource holds two relocatable pointers at offsets
// 0 and 4. The first points to a parameter header { u32 muCount; u32; void* mpParams }.
// mpParams points at a packed array of muCount 28-byte (7-dword) entries, beginning
// 4 bytes into the block; within each entry the words at +0 and +8 are themselves
// relocatable. FixUp adds the relocation delta, FixDown subtracts it.
//
// Ordering matches the X360 build: FixUp relocates the header/pointer fields first,
// then walks the (already relocated) entries; FixDown walks the entries first, then
// relocates the pointers. Reconstructed with explicit byte arithmetic to stay
// faithful to the recovered layout rather than inventing typed structs.

namespace CgsResource
{
    class RwShaderParameterResourceType
    {
    public:
        void FixDown(void* pResource, const int* pDelta);
        void FixUp(void* pResource, const int* pDelta);

        int GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 20;

        static u32& Word(uintptr_t base, int byteOffset)
        {
            return *reinterpret_cast<u32*>(base + byteOffset);
        }
    };

    void RwShaderParameterResourceType::FixDown(void* pResource, const int* pDelta)
    {
        const int liDelta = *pDelta;
        uintptr_t lBase = reinterpret_cast<uintptr_t>(pResource);

        uintptr_t lHeader = Word(lBase, 0);
        Word(lBase, 4) -= liDelta;

        int liCount = static_cast<int>(Word(lHeader, 0));
        uintptr_t lParams = Word(lHeader, 8);

        if (Word(lHeader, 0))
        {
            uintptr_t lEntry = lParams + 4;
            do
            {
                --liCount;
                Word(lEntry, 8) -= liDelta;
                Word(lEntry, 0) -= liDelta;
                lEntry += 28;
            } while (liCount);
        }

        Word(lHeader, 8) = static_cast<u32>(lParams - liDelta);
        Word(lBase, 0) = static_cast<u32>(lHeader - liDelta);
    }

    void RwShaderParameterResourceType::FixUp(void* pResource, const int* pDelta)
    {
        const int liDelta = *pDelta;
        uintptr_t lBase = reinterpret_cast<uintptr_t>(pResource);

        uintptr_t lHeader = Word(lBase, 0) + liDelta;
        u32 luField4 = Word(lBase, 4) + liDelta;
        Word(lBase, 0) = static_cast<u32>(lHeader);
        Word(lBase, 4) = luField4;

        int liCount = static_cast<int>(Word(lHeader, 0));
        uintptr_t lParams = Word(lHeader, 8) + liDelta;
        bool lbEmpty = (Word(lHeader, 0) == 0);
        Word(lHeader, 8) = static_cast<u32>(lParams);

        if (!lbEmpty)
        {
            uintptr_t lEntry = lParams + 4;
            do
            {
                --liCount;
                Word(lEntry, 8) += liDelta;
                Word(lEntry, 0) += liDelta;
                lEntry += 28;
            } while (liCount);
        }
    }
}
