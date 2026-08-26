#pragma once
#include "types.hpp"

// Global legacy helper (all-caps symbol preserved for cross-build identity):
// in-place lower-cases up to uMaxLen chars and returns the buffer.
extern char* CGSSTRNLOWER(char* pcString, u32 uMaxLen);

namespace CgsCore
{

    // LobbyNameCmp -- ADDITIVE PROMOTION [friends wave 2026-08-26]: declared here so
    // BrnGui::FriendsListComponent (SortFullList/BuddySortFunction/ShowSpecificFriend/
    // MoveHighlightDueToBranchOpen) resolves the same out-of-line definition the DirtySock
    // lobby TU already links (body home: CgsServerInterfaceGames.cpp).
    s32 LobbyNameCmp(const char* pNameA, const char* pNameB);

    extern void SPrintf(char* buffer, u32 len, const char* fmt, ...);
    extern void SnPrintf(char* buffer, u32 len, const char* fmt, ...);
    extern void StrCpy(char* dest, u32 len, const char* src);
    extern void StrCat(char* dest, u32 len, const char* src);

}
