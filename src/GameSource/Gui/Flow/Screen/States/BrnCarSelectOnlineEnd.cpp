// ===================================================================================
// BrnGui::CarSelectOnlineEnd -- out-of-line bodies for the online car-select "ready /
// countdown" screen flow state. Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   UpdateLoadResources @0x824967F0, UpdateWFInit @0x824849F0, UpdateRunning @0x824A3408.
//
// NOT BODIED HERE (declared in the header; each needs a collaborator this packet does not
// attest -- left for a later wave):
//   * UpdatePermanent @0x8248FBA8 -- drains the in-queue for load-notify / player-disconnect /
//     advance. Needs CarSelectOnlinePlayerList::HandleLoadNotification (un-recovered) + the
//     player-list's name-prefix field, and writes an un-homed GuiCache far member (+0x4B40, the
//     disconnected-player id slot); those are not groundable here.
//   * HandleLobbyPlayerList @0x824968F0 -- applies a lobby-player-list update. Needs the
//     un-recovered CarSelectOnlinePlayerList mutators (Show / Hide / SetPlayerName / SetPlayerCar /
//     SetFinalSelection), the un-homed GuiCache online-player id->index array (+0xAD90, 0x138
//     stride) and GuiCache::GetOnlinePlayerDisconnected (still todo) -- reconstructing those from
//     this call site alone would be invention.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectOnlineEnd.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the 18432 in-queue view
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::PlayAptMovie
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache

namespace BrnGui
{
namespace
{
    // The state's own in-event queue is a VariableEventQueue<18432,16> (mpInGuiEventQueue,
    // reached through the CgsGui::State base -- the same view the overlay states take).
    typedef CgsModule::VariableEventQueue<18432, 16> GuiStateInQueue;

    // In-queue event ids UpdateRunning acts on.
    const s32 KI_EVENT_SET_COUNTDOWN_TIME = 82;    // 0x52  -- payload word is the seconds-left float
    const s32 KI_EVENT_LOBBY_PLAYER_LIST  = 244;   // 0xF4  -- the lobby player-list update blob

    // The screen's apt movie (X360 loads its name from the language-database string table) and its
    // level index.
    const char KAC_SCREEN_MOVIE_NAME[] = "BrnCarSelectOnlineEnd";
    const s32  KI_SCREEN_MOVIE_LEVEL   = 3;

    // The online-host-game state value (GuiCache::GetOnlineHostGameState() == this) that means the
    // local client is the online host and drives the host-choosing UI.
    const s32 KI_ONLINE_HOST_GAME = 1;
}

// ---- CarSelectOnlineEnd (ctor) @ 0x825089C8 -----------------------------------------
// Compiler-emitted construction of the online-car-select-end flow state and its embedded
// GUI sub-objects. The X360 writes the state's own vtable (+0x000, off_8207553C), then a
// long run of embedded sub-object vtables across the object: the countdown clock
// (this+0x40 = off_82073058, mOnlineCountdown), the lobby player-list (this+0x1F8 =
// off_820746E8, mOnlinePlayerList) with its per-player TextField / animator widget slots
// (the repeated off_82072F8C / off_820730FC stores), and the host-choosing clip
// (this+0x1A20 = off_82072F68, mHostChoosingAnimator). Those per-sub-object vtable stores
// belong to widget types not individually recoverable from this ctor; the recovered,
// modelled effect is "construct the CgsGui::State base and the three embedded components",
// which the member initialisation here reproduces (the scalar members mpGuiCache /
// meInternalState / mpHostStatusData / mLastHostCarID take no store in the ctor asm).
// Reconstructed from the X360 asm.
CarSelectOnlineEnd::CarSelectOnlineEnd()
    : CgsGui::State()          // state vtable + base bookkeeping (X360 +0x000)
    , mOnlineCountdown()       // embedded countdown ctor   (X360 this+0x40)
    , mOnlinePlayerList()      // embedded player-list ctor  (X360 this+0x1F8)
    , mHostChoosingAnimator()  // embedded host-choosing clip (X360 this+0x1A20)
{
}

// ---- UpdateLoadResources @ 0x824967F0 -----------------------------------------------
bool CarSelectOnlineEnd::UpdateLoadResources()
{
    CGS_ASSERT(mpGuiCache, "mpGuiCache");   // cpp:195

    if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
        return false;

    // Play the screen's apt movie (X360 stack-builds the GuiEventPlayAptMovie { name, level } and
    // AddEvents it on channel 41 -- that is exactly StateInterface::PlayAptMovie).
    mpStateInterface->PlayAptMovie(KAC_SCREEN_MOVIE_NAME, KI_SCREEN_MOVIE_LEVEL);

    mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mHostChoosingAnimator.GetName());
    return true;
}

// ---- UpdateWFInit @ 0x824849F0 ------------------------------------------------------
bool CarSelectOnlineEnd::UpdateWFInit()
{
    if (!mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        return false;

    mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

    if (mpGuiCache->GetOnlineHostGameState() == KI_ONLINE_HOST_GAME)
        mHostChoosingAnimator.AddOutputAptViewState("apt_Transition", "visible", false);

    return true;
}

// ---- UpdateRunning @ 0x824A3408 -----------------------------------------------------
void CarSelectOnlineEnd::UpdateRunning()
{
    GuiStateInQueue* lpInQueue = reinterpret_cast<GuiStateInQueue*>(mpInGuiEventQueue);

    const CgsModule::Event* lpEvent = NULL;
    s32 liSize = 0;
    s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent != NULL)
    {
        if (liEventId == KI_EVENT_SET_COUNTDOWN_TIME)
        {
            // The event's leading word is the seconds-left value (X360 lfs f1, 0(event)).
            const f32 lfTimeLeft = *reinterpret_cast<const f32*>(lpEvent);
            mOnlineCountdown.SetTimeLeft(lfTimeLeft);
        }
        else if (liEventId == KI_EVENT_LOBBY_PLAYER_LIST)
        {
            HandleLobbyPlayerList(lpEvent);
        }

        liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
    }
}
}
