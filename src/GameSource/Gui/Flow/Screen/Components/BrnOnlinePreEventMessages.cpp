#include "BrnOnlinePreEventMessages.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::StateInterface / GuiAccessPointers
#include "GameSource/Gui/BrnGuiCache.h"                             // BrnGui::GuiCache
#include "GameSource/GameState/BrnGameStateSharedIO.h"             // EGameModeType

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGui::OnlinePreEventMessages::SelectScreenKeyFrameForGameMode @ 0x8241A010
//
// Walks the component's state interface -> access pointers -> gui cache to read the
// active game-mode type, picks the apt key-frame name, and pushes it onto the apt
// output view-state. The X360 re-reads / re-asserts the chain at each hop (the
// GetAccessPointers / GetGuiCache asserts are inlined into the caller); reproduced here
// through the by-name accessors with one assert per hop.

namespace BrnGui
{
namespace
{
    // The online modes that use the stunt-run key-frame (EGameModeType 12/14/17).
    bool UsesStuntRunKeyFrame(s32 liGameModeType)
    {
        namespace GM = BrnGameState::GameStateModuleIO;
        return liGameModeType == GM::E_MODE_ONLINE_FUGITIVE   // 12
            || liGameModeType == GM::E_MODE_ONLINE_FREE_BURN  // 14
            || liGameModeType == GM::E_MODE_ONLINE_MODE_END;  // 17
    }
}

int OnlinePreEventMessages::SelectScreenKeyFrameForGameMode()
{
    // Guest: assert the state interface is set (BrnOnlinePreEventMessages.h:116).
    CGS_ASSERT(mpStateInterface != 0, "mpStateInterface");

    // Guest: mpStateInterface->GetAccessPointers() -- asserted non-null both inside the
    // accessor (CgsGuiStateInterface.h:344 "mpAccessPointers != NULL") and at the call
    // site (BrnOnlinePreEventMessages.h:117).
    CgsGui::GuiAccessPointers* lpAccessPointers = mpStateInterface->GetAccessPointers();
    CGS_ASSERT(lpAccessPointers != 0, "mpAccessPointers != NULL");
    CGS_ASSERT(lpAccessPointers != 0, "mpStateInterface->GetAccessPointers()");

    // Guest: ...->GetGuiCache() -- asserted non-null inside the accessor
    // (CgsGuiShared.h:201 "mpGuiCache") and at the call site
    // (BrnOnlinePreEventMessages.h:118).
    BrnGui::GuiCache* lpGuiCache = lpAccessPointers->GetGuiCache();
    CGS_ASSERT(lpGuiCache != 0, "mpGuiCache");
    CGS_ASSERT(lpGuiCache != 0, "mpStateInterface->GetAccessPointers()->GetGuiCache()");

    // Guest: r11 = *(guiCache + 40536) -- the active game-mode type.
    const s32 liGameModeType = lpGuiCache->GetCurrentGameModeType();

    // Guest: v7 == 12 || v7 == 14 || v7 == 17 ? "anim1_StuntRun" : "anim1".
    const char* lpacViewState = UsesStuntRunKeyFrame(liGameModeType) ? "anim1_StuntRun" : "anim1";

    // Guest: CgsGui::GuiComponent::AddOutputAptViewState(this, "apt_state", v8, 0).
    AddOutputAptViewState("apt_state", lpacViewState, false);

    // AddOutputAptViewState is void in the committed base; the X360 r3 result the guest
    // returns is unused by the Show/Hide callers, so return 0.
    return 0;
}
}
