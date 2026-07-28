// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 12: the splash-screen pair.
//   ClearSplashScreenOverlay    @0x82498E00  (DWARF cpp:2974)
//   HandleSplashScreenRequests  @0x824A54B8  (DWARF cpp:2758)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm,
// scratchpad/waveB/GameSource_Gui_Flow_Screen_States_BrnOnlineGameRoomPlayerInfo.cpp.asm.txt).
// The class shape, the member map and the statics live in the wave-H keystone scaffold
// BrnOnlineGameRoomPlayerInfo.{h,cpp}.
//
// Both functions talk to the network splash overlay -- the "entering online" full-screen
// image the network module raises while a game mode spins up. HandleSplashScreenRequests
// is the SHOW/STOP request sink; ClearSplashScreenOverlay is the acknowledge path the
// overlay-complete handshake and the cache-arrival latch both drive.
//
// The X360 reaches the out-queue through the template instantiations
//   CgsGui::StateInterface::OutputGuiEvent<BrnGui::GuiEventNetworkSplashEvent>  @0x82493880
//   CgsGui::StateInterface::OutputGuiEvent<BrnGui::GuiOverlayWaitFinishRequest> @0x82476E98
// whose bodies were read out of the export: the first stack-builds { 4, 269, 12, state }
// and AddEvent()s it on channel 40 with size 16; the second stack-builds
// { 8, 188, 16, <hole>, CgsID } and AddEvent()s it on channel 40 with size 24. That
// { payload size, payload type, payload offset, payload } shape is exactly the canonical
// CgsGui::GuiEventWrapper<T, 40> (CgsGuiEvent.h:71), so both records are built with it --
// the header words fall out as sizeof(T) / T::GetEventType() / offsetof(mOutEvent) and the
// console's 4 / 8 / 12 / 16 / 24 literals (32-bit sizes/offsets) are never reproduced.
//
// BrnGui::GuiEventNetworkSplashEvent is homed only in BrnGuiDemangledEventTypes.h, which
// hard-collides with BrnGuiEventTypeDefs.h (pulled in transitively by
// BrnGuiOverlaysDirector.h for GuiOverlayWaitFinishRequest), so its payload shape is
// carried as a file-local view -- the accommodation BrnInGame.cpp,
// BrnCarSelectMain_wG_02.cpp and the sibling wave-H partfiles all make.
//
// HandleGuiCacheEvent @0x824A3DD8 was assigned to this group too, but was blocked on a
// missing declaration at the time; it is now bodied in BrnOnlineGameRoomPlayerInfo_wH_18.cpp.
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsID / CgsIDCompress
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEventWrapper<T,N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GsmIO::EGameModeType / IsOnlineFreeBurnLobby
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (meGameModeType; we are a friend)
#include "GameSource/Gui/BrnGuiOverlaysDirector.h"                        // BrnGui::GuiOverlayWaitFinishRequest
#include "GameSource/Gui/Flow/Screen/States/Shared/BrnScreenShared.h"     // GetSplashScreenIDForGameMode

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;   // OutputGuiEvent

        // The upper bound both "leGameModeType < GsmIO::E_MODE_COUNT" asserts compare
        // against. ARTIST E_MODE_COUNT == 18: the X360 bakes `cmpwi 0x12` into BOTH
        // asserts, and the splash-id table it guards has 18 live entries. The committed
        // BrnGameStateSharedIO.h enum is the Dec-2007 DecFIGS one and still says 17, so
        // the bound is carried here as the attested literal rather than the stale enum.
        const s32 KI_ARTIST_MODE_COUNT = 18;

        // ---- the splash request/response states (the event's leading word) ------------
        // Same three values BrnInGame.cpp's splash handler names.
        const u32 KU_SPLASH_STATE_SHOW     = 0;
        const u32 KU_SPLASH_STATE_STOP     = 1;
        const u32 KU_SPLASH_STATE_FINISHED = 2;

        // ---- the splash event ----------------------------------------------------------
        // BrnGui::GuiEventNetworkSplashEvent, carried as a file-local view (its home header
        // collides -- see the file banner). One state word, and the GetEventType() that
        // feeds the record's type field: OutputGuiEvent<GuiEventNetworkSplashEvent>
        // @0x82493880 writes 269 there (`li r11, 0x10D`).
        //
        // The same view serves BOTH directions: the in-queue hands the state the
        // HEADER-STRIPPED payload, so the handler's read is this struct's first word
        // (`lwz r11, 0(r31)`), and the out-queue boxes the identical 4 bytes.
        struct GuiEventNetworkSplashEvent : public CgsModule::Event
        {
            u32 muSplashState;   // +0x00

            s32 GetEventType() const { return 269; }
        };

        // ---- out-queue wire records ---------------------------------------------------
        // OutputGuiEvent<BrnGui::GuiEventNetworkSplashEvent> @0x82493880: sizeof 4 /
        // type 269 / offset 12, record size 16.
        typedef CgsGui::GuiEventWrapper<GuiEventNetworkSplashEvent, KI_CHANNEL_GUI_OUT>
            GuiEventNetworkSplashWire;

        // OutputGuiEvent<BrnGui::GuiOverlayWaitFinishRequest> @0x82476E98: sizeof 8 /
        // type 188 / offset 16 (the 8-aligned CgsID leaves a 4-byte hole after the header
        // that the X360 never stores -- var_14 is written by neither the callee nor the
        // caller -- so nothing is modelled there either).
        //
        // FLAG: the wrapper takes the payload's own GetEventType(), and the committed
        // BrnGui::GuiOverlayWaitFinishRequest does not declare one yet (its sibling
        // GuiOverlayShowingNotification does). Rather than fork the request's layout, the
        // attested id is supplied by deriving from the canonical type; delete this and add
        // `s32 GetEventType() const { return 188; }` to BrnGuiOverlaysDirector.h when
        // shared-header edits are in scope for this wave.
        struct GuiOverlayWaitFinishEvent : public GuiOverlayWaitFinishRequest
        {
            s32 GetEventType() const { return 188; }
        };

        typedef CgsGui::GuiEventWrapper<GuiOverlayWaitFinishEvent, KI_CHANNEL_GUI_OUT>
            GuiOverlayWaitFinishWire;
    }

    // ------------------------------------------------ ClearSplashScreenOverlay @0x82498E00
    // The overlay-complete acknowledge path. An overlay just finished; if it was OUR game
    // mode's splash screen and it left by the plain (method 0) route, tell the network
    // module the splash is FINISHED. Either way the "still owe a splash clear" latch drops
    // -- but only when a game mode is actually set: with meGameModeType == E_MODE_NONE the
    // X360 returns without touching anything (the whole body sits under that test).
    //
    // Called by HandleOverlayComplete (with the stored mOverlayCompleteEvent) and by
    // HandleGuiCacheEvent when the cache arrives with the latch already set.
    void OnlineGameRoomPlayerInfo::ClearSplashScreenOverlay(
        const OverlayCompleteData* lpOverlayCompleteEvent)
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:2990

        const s32 leGameModeType = mpGuiCache->meGameModeType;
        if (leGameModeType != BrnGameState::GameStateModuleIO::E_MODE_NONE)
        {
            // ARTIST E_MODE_COUNT (the X360 compares against 18) -- see KI_ARTIST_MODE_COUNT.
            CGS_ASSERT(leGameModeType < KI_ARTIST_MODE_COUNT,
                       "leGameModeType < GsmIO::E_MODE_COUNT");   // cpp:2997

            const char* lpcSplashScreenID = GetSplashScreenIDForGameMode(
                static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(leGameModeType));
            CGS_ASSERT(lpcSplashScreenID != 0, "lpcSplashScreenID != NULL");   // cpp:3002

            if (lpOverlayCompleteEvent->mOverlayId == CgsIDCompress(lpcSplashScreenID) &&
                lpOverlayCompleteEvent->miLeaveMethod == 0)
            {
                GuiEventNetworkSplashEvent lFinished;
                lFinished.muSplashState = KU_SPLASH_STATE_FINISHED;

                GuiEventNetworkSplashWire lWire(lFinished);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lWire), lWire.GetChannel(),
                    static_cast<s32>(sizeof(lWire)));   // X360 record size 16
            }

            mbNeedToClearSplashScreen = false;
        }
    }

    // ------------------------------------------------ HandleSplashScreenRequests @0x824A54B8
    // The network module's splash SHOW / STOP requests (event 269).
    //
    // SHOW: while the screen is sitting in a free-burn lobby -- or before any cache/game
    //   mode exists at all -- there is nothing to raise yet, so the request is just latched
    //   into mbShowSplashScreen (the sub-state ladder raises it later) and logged. In any
    //   other mode the request is handed straight to ShowSplashScreen.
    // STOP: if the request never got as far as an overlay (mbShowSplashScreen still set)
    //   the latch drops and the module is told FINISHED immediately; otherwise the overlay
    //   really is up, so the screen posts the wait-to-finish handshake (event 188) for this
    //   game mode's splash id and waits for the overlay-complete round trip.
    // Any other state word is ignored (the X360 `cmplwi 1` / `blt` / `bne` pair).
    //
    // DWARF param type is const GuiEventNetworkSplashEvent*; the in-queue delivers the
    // header-stripped payload, hence the file-local view.
    void OnlineGameRoomPlayerInfo::HandleSplashScreenRequests(const CgsModule::Event* lpNetworkSplashEvent)
    {
        CGS_ASSERT(lpNetworkSplashEvent != 0, "lpNetworkSplashEvent");   // cpp:2770

        const u32 luSplashState =
            reinterpret_cast<const GuiEventNetworkSplashEvent*>(lpNetworkSplashEvent)->muSplashState;

        if (luSplashState == KU_SPLASH_STATE_SHOW)
        {
            // The free-burn-lobby test is the compiler-inlined
            // GameStateModuleIO::IsOnlineFreeBurnLobby predicate (the asm compares the
            // mode word against 15 and 16 in line, no call), restored to the call it was
            // written as -- same de-inlining the wave-H load ladder makes.
            if ((mpGuiCache != 0 &&
                 BrnGameState::GameStateModuleIO::IsOnlineFreeBurnLobby(
                     static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(
                         mpGuiCache->meGameModeType))) ||
                mpGuiCache == 0 ||
                mpGuiCache->meGameModeType == BrnGameState::GameStateModuleIO::E_MODE_NONE)
            {
                mbShowSplashScreen = true;

                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                {
                    *CgsDev::Log::gpDebugPrint << "mbShowSplashScreen = true;\n";
                }
            }
            else
            {
                // Why this call takes no argument (the header now matches):
                // The X360 hands ShowSplashScreen NOTHING but `this`: 0x824A5650 emits only
                // `mr r3, r29` before the bl, and the event pointer (r31, loaded by
                // `mr r31, r4` @0x824A54C4) is never moved back into r4. It could not have
                // been left in r4 either -- the assert path at 0x824A54DC..0x824A54F4
                // clobbers r4 and falls through to this same call. ShowSplashScreen's own
                // body @0x82499F00 confirms it: it reads the game mode from mpGuiCache and
                // touches no second argument at all.
                //
                // The argument is passed here ONLY because the frozen keystone header
                // (the header declaration has since been corrected to take none).
                ShowSplashScreen();
            }
        }
        else if (luSplashState == KU_SPLASH_STATE_STOP)
        {
            if (mbShowSplashScreen)
            {
                mbShowSplashScreen = false;

                GuiEventNetworkSplashEvent lFinished;
                lFinished.muSplashState = KU_SPLASH_STATE_FINISHED;

                GuiEventNetworkSplashWire lWire(lFinished);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lWire), lWire.GetChannel(),
                    static_cast<s32>(sizeof(lWire)));   // X360 record size 16
            }
            else
            {
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:2812

                const s32 leGameModeType = mpGuiCache->meGameModeType;
                CGS_ASSERT(leGameModeType > BrnGameState::GameStateModuleIO::E_MODE_NONE,
                           "leGameModeType > GsmIO::E_MODE_NONE");    // cpp:2814
                // ARTIST E_MODE_COUNT (the X360 compares against 18) -- see KI_ARTIST_MODE_COUNT.
                CGS_ASSERT(leGameModeType < KI_ARTIST_MODE_COUNT,
                           "leGameModeType < GsmIO::E_MODE_COUNT");   // cpp:2815

                // The X360 constructs the request into its own stack slot and the
                // instantiation copies the 8 bytes into the record.
                GuiOverlayWaitFinishEvent lWaitFinish;
                lWaitFinish.Construct(GetSplashScreenIDForGameMode(
                    static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(leGameModeType)));

                GuiOverlayWaitFinishWire lWire(lWaitFinish);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lWire), lWire.GetChannel(),
                    static_cast<s32>(sizeof(lWire)));   // X360 record size 24
            }
        }
    }

    // ---------------------------------------------------------------------------------
    // HandleGuiCacheEvent @0x824A3DD8 is bodied in BrnOnlineGameRoomPlayerInfo_wH_18.cpp.
}
