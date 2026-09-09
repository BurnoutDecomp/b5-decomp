// =================================================================================================
// GameSource/Gui/Flow/HUD/Components/BrnFriendsListLinkGates.cpp
//
// FLAG link scaffold: inert stand-ins for two friends-list symbols that real call sites need and
// that have no body anywhere in the tree. Both are cold on the offline boot path (the friends
// list is an online-HUD component); each logs once so an absence downstream is never a silent
// success.
//
// DELETE-WHEN the friends-list author lands FriendsListEntry::Select and
// GuiCache::IsMultiplayerAllowed in their own TUs (two definitions would be LNK2005).
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
    // Declared virtual at BrnFriendsListEntry.h:146, defined nowhere. BrnHudFlow.obj needs it
    // for the class vtable. Base default that does nothing.
    void FriendsListEntry::Select()
    {
        static bool sbLogged = false;
        LogGateOnce(sbLogged, "BrnGui::FriendsListEntry::Select");
    }

    // GuiCache far member (decl BrnGuiCache.h:451), defined nowhere. Returns false, the
    // fail-closed answer: callers use it to decide whether to OFFER an online affordance, so
    // false hides a menu entry rather than entering an unreconstructed online path.
    // (BrnInGame.cpp carries its own file-local stand-in, CacheIsMultiplayerAllowed, which
    // answers `cache != 0`; the two disagree deliberately.)
    bool GuiCache::IsMultiplayerAllowed() const
    {
        static bool sbLogged = false;
        LogGateOnce(sbLogged, "BrnGui::GuiCache::IsMultiplayerAllowed");
        return false;
    }

}

