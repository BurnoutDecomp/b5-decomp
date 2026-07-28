// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 18: the GuiCache arrival handler.
//   HandleGuiCacheEvent @0x824A3DD8  (DWARF cpp:912; asserts cpp:924 / cpp:956)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm; see
// scratchpad/waveB/GameSource_Gui_Flow_Screen_States_BrnOnlineGameRoomPlayerInfo.cpp.asm.txt).
// The class shape, the member map and the statics live in the wave-H keystone scaffold
// BrnOnlineGameRoomPlayerInfo.{h,cpp}.
//
// This is the screen's cache-arrival sink: the GUI module broadcasts the GuiCache pointer
// and every screen latches it. Three things hang off that broadcast here.
//
//  1. FIRST arrival only (mpGuiCache still null): latch the cache, drop the two "an online
//     game start is in flight" gates the menu path left set, service a splash-screen clear
//     that was requested before the cache existed, and ask the network module for the
//     current game options so the lobby page can be populated.
//  2. Every arrival while the challenges page is up: re-evaluate the select-button state.
//  3. Every arrival while free-burning: consume the "the online event has finished" flag.
//     For an ONLINE SHOWTIME event that means dropping the sat-nav tracker route and
//     driving the flow on to the post-event screen; for any other mode the flag is just
//     cleared and the screen stays where it is.
//
// This partfile was previously blocked on BrnGui::GuiTracker::ClearTracker (X360
// @0x824FA0A8) having no declaration; it is now declared on the real class in
// GameSource/Gui/SatNav/BrnGuiTracker.h and is called by name here.
//
// The out-queue post is written as a typed record, NOT as the console's baked immediates:
// the payload-size and payload-offset header words are host sizeof expressions and the
// AddEvent size argument is sizeof(the record). The X360's 1 / 12 / 16 literals are the
// 32-bit console record sizes (BrnInGame.cpp + the sibling wave-H partfiles precedent).
//
// The in-queue hands handlers the HEADER-STRIPPED payload, so the incoming cache pointer
// is read through a file-local payload view (the DWARF types the parameter
// const GuiEventCache*, whose home header hard-collides with BrnGuiEventTypeDefs.h -- the
// same accommodation BrnInGame.cpp and the sibling wave-H partfiles make).
//
// MERGE-WINDOW DELTA (rung 1 arbitrates): the Dec-2007 DecFIGS DWARF for this function
// lists GuiCache::SetWaitingForShowtimeResults / ResetStartingGameDueToPlayerJoin /
// SetEnteredOnlineViaEasyDrive calls and has no ClearSplashScreenOverlay, no
// ChallengeListComponent::ShowButton-driven challenges arm tail and no tracker/"TO_ST_POST"
// arm. ARTIST is the target and its asm is what is reconstructed here: two byte stores in
// the first-arrival arm (mbOnlineStartPending, mbIsStartingGameDueToPlayerJoin), no
// showtime-results or EasyDrive store at all.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GsmIO::EGameModeType
#include "GameSource/Gui/BrnChallengeListComponent.h"                     // BrnGui::ChallengeListComponent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (we are a friend)
#include "GameSource/Gui/SatNav/BrnGuiTracker.h"                          // BrnGui::GuiTracker::ClearTracker

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channel (the out-queue selector word) ---------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;   // OutputGuiEvent

        // ---- in-queue payload view -----------------------------------------------------
        // The handler reads a single word at payload +0 and treats it as the GuiCache
        // pointer (`lwz r11, 0(r27)` at 0x824A3DF0, re-read at 0x824A3E8C). DWARF param
        // type is const GuiEventCache*.
        struct GuiEventCachePayload : public CgsModule::Event
        {
            GuiCache* mpGuiCache;   // +0x00
        };

        // ---- out-queue wire record -----------------------------------------------------
        // OutputGuiEvent<BrnGui::GuiEventNetworkGameOptionsRequest>: the request carries no
        // fields, so the boxed payload is an empty type and the header's size word is 1.
        // The X360 writes { 1, 260, 12 } and AddEvent()s 16 bytes on channel 40; the
        // payload byte itself is never written (0x824A3ED0..0x824A3EEC).
        //
        // Event id: the asm bakes `li r11, 0x104` == 260. The Dec-2007 DWARF typedefs
        // GuiEventNetworkGameOptionsRequest as GuiEvent<258>; ARTIST is the later build and
        // its literal is what the queue actually carries (the same id drift the committed
        // BrnGuiEventTypeDefs.h records for GuiNewBurnoutSkillzEvent, 526 -> 541).
        struct GuiEventNetworkGameOptionsRequest
        {
        };

        typedef CgsGui::GuiEvent<260> NetworkGameOptionsRequestHeader;

        struct GuiEventNetworkGameOptionsRequestWire : public NetworkGameOptionsRequestHeader
        {
            GuiEventNetworkGameOptionsRequest mRequest;   // +0x0C (never written)

            GuiEventNetworkGameOptionsRequestWire()
                : NetworkGameOptionsRequestHeader(
                      static_cast<u32>(sizeof(GuiEventNetworkGameOptionsRequest)),
                      static_cast<u32>(sizeof(NetworkGameOptionsRequestHeader)))
            {
            }
        };

        // Layout pins for the record above: the console posts a 16-byte record whose size
        // word is 1 and whose payload offset word is 12. All three survive on the host.
        typedef char KAC_ASSERT_REQUEST_PAYLOAD_SIZE[
            sizeof(GuiEventNetworkGameOptionsRequest) == 1 ? 1 : -1];
        typedef char KAC_ASSERT_REQUEST_HEADER_SIZE[
            sizeof(NetworkGameOptionsRequestHeader) == 12 ? 1 : -1];
        typedef char KAC_ASSERT_REQUEST_WIRE_SIZE[
            sizeof(GuiEventNetworkGameOptionsRequestWire) == 16 ? 1 : -1];
    }

    // ------------------------------------------------------ HandleGuiCacheEvent @0x824A3DD8
    void OnlineGameRoomPlayerInfo::HandleGuiCacheEvent(const CgsModule::Event* lpEvent)
    {
        const GuiEventCachePayload* lpCacheEvent =
            reinterpret_cast<const GuiEventCachePayload*>(lpEvent);

        // Non-fatal (BeginAssert / FireAssert / EndAssert, no early-out) -- the X360 falls
        // straight through into the body, so the null cache is latched as-is.
        CGS_ASSERT(lpCacheEvent->mpGuiCache != 0,
                   "Invalid cache in HandleGuiCacheEvent::Update");   // cpp:924

        // ---- first arrival: latch the cache and open the lobby ------------------------
        if (mpGuiCache == 0)
        {
            mpGuiCache = lpCacheEvent->mpGuiCache;

            // The two "we are mid online-game-start" gates the menu path set on the way in.
            mpGuiCache->mbOnlineStartPending            = false;   // GuiCache +0x4B53
            mpGuiCache->mbIsStartingGameDueToPlayerJoin = false;   // GuiCache +0x4B4E

            // A splash clear was requested before the cache arrived; service it now that
            // there is a game mode to resolve the splash id against.
            if (mbNeedToClearSplashScreen)
            {
                ClearSplashScreenOverlay(&mOverlayCompleteEvent);
            }

            GuiEventNetworkGameOptionsRequestWire lGameOptionsRequest;
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lGameOptionsRequest),
                KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lGameOptionsRequest)));
        }

        // ---- challenges page: refresh the select-button state -------------------------
        // The X360 inlines ChallengeListComponent::ShowButton -- the
        // `lbz +0x981 / cmplw / stb +0x981 / stb 1, +0x980` guard at 0x824A3F0C..0x824A3F28
        // IS its body, and the DecFIGS DWARF lists ShowButton among this function's calls.
        if (meSubState == E_SUBSTATE_CHALLENGES)
        {
            mChallengeListComponent.ShowButton(ShouldShowButton());
        }

        // ---- free-burning: consume the "online event finished" flag -------------------
        if (mpGuiCache != 0 && mpGuiCache->mbOnlineEventCompleted &&
            meSubState == E_SUBSTATE_FREE_BURNING)
        {
            if (mpGuiCache->meGameModeType ==
                BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
            {
                // Non-fatal assert; the tracker is dereferenced either way.
                CGS_ASSERT(mpGuiCache->GetGuiTracker() != 0,
                           "mpGuiCache->GetGuiTracker()");   // cpp:956

                mpGuiCache->GetGuiTracker()->ClearTracker();
                SendStateEvent("TO_ST_POST");
            }

            mpGuiCache->mbOnlineEventCompleted = false;   // GuiCache +0x4B5A
        }
    }
}
