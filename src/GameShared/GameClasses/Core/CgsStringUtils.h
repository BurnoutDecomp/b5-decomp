#pragma once
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

#include <string.h>

// Global legacy helper (all-caps symbol preserved for cross-build identity):
// Lower-cases up to uMaxLen characters in place (DecFIGS CgsStringUtils.cpp:46).
extern void CGSSTRNLOWER(char* pcString, u32 uMaxLen);

namespace CgsCore
{

    extern void SPrintf(char* buffer, u32 len, const char* fmt, ...);
    extern void SnPrintf(char* buffer, u32 len, const char* fmt, ...);
#define CGS_ARTIST_STRING_UTILS_FILE "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h"

    __forceinline void StrnCpy(char* lpcDest, const char* lpcSource, u32 luBytes)
    {
        CGS_ASSERT_STREAM_AT(strlen(lpcSource) < luBytes, "String too long: " << lpcSource,
            CGS_ARTIST_STRING_UTILS_FILE, 55);
        strncpy(lpcDest, lpcSource, luBytes);
    }

    __forceinline void StrCpy(char* lpcDest, u32 luBytes, const char* lpcSource)
    {
        if (strlen(lpcSource) >= luBytes)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            const u32 luLength = static_cast<u32>(strlen(lpcSource)); // ARTIST measures the length here at 0x828DE11C.
            lStrStream << "String " << lpcSource << " is too long. Buffer size = " << luBytes
                << ", string length = " << luLength << "\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
                CGS_ASSERT_LOCATION(CGS_ARTIST_STRING_UTILS_FILE, 65));
            CgsDev::Assert::EndAssert();
        }
        strcpy(lpcDest, lpcSource);
    }

    __forceinline void StrCat(char* lpcDest, u32 luBytes, const char* lpcSource)
    {
        CGS_ASSERT_AT((strlen(lpcSource) + strlen(lpcDest)) < luBytes - 1,
            "(strlen(lpcSource)+strlen(lpcDest))<luBytes - 1",
            CGS_ARTIST_STRING_UTILS_FILE, 75);
        strcat(lpcDest, lpcSource);
    }

#undef CGS_ARTIST_STRING_UTILS_FILE

}

// ============================================================================
// [friends wave 2026-08-26] ADDITIVE PROMOTION -- declared at GLOBAL scope because
// the single out-of-line definition (body home: CgsServerInterfaceGames.cpp, which
// declares it at file scope) is a plain ::LobbyNameCmp on X360, consumed by both the
// DirtySock lobby code and BrnGui::FriendsListComponent's buddy-list passes
// (SortFullList/BuddySortFunction/ShowSpecificFriend/MoveHighlightDueToBranchOpen).
// ============================================================================
// Linkage: the DirtySock home wraps the definition in extern "C" (the X360 exports
    // one plain-global symbol); the friends-list TU must see the same linkage.
extern "C" s32 LobbyNameCmp(const char* pNameA, const char* pNameB);
