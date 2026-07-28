// ===================================================================================
// BrnGui::CarSelectMain -- wave-G partfile 01: the enter/leave/accept lifecycle plus the
// class's four static tables.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   OnEnter          @ 0x824C8920
//   OnLeave          @ 0x824C8B78
//   ExitCarSelection @ 0x824C8CB8
//
// The statics were read out of the decrypted image (headless IDA dump of the .data
// tables the three referencing bodies address):
//   maiEventToObserve[21]        @ 0x82065E80   (21 event ids, table order)
//   miNumEventsObserved          @ 0x82065ED4   (== 21, immediately after the table)
//   KPC_LAUNCH_FAILED_STRINGIDS  @ 0x82F26C94   (7 char* -- consumed by HandleLaunchedEvent)
//   KPAC_MODE_STRINGS            @ 0x82F26CB0   (17 char*, EGameModeType-indexed --
//                                                consumed by HandleLaunchingEvent)
//
// The sibling bodies live in BrnCarSelectMain.cpp and the other wave-G partfiles.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectMain.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                      // CgsGui::GuiEvent<N> / GuiEventQueueLarge
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                     // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // StateInterface (out-queue / PlayAptMovie)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                   // GsmIO::ECarSelectType
#include "GameSource/Gui/BrnGuiCache.h"                                  // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // BrnGui::GuiFlow (E_GUIFLOW_SCREEN)

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------
        const s32 KI_CHANNEL_GUI_OUT      = 40;   // GuiEventOut
        const s32 KI_CHANNEL_GUI_INTERNAL = 42;   // internal/HUD-component channel

        // ---- out-queue wire records (the GuiEventWrapper shape: { payload size, event
        //      type, payload offset } then the payload; same idiom as BrnInGame.cpp's
        //      anon namespace) --------------------------------------------------------

        // 16-byte command record { 1, N, 12, flag }. The payload is a single byte, so the
        // X360 size word is 1 and the record pads out to 16.
        template <s32 N>
        struct GuiCommandEvent16 : public CgsGui::GuiEvent<N>
        {
            u8 mu8Flag;    // +0x0C
            u8 maPad[3];   // +0x0D (the X360 never writes these; modelled zeroed)

            explicit GuiCommandEvent16(u8 lu8Flag = 0)
                : CgsGui::GuiEvent<N>(1, 12), mu8Flag(lu8Flag)
            {
                maPad[0] = maPad[1] = maPad[2] = 0;
            }
        };

        template <s32 N>
        void PostCommand16(CgsGui::StateInterface* lpStateInterface, s32 liChannel,
                           u8 lu8Flag = 0)
        {
            GuiCommandEvent16<N> lEvent(lu8Flag);
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), liChannel,
                static_cast<s32>(sizeof(lEvent)));   // X360 record size 16
        }

        // The junkyard "activate car select" record (@0x824C8D38): { 8, 192, 12, 4, 1 },
        // channel 40, 20 bytes. BrnGuiDemangledEventTypes.h attests the id-192 payload as
        // 8 bytes; the two payload words' roles are not recovered, so they stay generic.
        struct GuiEventActivateCarSelect20 : public CgsGui::GuiEvent<192>
        {
            u32 muWord0;   // +0x0C (X360 stores 4)
            u32 muWord1;   // +0x10 (X360 stores 1)

            GuiEventActivateCarSelect20(u32 luWord0, u32 luWord1)
                : CgsGui::GuiEvent<192>(8, 12), muWord0(luWord0), muWord1(luWord1) {}
        };

        // The always-posted accept record (@0x824C8D78): { 2, 536, 12, u8 1, u8 0 },
        // channel 40, 16 bytes. The X360 builds the two payload bytes separately and
        // pushes them out as one halfword -- modelled as the two bytes they are, NOT as a
        // 16-bit word (that value only reads back as 256 because the console is
        // big-endian). The trailing two bytes are never written by the X360.
        struct GuiEventCarSelectAccept16 : public CgsGui::GuiEvent<536>
        {
            u8 mu8Byte0;   // +0x0C (X360 stores 1)
            u8 mu8Byte1;   // +0x0D (X360 stores 0)
            u8 maPad[2];   // +0x0E (untouched by the X360; modelled zeroed)

            GuiEventCarSelectAccept16(u8 lu8Byte0, u8 lu8Byte1)
                : CgsGui::GuiEvent<536>(2, 12), mu8Byte0(lu8Byte0), mu8Byte1(lu8Byte1)
            {
                maPad[0] = maPad[1] = 0;
            }
        };
    }

    // ================================================================================
    // statics -- DWARF h:254/h:255 + cpp:79/cpp:90; values read from the X360 image.
    // ================================================================================

    // @0x82065E80 -- the events OnEnter registers for / OnLeave unregisters, in table
    // order: controller (6/7/8/5), online launching/launched (57/58), disconnect (44),
    // gui cache (64), car info (412), player info (406), car audio load (564/565/566),
    // unlocked livery (413), 81, overlay complete (189), left game (273), 83, 84, 271,
    // enter game (93).
    const s32 CarSelectMain::maiEventToObserve[21] =
    {
          6,   7,   8,   5,  57,  58,  44,  64, 412, 406, 564,
        565, 566, 413,  81, 189, 273,  83,  84, 271,  93,
    };

    // @0x82065ED4 -- sits immediately after the table.
    const s32 CarSelectMain::miNumEventsObserved = 21;

    // @0x82F26C94 -- launched-result -> overlay message string id. Entry 0 is the pooled
    // empty string (@0x820046A7); results 1..6 are the failure reasons HandleLaunchedEvent
    // feeds to the "CNOnlLchFail" overlay as message param 2.
    const char* const CarSelectMain::KPC_LAUNCH_FAILED_STRINGIDS[7] =
    {
        "",
        "ONLINE_LAUNCHED_FAILED_PLAYERS",
        "ONLINE_LAUNCHED_FAILED_CONNECTING",
        "ONLINE_LAUNCHED_FAILED_ROUTES",
        "ONLINE_LAUNCHED_FAILED_TEAMS",
        "ONLINE_LAUNCHED_FAILED_PLAYING",
        "ONLINE_LAUNCHED_FAILED_GENERAL",
    };

    // @0x82F26CB0 -- EGameModeType-indexed mode string ids (17 entries, matching
    // GsmIO::E_MODE_COUNT). Only the online modes 10..14 are populated in the image; the
    // offline slots 0..9 and the online free-burn-lobby / showtime slots 15..16 are null.
    // Consumed by HandleLaunchingEvent (out of this partfile's scope).
    // The table is EIGHTEEN slots, not seventeen. Re-dumped big-endian from the image:
    // [17] @0x82F26CF4 = 0x8205CBEC -> "ONLINE_GAME_OPTION_MODE_STUNT_COOP" is a LIVE
    // entry, and the table ends at [18] @0x82F26CF8 = 0x00000095, which is not a pointer.
    // HandleLaunchingEvent @0x824C9008 indexes this by EGameModeType, so declaring it [17]
    // lost a real mode string with no diagnostic.
    const char* const CarSelectMain::KPAC_MODE_STRINGS[18] =
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                     // 0..9   offline modes
        "ONLINE_GAME_OPTION_MODE_RACE",                   // 10  E_MODE_ONLINE_RACE
        "ONLINE_GAME_OPTION_MODE_ROAD_RAGE",              // 11  E_MODE_ONLINE_ROAD_RAGE
        "ONLINE_GAME_OPTION_MODE_STUNT",                  // 12  E_MODE_ONLINE_FUGITIVE
        "ONLINE_GAME_OPTION_MODE_BURNING_HOME_RUN",       // 13  E_MODE_ONLINE_BURNING_HOME_RUN
        "ONLINE_GAME_OPTION_MODE_STUNT_FREE_FOR_ALL",     // 14  E_MODE_ONLINE_FREE_BURN
        0,                                                // 15  E_MODE_ONLINE_FREE_BURN_LOBBY
        0,                                                // 16  E_MODE_ONLINE_SHOWTIME
        "ONLINE_GAME_OPTION_MODE_STUNT_COOP",             // 17  (0x8205CBEC)
    };

    // ---- OnEnter @ 0x824C8920 ------------------------------------------------------
    // Register the observed events, shut the HUD components down (command 148 on the
    // internal channel), clear the pending selection, latch the GuiCache, post the
    // car-select-entered command (405), Construct the five apt components, and latch
    // which car-select flow is running.
    void CarSelectMain::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // { 1, 148, 12, 0 } on channel 42: take the HUD components down.
        PostCommand16<148>(mpStateInterface, KI_CHANNEL_GUI_INTERNAL, 0);

        miMostRecentDropInId = 0;

        // The X360 inlines GetAccessPointers() (assert "mpAccessPointers != NULL",
        // CgsGuiStateInterface.h:344) and GetGuiCache() (assert "mpGuiCache",
        // CgsGuiShared.h:201) here; the accessor calls reproduce that assert sequence.
        GuiCache* lpGuiCache = mpStateInterface->GetAccessPointers()->GetGuiCache();

        // Both setup descriptors are cleared to the null car id -- the X360 writes only
        // the leading id qword of each, leaving mbSelectable as Construct left it.
        mCurrentSetupInfo.mCarId = 0;
        mDesiredSetupInfo.mCarId = 0;

        meCurrentState = E_CARSELECT_UNLOADED;
        mpGuiCache     = lpGuiCache;

        // { 1, 405, 12 } on channel 40. The X360 reuses the previous record's payload
        // slot without rewriting the flag byte (it still holds the zero the 148 record
        // put there), so the posted flag is 0.
        PostCommand16<405>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);

        // The five apt components, in the X360's Construct order. All are parented to the
        // movie root (null parent name).
        mManufacturerLogo.Construct("ManufacturerLogo_mc", mpStateInterface, 0);
        mManufacturerName.Construct("CarMan_mc",           mpStateInterface, 0);
        mCarName.Construct         ("CarName_mc",          mpStateInterface, 0);
        mHelpItemContinue.Construct("HelpItemContinue_mc", mpStateInterface, 0);
        mHelpItemBack.Construct    ("HelpItemBack_mc",     mpStateInterface, 0);

        // A SECOND, freshly re-walked access-pointers -> gui-cache chain (the X360 repeats
        // both asserts rather than reusing mpGuiCache), then the car-select-type latch.
        meCarSelectType =
            mpStateInterface->GetAccessPointers()->GetGuiCache()->GetCurrentCarSelectType();
    }

    // ---- OnLeave @ 0x824C8B78 ------------------------------------------------------
    // Unregister, invalidate the load state, clear the level-3 apt movie, drop the
    // expected-component list and post the car-select-left command (533).
    void CarSelectMain::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        meCurrentState = E_CARSELECT_INVALID;

        // The X360 inlines StateInterface::PlayAptMovie (type 18, channel 41, 20 bytes).
        // Its movie name is the pooled empty string @0x820046A7 (verified in the image:
        // the terminating NUL of "%s%s%s"), i.e. "clear level 3".
        mpStateInterface->PlayAptMovie("", 3);

        // cpp:213 -- the streamed-message form of the assert in the X360 image.
        CGS_ASSERT(mpGuiCache != 0, "Invalid GuiCache pointer here");

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        // { 1, 533, 12 } on channel 40. The X360 leaves the payload byte at whatever the
        // apt-movie record left in that stack slot; it is modelled zeroed.
        PostCommand16<533>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);
    }

    // ---- ExitCarSelection @ 0x824C8CB8 ---------------------------------------------
    // Accept the current selection: the junkyard flow additionally posts the
    // activate-car-select event, then everybody posts the accept record and sends the
    // "ACCEPT" state event.
    void CarSelectMain::ExitCarSelection()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:616

        // GetCurrentCarSelectType() carries the "meCarSelectType >
        // GsmIO::E_CAR_SELECT_TYPE_NONE" assert (BrnGuiCache.h:4378) the X360 inlines here.
        if (mpGuiCache->GetCurrentCarSelectType() ==
            BrnGameState::GameStateModuleIO::E_CAR_SELECT_TYPE_JUNKYARD)
        {
            GuiEventActivateCarSelect20 lActivateEvent(4, 1);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lActivateEvent),
                KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lActivateEvent)));   // X360 record size 20
        }

        GuiEventCarSelectAccept16 lAcceptEvent(1, 0);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lAcceptEvent),
            KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lAcceptEvent)));   // X360 record size 16

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "RG :: CSM : SendStateEvent( \"ACCEPT\" ) 2\n";

        SendStateEvent("ACCEPT");
    }
}
