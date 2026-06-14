#include "types.hpp"
#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::PFXHookBundleResourceType::FixDown   @ 0x8250B128
//   CgsResource::PFXHookBundleResourceType::FixUp     @ 0x8250B038
//   CgsResource::PFXHookBundleResourceType::GetTypeID @ 0x824F5B90
//   CgsResource::PFXHookBundleResourceType::Serialise @ 0x82512478
//
// The bundle is {hookCount, secondCount, hook-pointer array, secondary-pointer array}. FixDown
// relativises every hook (delegating to BrnGui::PFXHook::FixDown) and both pointer arrays;
// FixUp does the inverse and inlines the per-hook fix-up (rebasing each hook's sub-array at
// +52/+56 and the sub-elements' +4 pointers). Serialise relativises the source, copies the
// block, rebases the copy, then restores the source. Serialised addresses are 32-bit by format.

namespace BrnGui
{
    struct PFXHook
    {
        void FixDown(int liDelta);
    };
}

namespace CgsResource
{
namespace
{
    struct PFXHookBundle
    {
        u32 muHookCount;     // +0
        u32 muSecondCount;   // +4
        u32 muHookArray;     // +8   -> hook-pointer array
        u32 muSecondArray;   // +12  -> secondary-pointer array
        u32 muSize;          // +16  serialised size
    };

    struct PFXHookView       // a single hook
    {
        u8  mPad0[52];
        u32 muSubArray;      // +52  -> sub-element pointer array
        u32 muSubCount;      // +56
    };
}

class PFXHookBundleResourceType
{
public:
    void FixDown(void* pResource, void* pBundle, int liDelta);
    int  FixUp(void* pResource, void* pBundle, int liDelta);
    int  GetTypeID() { return KI_TYPE_ID; }
    void* Serialise(void* pResource, void* pSource, int* pDestination);

private:
    static const int KI_TYPE_ID = 49;
};

void PFXHookBundleResourceType::FixDown(void* /*pResource*/, void* pBundle, int liDelta)
{
    PFXHookBundle* lpBundle = static_cast<PFXHookBundle*>(pBundle);
    const u32 luDelta = static_cast<u32>(liDelta);

    u32* lpHooks = reinterpret_cast<u32*>(static_cast<uintptr_t>(lpBundle->muHookArray));
    for (u32 luHook = 0; luHook < lpBundle->muHookCount; ++luHook)
    {
        reinterpret_cast<BrnGui::PFXHook*>(static_cast<uintptr_t>(lpHooks[luHook]))->FixDown(liDelta);
        lpHooks[luHook] -= luDelta;
    }

    u32* lpSecond = reinterpret_cast<u32*>(static_cast<uintptr_t>(lpBundle->muSecondArray));
    for (u32 luEntry = 0; luEntry < lpBundle->muSecondCount; ++luEntry)
        lpSecond[luEntry] -= luDelta;

    lpBundle->muHookArray   -= luDelta;
    lpBundle->muSecondArray -= luDelta;
}

int PFXHookBundleResourceType::FixUp(void* /*pResource*/, void* pBundle, int liDelta)
{
    PFXHookBundle* lpBundle = static_cast<PFXHookBundle*>(pBundle);
    const u32 luDelta = static_cast<u32>(liDelta);

    lpBundle->muHookArray   += luDelta;
    lpBundle->muSecondArray += luDelta;

    int liCount = 0;
    u32* lpHooks = reinterpret_cast<u32*>(static_cast<uintptr_t>(lpBundle->muHookArray));
    for (u32 luHook = 0; luHook < lpBundle->muHookCount; ++luHook)
    {
        lpHooks[luHook] += luDelta;
        PFXHookView* lpHook = reinterpret_cast<PFXHookView*>(static_cast<uintptr_t>(lpHooks[luHook]));

        const u32 luSubCount = lpHook->muSubCount;
        lpHook->muSubArray += luDelta;
        u32* lpSub = reinterpret_cast<u32*>(static_cast<uintptr_t>(lpHook->muSubArray));
        for (u32 luSub = 0; luSub < luSubCount; ++luSub)
        {
            lpSub[luSub] += luDelta;
            *reinterpret_cast<u32*>(static_cast<uintptr_t>(lpSub[luSub]) + 4) += luDelta;
        }
        ++liCount;
    }

    u32* lpSecond = reinterpret_cast<u32*>(static_cast<uintptr_t>(lpBundle->muSecondArray));
    for (u32 luEntry = 0; luEntry < lpBundle->muSecondCount; ++luEntry)
        lpSecond[luEntry] += luDelta;

    return liCount;
}

void* PFXHookBundleResourceType::Serialise(void* pResource, void* pSource, int* pDestination)
{
    PFXHookBundle* lpSource = static_cast<PFXHookBundle*>(pSource);
    const u32 luSize = lpSource->muSize;
    uintptr_t lDest  = static_cast<uintptr_t>(*pDestination);
    const int liSrcBase = static_cast<int>(reinterpret_cast<uintptr_t>(lpSource));

    FixDown(pResource, lpSource, liSrcBase);                 // relativise source
    memcpy(reinterpret_cast<void*>(lDest), lpSource, luSize);
    FixUp(pResource, reinterpret_cast<void*>(lDest), static_cast<int>(lDest));  // rebase copy
    FixUp(pResource, lpSource, liSrcBase);                   // restore source

    return reinterpret_cast<void*>(lDest);
}
}
