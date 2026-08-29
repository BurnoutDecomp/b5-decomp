// ===================================================================================
// BrnGui::CrashNavSettings -- implementation
//   GameSource/Gui/Flow/Screen/States/BrnCrashNavSettings.cpp
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + the
// per-address assembly in .ida-exports/BURNOUT_X360_ARTIST.XEX/):
//   OnEnter                @0x824B7640   OnLeave                    @0x824CCEC8
//   Update                 @0x824DE0D8   HandleControllerInput      @0x824D90E8
//   HandleGuiCacheEvent    @0x824B7980   HandleTriggers             @0x824B78E8
//   ShowMenu               @0x824BCDF8   InputSponsorProductCode    @0x824CCFC0
//   CrashNavProductCodeKeyboardListener::FillString                 @0x824B7A28
//   SteelWheelsSponsorCode @0x824B76B8   OnInputSponsorProductCode  @0x824CD030
// The assert strings and their cpp: line numbers are the image's own.
//
// ⭐⭐ WHY THIS TU EXISTS: IT CLOSES A PLAYER-FACING SOFT-LOCK. See the header banner.
// In one line: the offline pause screen draws LB/RB prompts, RB posts TOGGLE_RIGHT,
// BRNSCREENFSM routes that to CN_SETTINGS, and CN_SETTINGS was a header-only shell whose
// OnEnter/Update/OnLeave were the do-nothing CgsGui::State base -- so the pause screen
// vanished, the sim stayed suspended, and NOTHING could ever send the resume. Measured
// 2026-08-29 on b5 cb80d9b7: zero changed pixels for 200 s after RB.
// THE ARM THAT FIXES IT is HandleControllerInput's `case 45/50` below: SendStateEvent
// ("GO_BACK") followed by the resume pair -- the console's own, in the console's own
// order, which the empty base could never run because the empty base also never
// registered for event 6.
//
// ⛔ PARKED, AND WHY -- read before "finishing" this TU.
// Two of the eleven ledger functions are PARKED on genuinely-unported PC platform leaves,
// not on missing reconstruction:
//   (a) CgsSystem::HardwareSku::GetSku() DOES NOT EXIST ON PC. CgsHardwareSkuPC.cpp
//       defines only FindLanguage, and SharedClasses/DataLists/VehicleList.cpp:135-149
//       already made this call deliberately -- "picking a SKU number for the PC build
//       would be inventing the platform's identity" -- and left every sponsor slot
//       mbAvailable == false as a result. SteelWheelsSponsorCode and
//       OnInputSponsorProductCode branch on GetSku() four times between them; calling it
//       is an LNK2019 (verified absent from build/game/Burnout_PC.map).
//   (b) The on-screen keyboard is X360 XDK. BrnGui::BrnGuiKeyboard::Show @0x824F5B00 has NO
//       DECLARATION in BrnGuiKeyboard.h (which carries only Prepare) and no body, so the
//       call does not even compile; it tail-calls CgsGui::GuiKeyboard::Show @0x8284CFD8 --
//       which IS reconstructed, in CgsGuiKeyboard.cpp, but that TU is NOT MOUNTED and its
//       body calls XShowKeyboardUI, for which there is no PC leaf.
//       Its string prep is parked with it because CgsUnicode::ConvertUtf8ToUtf16
//       @0x82835828 is ALSO body-less: the ledger calls it `reviewed`, and CgsUnicode.cpp
//       (which IS mounted, and does define plenty of other functions) has no definition for
//       THAT one. Found by the link, not by a gate: exactly the ledger-vs-files divergence
//       AGENTS.md warns about.
// ⭐ NEITHER PARK CAN RE-CREATE THE SOFT-LOCK, and that is checked, not assumed: the
// console's 45/50 (GO_BACK + resume) and 54/55 (TOGGLE_*) arms sit OUTSIDE the
// `meState == E_STATE_MAIN` gate in HandleControllerInput, so Start/Back leaves this
// screen from EVERY state, including 3 and 4 (the sponsor-code states). Row 6 therefore
// parks the SCREEN, never the GAME.
// ⭐ Each parked site logs once, so the gap is visible in BrnGame.log rather than being a
// silent drop. RESTORE both verbatim the moment those two leaves land.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavSettings.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                         // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                 // CgsDev::Log::WriteToLog
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                        // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"   // StateInterface, GuiEventNetworkSuspension
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"           // the state in-queue
#include "GameSource/Gui/BrnGuiCache.h"                                    // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                            // GuiEventActivateCrashNav / GuiOverlayRequest / GuiFlow
#include "GameSource/Gui/BrnGuiShared.h"                                   // gGuiResourceIdentifier (the apt movie name)
#include "GameSource/GameState/Progression/BrnProfile.h"                   // BrnProgression::Profile

