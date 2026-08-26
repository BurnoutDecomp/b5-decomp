#pragma once
#include "types.hpp"

// Global legacy helper (all-caps symbol preserved for cross-build identity):
// in-place lower-cases up to uMaxLen chars and returns the buffer.
extern char* CGSSTRNLOWER(char* pcString, u32 uMaxLen);

namespace CgsCore
{

    extern void SPrintf(char* buffer, u32 len, const char* fmt, ...);
    extern void SnPrintf(char* buffer, u32 len, const char* fmt, ...);
    extern void StrCpy(char* dest, u32 len, const char* src);
    extern void StrCat(char* dest, u32 len, const char* src);

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
