#include "SDKs/EA/GameTalk/GameTalk.h"

#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h" // CgsAttribSys::AttribSysMemoryManager
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h" // CgsAttribSys::AttribSysPackageAllocator
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

#include <cctype> // tolower

// ============================================================================
// SDKs/EA/GameTalk/GameTalk.cpp
//
// EA::GameTalk namespace-level helpers, reconstructed store-for-store from the
// X360 .XEX (BURNOUT_X360_ARTIST.XEX):
//   EA::GameTalk::Alloc      @ 0x82838000
//   EA::GameTalk::Free       @ 0x82836E18
//   EA::GameTalk::StrIEqual  @ 0x82836C60
//
// Alloc/Free route GameTalk's allocations through the AttribSys GameTalk package
// allocator. The X360 asm inlines AttribSysMemoryManager::GetGameTalkAllocator()
// -- it reads the manager's sbHasLinearAllocator flag directly and uses the
// static GameTalk package-allocator instance (&dword_83011B64), asserting against
// CgsAttribSysMemoryManager.h:192 that the manager has been Prepare'd. The
// portable reconstruction keeps that observable behaviour by going through the
// GetGameTalkAllocator() accessor (which performs the same assert and hands back
// the same instance), then forwarding to Malloc(size, 0) / Free(block). The NULL
// guard short-circuits before the allocator is touched (Alloc returns 0; Free
// returns its argument unchanged).
// ============================================================================

namespace EA
{
namespace GameTalk
{
    // @ 0x82838000 -- allocate liSize bytes from the GameTalk package allocator.
    // r3 = size; NULL size returns 0 without touching the allocator. Otherwise the
    // asm asserts sbHasLinearAllocator then calls Malloc(&dword_83011B64, size, 0).
    void* Alloc(s32 liSize)
    {
        if (!liSize)
            return 0;

        return CgsAttribSys::AttribSysMemoryManager::GetGameTalkAllocator()->Malloc(
            static_cast<size_t>(liSize), 0);
    }

    // @ 0x82836E18 -- free a block previously returned by Alloc. r3 = block; a NULL
    // block is returned unchanged without touching the allocator. Otherwise the asm
    // asserts sbHasLinearAllocator then calls Free(&dword_83011B64, block). The X360
    // leaves r3 as the allocator Free result (Hex-Rays types the function as
    // returning the original pointer on the NULL-block path); modelled likewise.
    void* Free(void* lpBlock)
    {
        if (lpBlock)
        {
            CgsAttribSys::AttribSysMemoryManager::GetGameTalkAllocator()->Free(lpBlock);
        }
        return lpBlock;
    }

    // @ 0x82836C60 -- case-insensitive C-string equality. Walks both strings in
    // lockstep comparing tolower() of each byte; on the first mismatch returns false,
    // and on reaching the end of either string returns whether both terminated at the
    // same position (i.e. *lpcLhs == *lpcRhs at the stop point). Matches the X360
    // store order: lpcLhs is r31 (advanced via r2), lpcRhs is r30.
    bool StrIEqual(const char* lpcLhs, const char* lpcRhs)
    {
        const char* lpcL = lpcLhs;
        int liLeftChar = static_cast<signed char>(*lpcL);
        if (liLeftChar)
        {
            while (*lpcRhs)
            {
                int liRightLower = tolower(static_cast<signed char>(*lpcRhs++));
                ++lpcL;
                if (tolower(liLeftChar) != liRightLower)
                    return false;
                liLeftChar = static_cast<signed char>(*lpcL);
                if (!*lpcL)
                    break;
            }
        }
        return static_cast<signed char>(*lpcRhs) == static_cast<signed char>(*lpcL);
    }
}
}
