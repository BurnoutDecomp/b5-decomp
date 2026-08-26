// =================================================================================================
// GameSource/Gui/Flow/HUD/Components/BrnFriendsListLinkGates.cpp
//
// ⛔ LINK SCAFFOLD -- NOT A RECONSTRUCTION. Every body in this file is a stand-in.
//
// WHY IT EXISTS (measured 2026-08-26, aimodule wave). `origin/dev` at c0dc4af2 does not LINK.
// The friends-list tranches (3f63dc20 .. c0dc4af2) and the BoostMessageManager landing
// (e9c4cfe9) added real call sites for symbols that have NO DEFINITION ANYWHERE IN THE TREE,
// so `build exe` ended in `LNK1120: 15 unresolved externals` -- on a tree whose last commit
// reports success. That is the [[shadowing-redeclarations]] lesson in its plainest form:
// ⭐⭐ ONLY A LINK FINDS THIS, and the author of those commits does not hold build rights.
// Verified pre-existing: the same 15 unresolveds reproduce at a clean c0dc4af2 with this
// wave's working tree stashed.
//
// Two of the fifteen were not missing at all -- BrnBoostMessageManager.cpp and
// BrnFriendsListEntry.cpp were on disk but not in the exe source list; those are MOUNTED
// (tools/build/build_game_exe.bat), not gated. The seven below are the genuine holes.
//
// ⚠️ EVERY ONE OF THESE IS COLD ON TODAY'S BOOT PATH: the friends list is an online-HUD
// component the junkyard->driving flow never enters, and the challenge list is only reached
// through it here. They are gated rather than guessed for exactly that reason -- there is no
// live behaviour to preserve, only a link to close. Each logs once so an absence downstream
// is never scored as a silent success ([[silent-drop-stubs]]).
//
// DELETE-WHEN the friends-list author lands the bodies (GuiCache's two far-member accessors,
// FriendsListComponent::UpdateAllFriendsEntryData @0x8242B498, FriendsListEntry::Select, the
// two ChallengeList/ChallengeListEntry accessors) and the DirtySock lobby TUs mount.
// =================================================================================================


#include "GameSource/Gui/Flow/HUD/Components/BrnFriendsList.h"
#include "GameSource/Gui/Flow/HUD/Components/BrnFriendsListEntry.h"
#include "GameSource/Gui/BrnGuiCache.h"
#include "SharedClasses/DataLists/ChallengeList.h"
#include "SharedClasses/DataLists/ChallengeListEntry.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

namespace
{
    void LogGateOnce(bool& lrbLogged, const char* lpacSymbol)
    {
        if (lrbLogged)
        {
            return;
        }
        lrbLogged = true;
        if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[friends-link-gate] " << lpacSymbol
                << ": inert stand-in, no body anywhere in the tree [FLAG link scaffold]\n";
        }
    }
}

namespace BrnGui
{
    // X360 FriendsListEntry::Select -- declared virtual at BrnFriendsListEntry.h:146, never
    // defined. BrnHudFlow.obj needs it for the class vtable, so the class cannot be
    // instantiated without it. [[hollow-shell-classes]]: a base default that does nothing.
    void FriendsListEntry::Select()
    {
        static bool sbLogged = false;
        LogGateOnce(sbLogged, "BrnGui::FriendsListEntry::Select");
    }

    // X360 0x8242B498. Called from three sites inside BrnFriendsList.cpp itself
    // (ShowSpecificFriend and the two refresh arms) and declared at BrnFriendsList.h:112.
    void FriendsListComponent::UpdateAllFriendsEntryData()
    {
        static bool sbLogged = false;
        LogGateOnce(sbLogged, "BrnGui::FriendsListComponent::UpdateAllFriendsEntryData");
    }

    // GuiCache far member -- X360 CheckPrivileges @0x82485980 calls the real out-of-line method.
    // ⚠️ RETURNS false, WHICH IS THE FAIL-CLOSED ANSWER: every caller uses it to decide whether
    // to OFFER an online affordance, so false hides a menu entry rather than entering an
    // unreconstructed online path. (BrnInGame.cpp carries its own file-local stand-in for the
    // same concept -- CacheIsMultiplayerAllowed -- which answers `cache != 0`; the two disagree
    // deliberately, and unifying them is the author's call, not this scaffold's.)
    bool GuiCache::IsMultiplayerAllowed() const
    {
        static bool sbLogged = false;
        LogGateOnce(sbLogged, "BrnGui::GuiCache::IsMultiplayerAllowed");
        return false;
    }

    // GuiCache far byte @0x13B9A, read by BuildShortcutOptions @0x824145B0. Fail-closed: false
    // suppresses the option-1 shortcut entry rather than publishing an unbacked one.
    bool GuiCache::GetOfflineShortcutProgressGate() const
    {
        static bool sbLogged = false;
        LogGateOnce(sbLogged, "BrnGui::GuiCache::GetOfflineShortcutProgressGate");
        return false;
    }
}

namespace BrnResource
{
    // ChallengeList far accessor @0x8242B894 (declared-only, ChallengeList.h:95).
    // ⛔ RETURNS 0, WHICH IS A [[silent-drop-stub]] BY CONSTRUCTION: every caller loops
    // `for (i = 0; i < GetChallengeCount(); ++i)`, so zero means "no challenges exist" and the
    // loop body never runs. That is harmless ONLY while the sole caller is the cold friends
    // list. ⚠️ BrnChallengeManager_wB_03.cpp and BrnChallengeManagerDebugComponent.cpp call it
    // too and are NOT mounted today -- mounting either of them WITHOUT landing the real body
    // would make the challenge system quietly empty. Land the body with that mount.
    s32 ChallengeList::GetChallengeCount() const
    {
        static bool sbLogged = false;
        LogGateOnce(sbLogged, "BrnResource::ChallengeList::GetChallengeCount");
        return 0;
    }

    // ChallengeListEntry far byte +0xD3 (declared-only, ChallengeListEntry.h:375). Unreachable
    // today: its only caller sits inside the GetChallengeCount()==0 loop above.
    int32_t ChallengeListEntry::GetNumPlayers() const
    {
        static bool sbLogged = false;
        LogGateOnce(sbLogged, "BrnResource::ChallengeListEntry::GetNumPlayers");
        return 0;
    }
}

// RETIRED 2026-08-26 (same day, other session): the std::strcmp LobbyNameCmp STAND-IN that
// lived here is gone -- the REAL EA DirtySock body landed in its canonical vendor home,
// vendor/dirtysdk/src/lobbyname.cpp (@0x82B10050, the 128-byte translation table dumped from
// the image @0x82146038: case-insensitive, skips controls/space/DEL -- exactly the semantics
// this banner predicted strcmp would disagree on). Two definitions would be LNK2005; the
// vendor TU is mounted, so the stand-in dies per its own DELETE-WHEN.