#include <cstdio>   // std::snprintf (the one-shot parked-leaf logs)

// The XDK sign-in query the profile row gates on (X360 `lwz r3, 0x4B38(cache) ; bl
// XUserGetSigninState`). Declared locally, exactly as the sibling consumers do
// (CgsGuideIntegration.cpp:18, CgsBuddyManagerDirtySockX360.cpp:34); the PC leaf lives
// in GameSource/BrnBaselineLinkStubs.cpp and answers "not signed in".
extern "C" u32 XUserGetSigninState(u32 luUserIndex);

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels -----------------------------------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;   // X360 `li r5, 0x28` -- GuiEventOut

        // ---- observed event ids (.rdata @0x820663A8, five words) -------------------
        const s32 KI_EVENT_CONTROLLER_INPUT   = 6;     // controller action (sub-id @ payload +4)
        const s32 KI_EVENT_APT_TRIGGER        = 21;    // apt-movie trigger
        const s32 KI_EVENT_GUI_CACHE          = 64;    // per-frame cache event (GuiCache* payload)
        const s32 KI_EVENT_KEYBOARD_RESPONSE  = 142;   // 0x8E -- carries the BrnGuiKeyboard*
        const s32 KI_EVENT_493_CONSUMED       = 493;   // 0x1ED -- observed and DELIBERATELY dropped
                                                       //   (the X360 arm is empty: it exists only to
                                                       //   keep the id off the "Unexpected event" log)

        // ---- the ids this screen POSTS (every one read off the asm) ----------------
        const s32 KI_EVENT_KEYBOARD_REQUEST   = 141;   // 0x8D  -- ask for the on-screen keyboard
        const s32 KI_EVENT_SCREEN_CLOSED_533  = 533;   // 0x215 -- the same "screen done" marker
                                                       //   CrashNavMapMain / CrashNavDriverDetails post
                                                       //   on their resume path
        const s32 KI_EVENT_NETWORK_NEWS_TOS   = 266;   // 0x10A -- BrnGui::GuiEventNetworkNewsAndTOS
                                                       //   (BrnGuiDemangledEventTypes.h:650, "id 266
                                                       //   size 4"). That header hard-collides with the
                                                       //   BrnGuiEventTypeDefs.h this TU needs, so the
                                                       //   record is posted directly -- the standing
                                                       //   family accommodation.
        const u32 KU_NEWS_TOS_PAYLOAD_SETTINGS = 6;    // `li r11, 6` at both post sites
        const s32 KI_EVENT_OVERLAY_REQUEST    = 184;   // 0xB8 -- GuiOverlayRequest::GetEventType()
        const s32 KI_EVENT_SPONSOR_CAR_UNLOCK = 78;    // 0x4E -- { 8, 78, 16, <CgsID car> }, 24 bytes

        // ---- controller action ids (the action event's payload word +4) ------------
        // BrnGui::EGameInputActions (DWARF GameSource/Input/GameInputActions.h:24; no
        // committed home yet, so they stay s32 -- the same reason
        // BrnCrashNavColourCalibrate.cpp / BrnOnlineGameOptions_wI_02.cpp give).
        const s32 KI_ACTION_MENU_UP    = 0x29;   // 41 -> MenuComponent vtable +0x38 HighlightPrevious
        const s32 KI_ACTION_MENU_DOWN  = 0x2A;   // 42 -> MenuComponent vtable +0x34 HighlightNext
        const s32 KI_ACTION_START      = 0x2D;   // 45 GUI_START
        const s32 KI_ACTION_SELECT     = 0x31;   // 49 GUI_SELECT
        const s32 KI_ACTION_CANCEL     = 0x32;   // 50 GUI_CANCEL
        const s32 KI_ACTION_TOGGLE_L   = 0x36;   // 54 -> "TOGGLE_LEFT"  (CN_SETTINGS -> CN_D_DETAIL)
        const s32 KI_ACTION_TOGGLE_R   = 0x37;   // 55 -> "TOGGLE_RIGHT" (CN_SETTINGS -> ON_PLAY)

        // The credit-unlock cheat sequence, .rdata @0x820663D8 (nine words, -1 terminated):
        // up, down, left, left, up, down, right, right. HandleControllerInput advances the
        // stage only while the action is in the 41..44 block, and the terminator being
        // reached is what sets Profile::mbHasUnlockedCredits.
        const s32 KI_ACTION_CREDITS_SEQUENCE_FIRST = 41;
        const s32 KI_ACTION_CREDITS_SEQUENCE_LAST  = 44;
        const s32 KI_CREDITS_SEQUENCE_END          = -1;
        const s32 KAI_CREDITS_UNLOCK_SEQUENCE[9] = { 41, 42, 43, 43, 41, 42, 44, 44, -1 };

        // The menu rows, in highlight order. Row 3 (Credits) is the one ShowMenu disables
        // until Profile::mbHasUnlockedCredits is set.
        const s32 KI_MENU_ROW_PROFILE      = 0;
        const s32 KI_MENU_ROW_OPTIONS      = 1;
        const s32 KI_MENU_ROW_TRAX         = 2;
        const s32 KI_MENU_ROW_CREDITS      = 3;
        const s32 KI_MENU_ROW_COLOUR       = 4;
        const s32 KI_MENU_ROW_ACCOUNT_MAN  = 5;
        const s32 KI_MENU_ROW_SPONSOR_CODE = 6;

        // The apt component name every row is built from (X360 aMenuitem_5; DWARF
        // BrnCrashNavSettings.h:130 `const char KAC_MENU_COMPONENT[9]`).
        const char KAC_MENU_COMPONENT[9] = "MenuItem";

        // off_82F26EFC .. off_82F26F18 (.rdata, exactly seven pointers): the localisation
        // keys ShowMenu pushes into the seven rows.
        const char* const KAPC_MENU_TEXT[CrashNavSettings::KI_NUM_MENU_ITEMS] =
        {
            "$SettingMenu_1", "$SettingMenu_2", "$SettingMenu_3", "$SettingMenu_4",
            "$SettingMenu_5", "$SettingMenu_6", "$SettingMenu_7"
        };

        // The apt movie this screen plays, taken from the shared id->name table rather than
        // re-spelled: the X360 loads `off_82F27B0C`, which is `off_82F278E0 + 139*4` ==
        // gGuiResourceIdentifier[139] == "BrnCrashNavSettings"; 139 is also
        // maResourceTuplesToLoad[0].muId below.
        const s32 KI_RESOURCE_ID_SETTINGS = 139;
        const s32 KI_APT_DISPLAY_LEVEL    = 3;     // X360 `li r5, 3`
        const char KAC_EMPTY[]            = "";    // X360 unk_820046A7 (the unload sentinel)

        // X360 `li r8, -1 ; clrldi r8, r8, 32` -- the 32-bit -1 zero-extended into the
        // 64-bit apt-id slot, i.e. Selectable::K_INVALID_ID.
        const u64 KU_INVALID_APT_ID = 0xFFFFFFFFull;

        // The state's in-event queue (CgsGui::State +0x18) is the DWARF
        // InputBuffer::GuiEventQueue, an incomplete alias for the concrete queue
        // instantiation the X360 walks.
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // The event-64 payload view (the queue delivers the header-stripped payload; the
        // member name is the X360 assert's). Same local view BrnCrashNavColourCalibrate.cpp
        // carries -- the type has no committed home yet.
        struct GuiEventCache : public CgsModule::Event
        {
            GuiCache* mpCachePointer;
        };

        // The event-6 payload view: the action sub-id rides in the payload's +4 word
        // (`lwz r9, 4(r4)`).
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (EGameInputActions)
        };

        // The event-142 payload view: the keyboard pointer is the payload's first word
        // (`lwz r11, 0(r27)`), and the X360 asserts it non-NULL at cpp:237.
        struct GuiEventKeyboardResponse : public CgsModule::Event
        {
            BrnGuiKeyboard* mpKeyboard;   // +0x00
        };

        // { 4, 266, 12, <selector> } -- 16 bytes on channel 40. Update posts it through the
        // inlined OutputGuiEvent<GuiEventNetworkNewsAndTOS>; OnLeave stack-builds the same
        // record by hand. Both carry payload 6.
        struct GuiNetworkNewsAndTosWire266 : public CgsGui::GuiEvent<KI_EVENT_NETWORK_NEWS_TOS>
        {
            u32 muSelector;   // +0x0C
            explicit GuiNetworkNewsAndTosWire266(u32 luSelector)
                : CgsGui::GuiEvent<KI_EVENT_NETWORK_NEWS_TOS>(4, 12)
                , muSelector(luSelector)
            {
            }
        };

        // { 288, 184, 16, <GuiOverlayRequest> } -- 304 bytes on channel 40. The X360 builds
        // this record on the stack and memcpy's the 0x120-byte request into it; the header's
        // third word is 16 here, NOT 12, because the payload is 8-aligned (`li r11, 0x10 ;
        // stw r11, +8`). Verified at 0x824CD59C / 0x824CD638.
        struct GuiOverlayRequestWire184 : public CgsGui::GuiEvent<KI_EVENT_OVERLAY_REQUEST>
        {
            GuiOverlayRequest mRequest;   // +0x10
            GuiOverlayRequestWire184()
                : CgsGui::GuiEvent<KI_EVENT_OVERLAY_REQUEST>(
                      static_cast<u32>(sizeof(GuiOverlayRequest)), 16)
            {
            }
        };

        // The one-shot gap log the two PARKED platform sites share. Same shape as
        // BrnScreenStatesLinkStubs.cpp's LogUnreconstructedState: a gap that is invisible at
        // runtime reads exactly like working code, which is the failure this project keeps
        // paying for.
        void LogParkedPlatformLeaf(const char* lpacSite, const char* lpacMissingLeaf)
        {
            char lac[192];
            std::snprintf(lac, sizeof(lac),
                          "[CrashNavSettings] %s PARKED -- no PC leaf for %s (FLAG).\n",
                          lpacSite, lpacMissingLeaf);
            CgsDev::Log::WriteToLog(lac);
        }
    }

    // ---- static data ---------------------------------------------------------------
    // .rdata @0x820663A8 (five words) -- exactly the five ids Update dispatches on.
    const s32 CrashNavSettings::maiEventToObserve[5] =
    {
        KI_EVENT_CONTROLLER_INPUT, KI_EVENT_APT_TRIGGER, KI_EVENT_GUI_CACHE,
        KI_EVENT_KEYBOARD_RESPONSE, KI_EVENT_493_CONSUMED
    };
    const s32 CrashNavSettings::miNumEventsObserved = 5;   // X360 `li r5, 5`

    // ---- CrashNavProductCodeKeyboardListener::FillString @0x824B7A28 ---------------
    // Consume the latched keyboard result. The console clears the "closed" latch even when
    // it returns nothing, so a stale close cannot be read twice.
    char* CrashNavProductCodeKeyboardListener::FillString()
    {
        CGS_ASSERT(mbKeyboardClosed, "mbKeyboardClosed");   // cpp:1202

        const bool lbHadNewData = (mbNewData == 1);         // `lbz r10, 0x19` read BEFORE the clear
        mbKeyboardClosed = false;                           // `stb r11(0), 0x18`

        if (!lbHadNewData)
        {
            return 0;
        }
        return macKeyboardString;                           // `addi r3, r31, 4`
    }

    // ---- OnEnter @0x824B7640 -------------------------------------------------------
    void CrashNavSettings::OnEnter()
    {
        // ⭐⭐ REGISTER FIRST, and note WHY this single line is the whole defect: a state
        // that observes nothing receives nothing, so every arm in Update and
        // HandleControllerInput below is dead code without it. That is exactly how the
        // shell version of this class soft-locked the game -- the same lesson
        // CrashNavMapMain's partial already recorded (BrnScreenStatesLinkStubs.cpp).
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        mMenuComponent.Construct(KAC_MENU_COMPONENT, mpStateInterface, KI_NUM_MENU_ITEMS,
                                 0, KU_INVALID_APT_ID);

        meState                      = E_STATE_LOADING_SCREEN;   // `stw r11, 0x38`
        mpGuiCache                   = 0;                        // `stw r11, 0x1100`
        miCreditsButtonSequenceStage = 0;                        // `stw r11, 0x12A8`
        mbCreditsUnlockedOnEnter     = false;                    // `stb r11, 0x12A4`
    }

    // ---- OnLeave @0x824CCEC8 -------------------------------------------------------
    void CrashNavSettings::OnLeave()
    {
        mMenuComponent.Clear();   // component vtable slot 6 (`lwz r11, 0x18(vtbl)`)

        if (mpGuiCache != 0)
        {
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        }

        // Symmetric with OnEnter. NOT optional bookkeeping: the observer table is 4 slots
        // wide (CgsGui::KI_MAX_OBSERVERS), so registering on every entry without releasing
        // runs it out after four visits to this tab.
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // The apt UNLOAD sentinel: an inlined bare GuiEventPlayAptMovie (GuiEvent<18>(8,12)
        // { name, level }) pushed onto the view-state channel with the EMPTY name and level
        // 3. StateInterface::PlayAptMovie posts exactly that record.
        mpStateInterface->PlayAptMovie(KAC_EMPTY, KI_APT_DISPLAY_LEVEL);

        // Leaving from anything but the settled main state re-arms the news/TOS request.
        if (meState != E_STATE_MAIN)
        {
            GuiNetworkNewsAndTosWire266 lNews(KU_NEWS_TOS_PAYLOAD_SETTINGS);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lNews), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lNews)));
        }
    }

    // ---- HandleGuiCacheEvent @0x824B7980 -------------------------------------------
    // Latch the GuiCache the FIRST time one arrives (the console's guard is on the member,
    // not on the event), and assert the carried pointer.
    void CrashNavSettings::HandleGuiCacheEvent(const CgsModule::Event* lpEvent)
    {
        if (mpGuiCache != 0)
        {
            return;
        }

        const GuiEventCache* lpCacheEvent = reinterpret_cast<const GuiEventCache*>(lpEvent);
        CGS_ASSERT(lpCacheEvent->mpCachePointer != 0,
                   "Invalid cache in CrashNavSettings::HandleGuiCacheEvent");   // cpp:956

        mpGuiCache = lpCacheEvent->mpCachePointer;
    }

    // ---- HandleTriggers @0x824B78E8 ------------------------------------------------
    // ⭐ The X360 body is the NULL assert and NOTHING ELSE -- the apt-trigger arm was never
    // written (its message even names CrashNavMapMain, the console's own copy/paste). Event
    // 21 is registered and dispatched here purely so it does not reach Update's
    // "Unexpected event received" log. Reproduced as-is: adding a handler would be
    // inventing behaviour the binary does not have.
    void CrashNavSettings::HandleTriggers(const CgsModule::Event* lpAptTrigger)
    {
        CGS_ASSERT(lpAptTrigger != 0,
                   "Invalid event in CrashNavMapMain::HandleTriggers");   // cpp:940
    }

    // ---- ShowMenu @0x824BCDF8 ------------------------------------------------------
    // Latch the profile's credits-unlocked flag, build the seven rows, and grey out the
    // Credits row while the flag is clear.
    void CrashNavSettings::ShowMenu()
    {
        // `lwz r11, 0x1100 ; lwz r11, 0x405C(r11) ; lbzx r11, r11, 0x1CD14 ; stb 0x12A4`
        mbCreditsUnlockedOnEnter = mpGuiCache->GetProfile()->HasUnlockedCredits();

        mMenuComponent.SetupMenu(KI_NUM_MENU_ITEMS, true);   // X360 `li r4, 7 ; li r5, 1`

        if (!mbCreditsUnlockedOnEnter)
        {
            mMenuComponent.DisableSelectable(KI_MENU_ROW_CREDITS);
        }

        for (s32 liRow = 0; liRow < KI_NUM_MENU_ITEMS; ++liRow)
        {
            mMenuComponent.SetText(liRow, KAPC_MENU_TEXT[liRow]);
        }
    }

    // ---- InputSponsorProductCode @0x824CCFC0 ---------------------------------------
    // Ask the GUI module for the on-screen keyboard and park the screen until the
    // response (event 142) arrives.
    void CrashNavSettings::InputSponsorProductCode()
    {
        CgsGui::GuiEvent<KI_EVENT_KEYBOARD_REQUEST> lRequest(1, 12);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest), KI_CHANNEL_GUI_OUT, 16);

        mpGuiKeyboard                     = 0;      // `stw r11, 0x1284`
        mKeyboardListener.mbKeyboardClosed = false; // `stb r11, 0x12A0`
        mKeyboardListener.mbNewData        = false; // `stb r11, 0x12A1`
        meState                            = E_STATE_PRODUCT_CODE_INPUT;   // `stw r10(4), 0x38`
    }

    // ---- SteelWheelsSponsorCode @0x824B76B8 ---------------------------- [PARKED] ---
    // ⛔ PARKED on CgsSystem::HardwareSku::GetSku(), which has no PC leaf -- see the file
    // banner (a). The X360 body is fully recovered and is a two-branch validator over the
    // 19-character mask "L##L #L#L LL#L ##L#" (L == A-Z, # == 0-9): SKU 1/2 compare the
    // code against the literal "Z891 4K88 IN25 79AA", SKU 0 runs the mask, SKU >= 3 always
    // fail. Only the leading length compare against the mask is SKU-independent, and on its
    // own it decides nothing -- every path that can return TRUE reads the SKU first.
    // Returning false is the console's own SKU >= 3 answer and is the only outcome this
    // build can reach anyway: the caller below is itself parked.
    bool CrashNavSettings::SteelWheelsSponsorCode(const char* lpacProductCode)
    {
        CGS_ASSERT(lpacProductCode != 0, "NULL match product code string");   // cpp:612

        static bool sbLogged = false;
        if (!sbLogged)
        {
            sbLogged = true;
            LogParkedPlatformLeaf("SteelWheelsSponsorCode",
                                  "CgsSystem::HardwareSku::GetSku");
        }
        return false;
    }

    // ---- OnInputSponsorProductCode @0x824CD030 ------------------------- [PARKED] ---
    // ⛔ PARKED on CgsSystem::HardwareSku::GetSku() -- see the file banner (a). The X360
    // body, fully recovered, is:
    //   1. scan the 12 sponsor codes (off_82F26F18: bestbuy, BZFRICTION, circuitcity,
    //      gamestop, target, metalwheel, tillys, walmart, yodobashi, micromania, CHROME,
    //      CHALLENGE) for one whose VehicleList sponsor slot is mbAvailable and whose text
    //      equals the typed code;
    //   2. then FOUR SKU-gated overrides (SKU 0 rejects "metalwheel"; SKU 1/2 reject every
    //      table hit; SteelWheelsSponsorCode re-adds "metalwheel"; "B179 8M20 XA09 80FF"
    //      re-adds "gamestop" on SKU 1/2; "H211 1Z99 LZ00 00BB" re-adds "micromania" on
    //      SKU 2 with cache language 10);
    //   3. on a hit, resolve the slot's car through WorldDataController::GetVehicleList and
    //      post one of four overlays (CNSpnsrUnlkA already-owned / CNSpnsrUnlk unlocked +
    //      the id-78 unlock event / CNSpnsrUnlkP rank-locked), else CNSpnsrUnlkF.
    // ⭐ STEP 1 CANNOT SUCCEED ON PC EVEN WITH THE SKU: VehicleList::Construct's per-SKU
    // SetSponserVehicleAvailable switch is itself parked on the same missing leaf
    // (VehicleList.cpp:135-149), so every sponsor slot is mbAvailable == false and the
    // scan matches nothing. Steps 2-3 are then all SKU reads. The console's answer for
    // "nothing matched" is the CNSpnsrUnlkF overlay, and that is exactly what is posted
    // here -- the reachable behaviour, not a swallowed call.
    void CrashNavSettings::OnInputSponsorProductCode(const char* lpacProductCode)
    {
        (void)lpacProductCode;   // the console's first act is the 12-entry scan

        static bool sbLogged = false;
        if (!sbLogged)
        {
            sbLogged = true;
            LogParkedPlatformLeaf("OnInputSponsorProductCode",
                                  "CgsSystem::HardwareSku::GetSku (+ the SKU-gated sponsor table)");
        }

        // The console's LABEL_65 "no code matched" arm, verbatim (0x824CD6D8 onward):
        // GuiOverlayRequest::Construct("CNSpnsrUnlkF") -> { 288, 184, 16, request }, ch 40.
        GuiOverlayRequestWire184 lWire;
        lWire.mRequest.Construct("CNSpnsrUnlkF");
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lWire)));
    }

    // ---- HandleControllerInput @0x824D90E8 -----------------------------------------
    // ⭐⭐ THE SOFT-LOCK FIX LIVES IN THE 45/50 ARM. Note the structure the console has and
    // this reproduces: 45/50 (leave + resume) and 54/55 (the tab shuffle) are handled
    // BEFORE the `meState == E_STATE_MAIN` gate, so they work from every internal state --
    // including the two sponsor-code states. There is no reachable state in which this
    // screen cannot be left.
    void CrashNavSettings::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        const ControllerButtonPayload* lpInput =
            reinterpret_cast<const ControllerButtonPayload*>(lpEvent);
        const s32 liAction = lpInput->miButtonId;

        // The credits cheat: only the four d-pad actions advance (or reset) the stage, and
        // reaching the terminator sets the profile flag. `stbx r30, ...` with r30 == 1.
        if (liAction >= KI_ACTION_CREDITS_SEQUENCE_FIRST &&
            liAction <= KI_ACTION_CREDITS_SEQUENCE_LAST)
        {
            const s32 liStage    = miCreditsButtonSequenceStage;
            const s32 liExpected = KAI_CREDITS_UNLOCK_SEQUENCE[liStage];
            if (liExpected != KI_CREDITS_SEQUENCE_END)
            {
                miCreditsButtonSequenceStage = (liAction == liExpected) ? (liStage + 1) : 0;
            }
            if (KAI_CREDITS_UNLOCK_SEQUENCE[miCreditsButtonSequenceStage] == KI_CREDITS_SEQUENCE_END)
            {
                mpGuiCache->GetProfile()->SetHasUnlockedCredits(true);
            }
        }

        switch (liAction)
        {
        case KI_ACTION_START:
        case KI_ACTION_CANCEL:
        {
            // ⭐ THE RESUME, in the console's own order: the state event FIRST, then the
            // two records that restart the simulation, then the "screen done" marker.
            SendStateEvent("GO_BACK");                 // CN_SETTINGS(8) -> INGAME(4)

            CgsGui::GuiEventNetworkSuspension lResume(false);
            mpStateInterface->OutputGuiEvent(lResume);

            GuiEventActivateCrashNav lActivate(true);  // <- THE UNPAUSE
            mpStateInterface->OutputGuiEvent(lActivate);

            CgsGui::GuiEvent<KI_EVENT_SCREEN_CLOSED_533> lDone(1, 12);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lDone), KI_CHANNEL_GUI_OUT, 16);
            break;
        }

        case KI_ACTION_TOGGLE_L:
            SendStateEvent("TOGGLE_LEFT");             // CN_SETTINGS(8) -> CN_D_DETAIL(111)
            break;

        case KI_ACTION_TOGGLE_R:
            SendStateEvent("TOGGLE_RIGHT");            // CN_SETTINGS(8) -> ON_PLAY(12)
            break;

        default:
            if (meState != E_STATE_MAIN)
            {
                break;
            }

            switch (liAction)
            {
            case KI_ACTION_MENU_UP:
                mMenuComponent.HighlightPrevious();    // menu vtable +0x38
                break;

            case KI_ACTION_MENU_DOWN:
                mMenuComponent.HighlightNext();        // menu vtable +0x34
                break;

            case KI_ACTION_SELECT:
                // `lbz r11, 0xE5(r31) ; extsb ; cmplwi 6` -- the compare is UNSIGNED after
                // the sign-extend, so the "no highlight" -1 falls to the default arm.
                switch (mMenuComponent.miHighlightedIndex)
                {
                case KI_MENU_ROW_PROFILE:
                    if (XUserGetSigninState(
                            static_cast<u32>(mpGuiCache->GetActiveControllerIndex())) != 0)
                    {
                        SendStateEvent("TO_PROFILE");
                    }
                    else
                    {
                        GuiOverlayRequest lRequest;
                        lRequest.Construct("PRONoSaveLd");
                        mpStateInterface->OutputGuiEvent(lRequest);
                    }
                    break;

                case KI_MENU_ROW_OPTIONS:
                    SendStateEvent("TO_OPTIONS");
                    break;

                case KI_MENU_ROW_TRAX:
                    SendStateEvent("TO_TRAX");
                    break;

                case KI_MENU_ROW_CREDITS:
                    // The row is disabled while the flag is clear, so this second guard is
                    // the console's belt-and-braces; kept.
                    if (mbCreditsUnlockedOnEnter)
                    {
                        SendStateEvent("TO_CREDITS");
                    }
                    break;

                case KI_MENU_ROW_COLOUR:
                    SendStateEvent("TO_COLOUR");
                    break;

                case KI_MENU_ROW_ACCOUNT_MAN:
                    SendStateEvent("TO_ACCT_MAN");
                    break;

                case KI_MENU_ROW_SPONSOR_CODE:
                    // While the autosave icon is on screen the OSK cannot be raised, so
                    // only remember that the prompt is wanted; Update re-issues it as
                    // soon as the icon clears (`lbzx r11, r11, 0x12F09`).
                    if (mpGuiCache->IsAutosaveIconVisible())
                    {
                        meState = E_STATE_PRODUCT_CODE_INPUT_PENDING;
                    }
                    else
                    {
                        InputSponsorProductCode();
                    }
                    break;

                default:
                    break;
                }
                break;

            default:
                break;
            }
            break;
        }
    }

    // ---- Update @0x824DE0D8 --------------------------------------------------------
    void CrashNavSettings::Update()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liEventId)
            {
            case KI_EVENT_CONTROLLER_INPUT:
                HandleControllerInput(lpEvent);
                break;

            case KI_EVENT_APT_TRIGGER:
                HandleTriggers(lpEvent);
                break;

            case KI_EVENT_GUI_CACHE:
                HandleGuiCacheEvent(lpEvent);
                break;

            case KI_EVENT_KEYBOARD_RESPONSE:
            {
                const GuiEventKeyboardResponse* lpResponse =
                    reinterpret_cast<const GuiEventKeyboardResponse*>(lpEvent);
                CGS_ASSERT(lpResponse->mpKeyboard != 0, "lpKeyboardResponse->lpKeyboard");   // cpp:237

                mpGuiKeyboard = lpResponse->mpKeyboard;   // `stw r11, 0x1284` -- a real store

                // ⛔ PARKED, AS ONE ARM -- see the file banner (b). Everything after the
                // latch above exists only to raise the on-screen keyboard, and the whole
                // tail is missing its links, not its reconstruction:
                //   * BrnGuiKeyboard::Show @0x824F5B00 is neither declared in
                //     BrnGuiKeyboard.h nor defined anywhere; its callee
                //     CgsGui::GuiKeyboard::Show IS reconstructed but sits in the unmounted
                //     CgsGuiKeyboard.cpp and bottoms out in XShowKeyboardUI, which has no PC
                //     leaf;
                //   * CgsUnicode::ConvertUtf8ToUtf16 @0x82835828 -- a real UTF-8 decoder over
                //     the lead-length table at byte_820DE3C8 -- is `reviewed` in the ledger
                //     but has NO BODY (a ledger/file divergence found by this wave's link,
                //     not by any gate).
                // Parking the heading prep WITH the Show it feeds is deliberate: converting
                // a string into a buffer nothing can ever display would look like progress
                // and be none. The console's tail, verbatim, for whoever lands those two:
                //   CgsUnicode::ConvertUtf8ToUtf16(
                //       mpStateInterface->GetLanguageManager()->FindString("SPONSOR_CAR_INPUT_CODE"),
                //       macDialogHeading);                    // dest 0x1204
                //   macDefaultHeading[0] = 0;                 // `sth r20, 0x1104` (UTF-16 NUL)
                //   macTitleHeading[0]   = 0;                 // `sth r20, 0x1184`
                //   mpGuiKeyboard->Show(macDefaultHeading, macTitleHeading,
                //                       macDialogHeading, mKeyboardListener);
                static bool sbLoggedKeyboard = false;
                if (!sbLoggedKeyboard)
                {
                    sbLoggedKeyboard = true;
                    LogParkedPlatformLeaf("Update[keyboard]",
                                          "BrnGuiKeyboard::Show / XShowKeyboardUI "
                                          "(+ CgsUnicode::ConvertUtf8ToUtf16, declared but never bodied)");
                }
                break;
            }

            case KI_EVENT_493_CONSUMED:
                // Observed so it is not "unexpected"; the X360 arm is empty.
                break;

            default:
                // The console's dev-only "Unexpected event received : <id> in <file> at
                // line 262" stream, behind CgsDev::Message::gxMessageFilterFlags & 1.
                break;
            }
        }
        lpInQueue->Clear();

        // Rung 0 -> 1: once the screen's own apt resource is in, hand the menu's expected
        // components to the cache and start the movie.
        if (meState == E_STATE_LOADING_SCREEN && mpGuiCache != 0)
        {
            if (mpGuiCache->EnsureResourcesAreLoaded(maResourceTuplesToLoad,
                                                     static_cast<u32>(miNumResourcesToLoad)))
            {
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                mMenuComponent.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
                mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KI_RESOURCE_ID_SETTINGS],
                                               KI_APT_DISPLAY_LEVEL);
                meState = E_STATE_INITIALISING_COMPONENTS;
            }
        }

        // Rung 1 -> 2: once every expected apt component has initialised, fill the rows and
        // ask the network layer for the news/TOS state.
        if (meState == E_STATE_INITIALISING_COMPONENTS && mpGuiCache != 0)
        {
            if (mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                ShowMenu();
                meState = E_STATE_MAIN;

                GuiNetworkNewsAndTosWire266 lNews(KU_NEWS_TOS_PAYLOAD_SETTINGS);
                mpStateInterface->OutputGuiEvent(lNews);
            }
        }

        mMenuComponent.Update();   // component vtable slot 5 (+0x14)

        // The deferred sponsor prompt: re-issue it as soon as the autosave icon clears.
        if (meState == E_STATE_PRODUCT_CODE_INPUT_PENDING &&
            !mpGuiCache->IsAutosaveIconVisible())
        {
            InputSponsorProductCode();
        }

        // The keyboard closed: consume the result and go back to the main state whether or
        // not anything was typed.
        if (meState == E_STATE_PRODUCT_CODE_INPUT && mKeyboardListener.HasJustClosed())
        {
            char* lpacProductCode = mKeyboardListener.FillString();
            meState = E_STATE_MAIN;
            OnInputSponsorProductCode(lpacProductCode);
        }
    }
}
