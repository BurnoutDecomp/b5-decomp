// ===================================================================================
// BrnGui::CarSelectVehicle  -- partfile 03: input, selection and the game-state events
//   class:BrnGui::CarSelectVehicle
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (asm + pseudocode):
//   HandleControllerInput      @ 0x824DCD80   (DWARF cpp:572)
//   SetupCar                   @ 0x824D8108   (DWARF cpp:1023)
//   SetTicker                  @ 0x824C9BC8   (DWARF cpp:1059)
//   TriggerSound               @ 0x824CA0F8   (DWARF cpp:1520)
//   HandleCarInfoResponseEvent @ 0x824BEDC0   (DWARF cpp:937)
//   HandleLobbyPlayerList      @ 0x824C9E58   (DWARF cpp:1380)
//
// ⓘ CarSelectMain's dispatcher passes the OBSERVED EVENT ID as HandleControllerInput's
// second parameter, and for events 5..8 that id is the input event KIND, not a controller
// port: 5 == GuiEventControllerInputDown, 6 == ...Pressed, 7 == ...Released,
// 8 == GuiEventControllerAxis (CgsGuiEventTypeDefs.h:81/:88/:95/:102). The committed
// BrnCarSelectMain.h comment calls it "the controller selector"; this body proves it is the
// kind -- it switches on it and reads a DIFFERENT payload shape in each arm.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectVehicle.h"
#include "GameSource/Input/GameInputActions.h"                       // EGameInputActions (the controller action vocabulary)

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // AddEvent
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiWorldDataController.h"                     // BrnGui::WorldDataController
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiAudioTriggerEvent
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GsmIO::ECarSelectType
#include "GameSource/Network/SharedIO/BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h" // LobbyPlayerStatusData (the event-244 rows)
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"      // InGamePlayerStatusData (the gamertag)
#include "SharedClasses/DataLists/VehicleList.h"                          // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"                     // BrnResource::VehicleListEntry

#include <cstring>   // std::strncpy / std::memset

namespace BrnGui
{
    namespace
    {
        const s32 KI_CHANNEL_GUI_OUT = 40;
        const char KAC_EMPTY[] = "";

        // ---- observed input event kinds (CgsGuiEventTypeDefs.h) ---------------------
        const s32 KI_EVENT_CONTROLLER_DOWN     = 5;
        const s32 KI_EVENT_CONTROLLER_PRESSED  = 6;
        const s32 KI_EVENT_CONTROLLER_RELEASED = 7;
        const s32 KI_EVENT_CONTROLLER_AXIS     = 8;

        // ---- the five action ids this screen reacts to ------------------------------
        // EGameInputActions now has a home (GameSource/Input/GameInputActions.h); the names
        // below stay role-named because that is what THIS handler does with them -- the two
        // "_ALT" ids are the dpad family (39/40 GUI_DPAD_LEFT/RIGHT, the DOWN/RELEASED
        // hold-to-scroll arms) and the plain pair are the generic-nav family (43/44 GUI_LEFT/
        // GUI_RIGHT, the PRESSED single-step arms).
        const s32 KI_ACTION_CAROUSEL_PREV_ALT = 39;   // 0x27
        const s32 KI_ACTION_CAROUSEL_NEXT_ALT = 40;   // 0x28
        const s32 KI_ACTION_CAROUSEL_PREV     = 43;   // 0x2B
        const s32 KI_ACTION_CAROUSEL_NEXT     = 44;   // 0x2C
        const s32 KI_ACTION_ACCEPT            = 49;   // 0x31 (EGameInputActions GUI_SELECT)

        // ✅ ROOT CAUSE FIXED ELSEWHERE, COMPENSATING ARM DELETED (input-vocabulary wave,
        // 2026-08-29). `KI_ACTION_ACCEPT_PC = 45` used to sit here because KA_BINDINGS bound
        // the accept key to 45 GUI_START rather than 49 GUI_SELECT. The console body --
        // BrnGui::CarSelectVehicle::HandleControllerInput @0x824DCD80 -- has switch cases
        // { 40, 43, 44 } and one `v11 != 49` test, and NO 45 anywhere, so the arm was pure
        // divergence. KA_BINDINGS now puts Enter/Space/pad-A on 49 and the CONTINUE prompt
        // works through the console's own id.

        // The GuiAudioTriggerEvent action word TriggerSound posts (X360 `li r4, 7`).
        const s32 KI_AUDIO_ACTION_CAROUSEL = 7;

        // The audio trigger record's queued event id. ⓘ The committed
        // BrnGui::GuiAudioTriggerEvent models this SAME 100-byte record under id 201 (its
        // presentation-action producer); this screen posts it under 457. The record the X360
        // builds is { 100, 457, 12, <the 100-byte audio record> }, 112 bytes on channel 40.
        const u32 KU_AUDIO_TRIGGER_EVENT_ID_CAROUSEL = 457;

        // ---- the analogue-axis scale (an inline literal in the source; the X360 pools it
        //      at flt_8206B2B8 == 17.647058f). It maps the live axis range
        //      [KF_AXIS_DEAD_ZONE, 1.0] onto [0, 15] pixels of scroll per sample.
        const f32 KF_AXIS_TO_CAROUSEL_SCALE = 17.647058f;

        // ---- in-queue payload views (the queue delivers the HEADER-STRIPPED payload) ----

        // CgsGui::GuiEventControllerInputDown / ...Pressed / ...Released, minus the header.
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (EGameInputActions)
        };

        // CgsGui::GuiEventControllerAxis, minus the header.
        struct ControllerAxisPayload : public CgsModule::Event
        {
            s32 miAxis;       // +0x00 (this screen only reacts to axis 0)
            f32 mfXAxis;      // +0x04
            f32 mfYAxis;      // +0x08
        };

        // Event 412 (GuiCarSelectionEvent). The transport is a
        // CgsContainers::Array<s64,128> (elements @+0x000, count @+0x400 -- see
        // CgsArrayS64_128.cpp) followed by the two 128-bit state arrays and the
        // "cars unlocked" companion counter. Modelled as a local view, the same idiom the
        // sibling screen states use, because the record's canonical home
        // (BrnGuiDemangledEventTypes.h) is mutually exclusive with BrnGuiEventTypeDefs.h.
        struct GuiCarSelectionPayload : public CgsModule::Event
        {
            CgsID maCarIds[128];        // +0x000
            s32   miCount;              // +0x400 (-1 == the array was never Construct/Clear'ed)
            u32   muPad404;             // +0x404
            u64   mau64DrivenBits[2];   // +0x408 (BitArray<128>)
            u64   mau64WreckedBits[2];  // +0x418 (BitArray<128>)
            s32   miNumCarsUnlocked;    // +0x428
        };

        // Event 244 (GuiEventNetworkLobbyPlayerList). The queue delivers the RAW record --
        // AddGuiEvent<GuiEventNetworkLobbyPlayerList> @0x823CF4C8 hands VariableEventQueue::
        // AddEvent the record with its id 244 and length 456 passed out-of-band, so there is
        // no CgsGui::GuiEvent header on this path. The 456 bytes are the eight lobby rows then
        // the live count (X360 `lwz r11, 0x1C0(record)` == 8 * 56). Row fields come from the
        // record's real home, BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h -- no forked
        // layout here. Identical view to the sibling consumers, BrnCarSelectLivery_wJ_01.cpp:31
        // and BrnOnlineGameRoomPlayerInfo_wH_15.cpp:60.
        struct GuiEventNetworkLobbyPlayerListPayload
        {
            BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData
                maPlayers[CarSelectOnlinePlayerList::KI_MAX_PLAYERS];   // +0x000
            s32 miNumPlayers;                                           // X360 +0x1C0 (448)
        };

        // The lobby vehicle-choice mode HandleLobbyPlayerList's host arm runs on
        // (X360 `lwzx r11, mpGuiCache, 0xA9C8 ; cmplwi cr6, r11, 1`): the host picks one car
        // for the whole lobby, so a non-host client never drives the table itself.
        // FLAG consumer-named -- the console has no symbol for the value, only the misspelt
        // "Not a valid vehicle chouce mode : " assert that bounds the word at 1.
        const s32 KI_VEHICLE_CHOICE_HOST_PICKS = 1;

        // ---- out-queue wire records -------------------------------------------------

        // The "activate car select" record HandleLobbyPlayerList's non-host arm posts:
        // { 8, 192, 12, 2, 2 }, channel 40, 20 bytes. Same wire shape (and the same reason for
        // building it by hand rather than through StateInterface::OutputGuiEvent) as
        // BrnCarSelectMain_wG_01.cpp's GuiEventActivateCarSelect20 -- see the call site.
        // The two payload words are (ACTION, TYPE), recovered from the consumer
        // GameStateModule::ProcessGameEvents @0x823A0A18 case 94.
        struct GuiEventActivateCarSelect20 : public CgsGui::GuiEvent<192>
        {
            u32 muWord0;   // +0x0C -- the ACTION
            u32 muWord1;   // +0x10 -- the TYPE (2 == E_CAR_SELECT_TYPE_ONLINE_EVENT_START)

            GuiEventActivateCarSelect20(u32 luWord0, u32 luWord1)
                : CgsGui::GuiEvent<192>(8, 12), muWord0(luWord0), muWord1(luWord1) {}
        };

        // The "clear the ticker" command: { 2, 536, 12, u8 1, u8 0 }, channel 40, 16 bytes.
        struct GuiTickerFlagsWire536 : public CgsGui::GuiEvent<536>
        {
            u8 mbFlagA;   // +0x0C
            u8 mbFlagB;   // +0x0D
            GuiTickerFlagsWire536()
                : CgsGui::GuiEvent<536>(2, 12), mbFlagA(1), mbFlagB(0) {}
        };

        // The custom ticker message payload (0x818 bytes). Layout recovered store-for-store
        // from BrnGui::GuiEventTickerCustomMessage::AddString @0x823A6940, whose asserts bake
        // "GameSource/Gui/BrnGuiEventTypeDefs.h" lines 390/391/392:
        //   +0x000  s32  maiStringTypes[4]            (`stwx r28, mi8NumStrings*4, this`)
        //   +0x010  char maacStrings[4][512]          (`strncpy(this + (n << 9) + 0x10, s, 512)`)
        //   +0x810  s8   mi8NumStrings                (`lbz/extsb`, bounded < 4)
        //   +0x811..+0x814  four flag bytes SetTicker seeds { 0, 0, 1, 0 }
        // Kept TU-LOCAL rather than promoted into BrnGuiEventTypeDefs.h: the type already
        // has an opaque twin in BrnGuiDemangledEventTypes.h (`GuiEvent<537>` + a 2060-byte
        // blob) and the two headers are mutually exclusive by construction, so a second
        // definition would be a live ODR fork.
        struct GuiTickerCustomMessagePayload
        {
            static const s32 KI_MAX_NUM_STRINGS   = 4;     // AddString's bound (h:391)
            static const s32 KI_MAX_STRING_LENGTH = 512;   // AddString's strncpy count

            s32  maiStringTypes[KI_MAX_NUM_STRINGS];                       // +0x000
            char maacStrings[KI_MAX_NUM_STRINGS][KI_MAX_STRING_LENGTH];    // +0x010
            s8   mi8NumStrings;                                            // +0x810
            // FLAG: four flag bytes at +0x811..+0x814 whose roles are not recovered; the only
            // observed producer (SetTicker) seeds them { 0, 0, 1, 0 } before AddString.
            u8   maFlags[4];                                               // +0x811
            u8   maPad815[3];                                              // +0x815 (sizeof == 0x818)

            // @0x823A6940 -- copy lpString into the next free 512-byte slot and record its
            // format type. The count is read as a SIGNED byte (X360 `lbz` + `extsb`).
            void AddString(const char* lpString, s32 liType)
            {
                CGS_ASSERT(mi8NumStrings >= 0, "mi8NumStrings >= 0");                   // h:390
                CGS_ASSERT(mi8NumStrings < KI_MAX_NUM_STRINGS,
                           "mi8NumStrings < KI_MAX_NUM_STRINGS");                       // h:391
                CGS_ASSERT(lpString != 0, "lpString");                                  // h:392

                std::strncpy(maacStrings[mi8NumStrings], lpString,
                             static_cast<size_t>(KI_MAX_STRING_LENGTH));
                maiStringTypes[mi8NumStrings] = liType;
                ++mi8NumStrings;
            }
        };

        // { 0x818, 537, 12, <the message> }, channel 40, 0x824 bytes.
        struct GuiTickerCustomMessageWire : public CgsGui::GuiEvent<537>
        {
            GuiTickerCustomMessagePayload mMessage;   // +0x0C
            GuiTickerCustomMessageWire()
                : CgsGui::GuiEvent<537>(static_cast<u32>(sizeof(GuiTickerCustomMessagePayload)), 12)
            {
                std::memset(&mMessage, 0, sizeof(mMessage));
                mMessage.maFlags[2] = 1;   // the one non-zero seed (+0x813)
            }
        };
    }

    // ================================================================================
    // controller input
    // ================================================================================

    // ---- HandleControllerInput @ 0x824DCD80 ----------------------------------------
    // Every arm ends in UpdateCarouselTransition(), which is what actually steps the
    // highlight once enough travel has accumulated.
    void CarSelectVehicle::HandleControllerInput(const CgsModule::Event* lpEvent, s32 liEventKind)
    {
        CGS_ASSERT(lpEvent != 0, "lpEvent");   // cpp:587

        CarSelectMain::HandleControllerInput(lpEvent, liEventKind);

        switch (liEventKind)
        {
        case KI_EVENT_CONTROLLER_DOWN:
        {
            // A held direction nudges the strip by KC_X_FRAME_CAROUSEL_ADJUST per sample,
            // but only after the press has been seen at least twice (the ref count the
            // PRESSED arm raises), so a single tap does not double-step.
            const ControllerButtonPayload* lpInput =
                reinterpret_cast<const ControllerButtonPayload*>(lpEvent);

            if (lpInput->miButtonId == KI_ACTION_CAROUSEL_PREV_ALT)
            {
                if (muCarouselControllerLeftPressedRefCount > 1u)
                {
                    mfCarouselXOffsetDecay = 0.0f;
                    if (mCarSelector.HighlightPrevious(true))
                        mfCarouselXOffset += KC_X_FRAME_CAROUSEL_ADJUST;
                }
            }
            else if (lpInput->miButtonId == KI_ACTION_CAROUSEL_NEXT_ALT)
            {
                if (muCarouselControllerRightPressedRefCount > 1u)
                {
                    mfCarouselXOffsetDecay = 0.0f;
                    if (mCarSelector.HighlightNext(true))
                        mfCarouselXOffset -= KC_X_FRAME_CAROUSEL_ADJUST;
                }
            }
            break;
        }

        case KI_EVENT_CONTROLLER_PRESSED:
        {
            const ControllerButtonPayload* lpInput =
                reinterpret_cast<const ControllerButtonPayload*>(lpEvent);

            if (lpInput->miButtonId == KI_ACTION_CAROUSEL_PREV)
            {
                // A fresh press while the strip is still coasting the other way is flushed
                // out first, one step at a time.
                if (muCarouselControllerLeftPressedRefCount == 0u && mfCarouselXOffsetDecay != 0.0f)
                {
                    while (!UpdateCarouselTransition())
                        ;
                }

                if (mCarSelector.HighlightPrevious(true))
                {
                    const u32 luRefCount = muCarouselControllerLeftPressedRefCount;
                    mfCarouselXOffsetDecay = 0.0f;
                    if (luRefCount == 0u)
                    {
                        mfCarouselXOffset      = 0.0f;
                        mfCarouselXOffsetDecay = KC_X_FRAME_CAROUSEL_DECAY_ADJUST;
                    }
                    muCarouselControllerLeftPressedRefCount = luRefCount + 1u;
                }

                mCarouselOverviewSelectableGroup.HighlightIndex(2);   // group slot 12
            }
            else if (lpInput->miButtonId == KI_ACTION_CAROUSEL_NEXT)
            {
                if (muCarouselControllerRightPressedRefCount == 0u && mfCarouselXOffsetDecay != 0.0f)
                {
                    while (!UpdateCarouselTransition())
                        ;
                }

                if (mCarSelector.HighlightNext(true))
                {
                    const u32 luRefCount = muCarouselControllerRightPressedRefCount;
                    mfCarouselXOffsetDecay = 0.0f;
                    if (luRefCount == 0u)
                    {
                        mfCarouselXOffset      = 0.0f;
                        mfCarouselXOffsetDecay = -KC_X_FRAME_CAROUSEL_DECAY_ADJUST;
                    }
                    muCarouselControllerRightPressedRefCount = luRefCount + 1u;
                }

                mCarouselOverviewSelectableGroup.HighlightIndex(2);
            }
            else if (lpInput->miButtonId == KI_ACTION_ACCEPT && !IsLoading())
            {
                // ⭐ THE VALIDATE PRESS. IsLoading() is dispatched through this class's own
                // vtable slot +0x2C, so a car change still in flight swallows the press.
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "RG :: CSV : SendStateEvent( \"ADVANCE\" )\n";

                SendStateEvent("ADVANCE");
            }
            break;
        }

        case KI_EVENT_CONTROLLER_RELEASED:
        {
            // A release stops the strip: either it starts a final decay step (when there is
            // travel left to unwind and the highlight can still move) or it snaps to rest.
            const ControllerButtonPayload* lpInput =
                reinterpret_cast<const ControllerButtonPayload*>(lpEvent);

            switch (lpInput->miButtonId)
            {
            case KI_ACTION_CAROUSEL_PREV_ALT:
                if (mfCarouselXOffset != 0.0f && mCarSelector.HighlightPrevious(true))
                    mfCarouselXOffsetDecay = KC_X_FRAME_CAROUSEL_DECAY_ADJUST;
                else
                    mfCarouselXOffsetDecay = 0.0f;
                muCarouselControllerLeftPressedRefCount = 0u;
                break;

            case KI_ACTION_CAROUSEL_NEXT_ALT:
                if (mfCarouselXOffset != 0.0f && mCarSelector.HighlightNext(true))
                    mfCarouselXOffsetDecay = -KC_X_FRAME_CAROUSEL_DECAY_ADJUST;
                else
                    mfCarouselXOffsetDecay = 0.0f;
                muCarouselControllerRightPressedRefCount = 0u;
                break;

            case KI_ACTION_CAROUSEL_PREV:
                // The stick-driven pair only unwind when the axis actually drove them.
                if (!mbControllerAxisActive)
                    break;
                if (mfCarouselXOffset != 0.0f && mCarSelector.HighlightPrevious(true))
                    mfCarouselXOffsetDecay = KC_X_FRAME_CAROUSEL_DECAY_ADJUST;
                else
                    mfCarouselXOffsetDecay = 0.0f;
                muCarouselControllerLeftPressedRefCount = 0u;
                mbControllerAxisActive = false;
                break;

            case KI_ACTION_CAROUSEL_NEXT:
                if (!mbControllerAxisActive)
                    break;
                if (mfCarouselXOffset != 0.0f && mCarSelector.HighlightNext(true))
                    mfCarouselXOffsetDecay = -KC_X_FRAME_CAROUSEL_DECAY_ADJUST;
                else
                    mfCarouselXOffsetDecay = 0.0f;
                muCarouselControllerRightPressedRefCount = 0u;
                mbControllerAxisActive = false;
                break;

            default:
                break;
            }
            break;
        }

        case KI_EVENT_CONTROLLER_AXIS:
        {
            const ControllerAxisPayload* lpAxis =
                reinterpret_cast<const ControllerAxisPayload*>(lpEvent);

            // Only axis 0 (the left stick's X) drives the carousel.
            if (lpAxis->miAxis != 0)
                break;

            if (lpAxis->mfXAxis > KF_AXIS_DEAD_ZONE)
            {
                mbControllerAxisActive = true;
                if (muCarouselControllerRightPressedRefCount > 1u && mCarSelector.HighlightNext(true))
                {
                    mfCarouselXOffsetDecay = 0.0f;
                    mfCarouselXOffset +=
                        (KF_AXIS_DEAD_ZONE - lpAxis->mfXAxis) * KF_AXIS_TO_CAROUSEL_SCALE;
                }
            }
            else if (lpAxis->mfXAxis < -KF_AXIS_DEAD_ZONE)
            {
                mbControllerAxisActive = true;
                if (muCarouselControllerLeftPressedRefCount > 1u && mCarSelector.HighlightPrevious(true))
                {
                    mfCarouselXOffsetDecay = 0.0f;
                    mfCarouselXOffset +=
                        (-lpAxis->mfXAxis - KF_AXIS_DEAD_ZONE) * KF_AXIS_TO_CAROUSEL_SCALE;
                }
            }
            break;
        }

        default:
            break;
        }

        UpdateCarouselTransition();
    }

    // ================================================================================
    // selection
    // ================================================================================

    // ---- SetupCar @ 0x824D8108 -- this class's own virtual (X360 vtable +0x64) -----
    void CarSelectVehicle::SetupCar(const CarSetupInfo* lpSetupInfo, bool lbCommit)
    {
        // cpp:1038 -- streamed form on the console.
        CGS_ASSERT(lpSetupInfo != 0, "Invalid CarSetupInfo structure");

        // Only a real selection commits into mDesiredSetupInfo (and raises the
        // car-change-in-progress gate); a carousel scroll just re-skins the screen.
        if (lbCommit)
            CarSelectMain::SetupCar(lpSetupInfo);

        CarSelectMain::SetupCarNameComponent(lpSetupInfo->mCarId);
        SetupCarsUnlockedTextComponent();
        SetupStatsComponent(lpSetupInfo);
        SetCarouselComponent(lpSetupInfo->mCarId);

        // The console's own re-fetch, RESTORED 2026-08-02 (the substitution that read the
        // latched mpVehicleList is retired with the WorldDataController).
        mManufacturerLogo.Set(mpGuiCache->GetWorldDataController()->GetVehicleList(),
                              lpSetupInfo->mCarId);

        SetTicker(lpSetupInfo->mCarId);
    }

    // ---- SetTicker @ 0x824C9BC8 ----------------------------------------------------
    // Clear whatever the ticker is showing, then post the car's blurb line: the unlocked
    // form for a car the player can pick, the "how to win it" form otherwise.
    void CarSelectVehicle::SetTicker(CgsID lCarId)
    {
        // cpp:1074 -- the console's assert, RESTORED 2026-08-02 (LIST guard retired).
        CGS_ASSERT(mpVehicleList != 0, "mpVehicleList");   // cpp:1074

        const s32 liVehicleIndex = mpVehicleList->GetVehicleIndex(lCarId);
        const BrnResource::VehicleListEntry* lpVehicleData =
            (liVehicleIndex < 0) ? 0 : mpVehicleList->GetVehicleData(liVehicleIndex);
        // cpp:1082 -- the console's assert on the ENTRY. Still suppressed: the id it is handed
        // comes from a stand-in producer (shape (b) in BrnCarSelectVehicle.h).

        {
            GuiTickerFlagsWire536 lTickerClear;
            mpStateInterface->GetOutputEventQueue()->AddEvent(&lTickerClear, KI_CHANNEL_GUI_OUT,
                                                              static_cast<s32>(sizeof(lTickerClear)));
        }

        // ⚠️ ENTRY guard (shape (b) in BrnCarSelectVehicle.h). The console dereferences
        // lpVehicleData unconditionally below.
        if (lpVehicleData == 0)
            return;

        GuiTickerCustomMessageWire lTicker;

        // A livery variant advertises its PARENT car's blurb.
        CgsID lBlurbCarId = lCarId;
        const u8 luLiveryType = lpVehicleData->GetLiveryType();
        if (luLiveryType == 1 || luLiveryType == 3 || luLiveryType == 4)
            lBlurbCarId = lpVehicleData->GetParentId();

        char lacCarId[16];
        CgsIDConvertToString(lBlurbCarId, lacCarId);

        char lacBlurbKey[32];
        CgsCore::SPrintf(lacBlurbKey, 31,
                         IsCarSelectable(lBlurbCarId) ? "CAR_BLURB_%s" : "CAR_BLURB_TO_WIN_%s",
                         lacCarId);
        lacBlurbKey[31] = 0;

        // Format type 2 == CgsLanguage::LanguageManager::E_FORMAT_MINUTES_SECONDS_HUNDREDTHS
        // in the parameter-format enum, but the ticker consumer uses the same word as its own
        // string-kind selector; the X360 literal is 2 and it is passed straight through.
        lTicker.mMessage.AddString(lacBlurbKey, 2);

        mpStateInterface->GetOutputEventQueue()->AddEvent(&lTicker, KI_CHANNEL_GUI_OUT,
                                                          static_cast<s32>(sizeof(lTicker)));
    }

    // ---- TriggerSound @ 0x824CA0F8 -------------------------------------------------
    void CarSelectVehicle::TriggerSound(bool lbClappers)
    {
        GuiAudioTriggerEvent lAudio;
        lAudio.Construct(KI_AUDIO_ACTION_CAROUSEL, KAC_EMPTY,
                         lbClappers ? "CodeCarChoiceCarouselClappers" : "CodeCarChoiceCarousel",
                         KAC_EMPTY);

        // The X360 record is { 100, 457, 12, <the 100-byte audio record> }: same payload as
        // the committed GuiAudioTriggerEvent, different queued id (see
        // KU_AUDIO_TRIGGER_EVENT_ID_CAROUSEL above).
        lAudio.muHeader0   = 100u;
        lAudio.muEventType = KU_AUDIO_TRIGGER_EVENT_ID_CAROUSEL;
        lAudio.muHeader2   = 12u;

        mpStateInterface->GetOutputEventQueue()->AddEvent(&lAudio, KI_CHANNEL_GUI_OUT,
                                                          static_cast<s32>(sizeof(lAudio)));
    }

    // ================================================================================
    // game-state events
    // ================================================================================

    // ---- HandleCarInfoResponseEvent @ 0x824BEDC0 -----------------------------------
    // Event 412 -- the ONLY producer of this screen's car list. The game-state module
    // publishes game action 184 and BrnGameModule::TranslateGameActionsToGuiEvents
    // @0x823EBCA4 turns it into this record.
    void CarSelectVehicle::HandleCarInfoResponseEvent(const CgsModule::Event* lpEvent,
                                                      s32 liEventType)
    {
        // cpp:582 -- the assert's baked file is BrnCarSelectMain.cpp (a copy-paste in the
        // original), so the line number belongs to that file, not this one.
        CGS_ASSERT(lpEvent != 0, "lpEvent");

        if (liEventType != 412)
            return;

        const GuiCarSelectionPayload* lpPayload =
            reinterpret_cast<const GuiCarSelectionPayload*>(lpEvent);

        // The two inlined CgsContainers::Array<s64,128> guards.
        CGS_ASSERT(lpPayload->miCount != -1,
                   "Array used before Construct/Clear was called");   // CgsArray.h:336
        CGS_ASSERT(lpPayload->miCount < KI_MAX_SELECTABLE_CARS,
                   "Too many cars for the selection");                // cpp:959 (streamed form)

        gsiNumCarouselCars = 0;
        for (s32 liCar = 0; liCar < lpPayload->miCount; ++liCar)
        {
            CGS_ASSERT(lpPayload->miCount != -1,
                       "Array used before Construct/Clear was called");   // CgsArray.h:336
            maSelectedCars[gsiNumCarouselCars] = lpPayload->maCarIds[liCar];
            gsiNumCarouselCars = gsiNumCarouselCars + 1;
        }

        // Both 128-bit state arrays are copied wholesale (two `ld`/`std` pairs each).
        std::memcpy(&maSelectedCarsDrivenState, lpPayload->mau64DrivenBits,
                    sizeof(lpPayload->mau64DrivenBits));
        std::memcpy(&maSelectedCarsWreckedState, lpPayload->mau64WreckedBits,
                    sizeof(lpPayload->mau64WreckedBits));

        gsiNumCarsUnlockedTotal = lpPayload->miNumCarsUnlocked;
    }

    // ---- HandleLobbyPlayerList @ 0x824C9E58 ----------------------------------------
    // RECONSTRUCTED 2026-09-07 from this function's OWN X360 export (asm arbitrates); the
    // tripwire that stood here is retired. Both of the things its banner named as missing
    // have landed: CarSelectOnlinePlayerList::Show / Hide / SetPlayerName / SetPlayerCar /
    // SetFinalSelection are bodied in that component's own TU, and the event-244 record is
    // the eight LobbyPlayerStatusData rows + the live count, read through the rows' real
    // home rather than a forked layout.
    //
    // Event 244 is the online lobby roster. The whole body is behind an ONLINE-ONLY entry
    // gate (`lwz 0x288` != 0 and `lwz 0x7C8` == 2), so the offline Junkyard flow -- which
    // observes 244 too -- falls straight out.
    //
    // Past the gate the console splits on the lobby's VEHICLE-CHOICE mode, read out of the
    // GuiCache (`lwzx r11, mpGuiCache, 0xA9C8`, `cmplwi 1`):
    //   * 0  -> straight to the row refresh.
    //   * 1  -> "the host picks the car for everybody": latch the roster's host row once,
    //           and if THIS client is not that row, accept immediately on the client's
    //           behalf instead of showing a table it cannot drive. If it IS the host row,
    //           fall through to the row refresh.
    //   * >1 -> the console's own misspelt assert, then the row refresh anyway.
    //
    // ⓘ NAMING, FLAGGED. The word at cache+0xA9C8 is exposed as GuiCache::GetOnlineHostGameState()
    // (BrnGuiCache.h:381), which is a CONSUMER name minted by the CarSelectOnlineEnd TU; that
    // header already notes the slot is structurally the game-params mirror's meVehicleChoice and
    // marks the naming MERGE RECONCILE PENDING. THIS body's own assert text -- "Not a valid
    // vehicle chouce mode : ", the console's typo -- settles it: the slot is the vehicle-choice
    // mode. The accessor is called by its committed name here so this TU does not fork the cache
    // header; the rename belongs to whoever next owns BrnGuiCache.h.
    void CarSelectVehicle::HandleLobbyPlayerList(const GuiEventNetworkLobbyPlayerList* lpEvent)
    {
        if (mpGuiCache == 0
            || meCarSelectType != BrnGameState::GameStateModuleIO::E_CAR_SELECT_TYPE_ONLINE_EVENT_START)
        {
            return;
        }

        const GuiEventNetworkLobbyPlayerListPayload* lpPlayerList =
            reinterpret_cast<const GuiEventNetworkLobbyPlayerListPayload*>(lpEvent);

        if (mpGuiCache->GetOnlineHostGameState() == KI_VEHICLE_CHOICE_HOST_PICKS)
        {
            // The host row is latched ONCE: the console skips this whole arm while
            // mpHostStatusData is already set (`lwz 0x4124 ; bne -> the row refresh`).
            if (mpHostStatusData == 0)
            {
                for (s32 liPlayer = 0; liPlayer < lpPlayerList->miNumPlayers; ++liPlayer)
                {
                    // X360 `lbz r8, 0(r10)` walking row+0x31 with a 56-byte stride.
                    if (lpPlayerList->maPlayers[liPlayer].mbIsHost)
                    {
                        // ⚠️ The console stores a pointer INTO THE EVENT RECORD
                        // (`mulli r11, r11, 0x38 ; add r11, r11, <event> ; stw r11, 0x4124`),
                        // not a copy of the row -- so mpHostStatusData is only good for as
                        // long as the queued record is. Reproduced, not "fixed": the member's
                        // only reader is the byte test two statements down, in this same call.
                        mpHostStatusData = &lpPlayerList->maPlayers[liPlayer];
                        break;
                    }
                }

                CGS_ASSERT(mpHostStatusData != 0, "mpHostStatusData");   // cpp:1417

                // The console reloads the pointer and reads its mbLocalPlayer byte (row+0x30)
                // straight after that non-gating assert -- the assert is a report, not a gate.
                // [FLAG PC bring-up guard] a roster with no host row leaves the pointer null,
                // and this build's asserts do not stop execution, so the extra `!= 0` term
                // keeps a reported miss from becoming a null deref. It is NOT in the X360 body.
                // It is spelled so a null falls through to the row refresh below rather than
                // firing an ACCEPT nobody asked for -- the smaller of the two divergences.
                // DELETE-WHEN asserts gate.
                if (mpHostStatusData != 0 && !mpHostStatusData->mbLocalPlayer)
                {
                    // The X360 posts this through
                    // StateInterface::OutputGuiEvent<GuiEventActivateCarSelect>, whose console
                    // body wraps the 8-byte payload as { 8, 192, 12, <payload> } and queues 20
                    // bytes on channel 40. The in-tree OutputGuiEvent template does NOT wrap
                    // (see its FLAG in CgsGuiStateInterface.h), so the exact wire record is
                    // built here and posted through the output queue -- the same standing
                    // accommodation CarSelectMain::ExitCarSelection uses for this very id.
                    // The two payload words are (ACTION, TYPE); the console writes 2 into
                    // both (`li r11, 2` stored twice). TYPE 2 is E_CAR_SELECT_TYPE_ONLINE_EVENT;
                    // ACTION 2 is NOT in the recovered action legend (0 start / 1 modify /
                    // 4 exit-junkyard) and the consumer, GameStateModule::ProcessGameEvents
                    // case 94, returns early for every non-junkyard car-select type anyway --
                    // so the literal is carried through as the console's, unnamed.
                    GuiEventActivateCarSelect20 lActivateEvent(2, 2);
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lActivateEvent),
                        KI_CHANNEL_GUI_OUT,
                        static_cast<s32>(sizeof(lActivateEvent)));   // X360 record size 20

                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0
                        && CgsDev::Log::gpDebugPrint != 0)
                    {
                        *CgsDev::Log::gpDebugPrint << "RG :: CSV : SendStateEvent( \"ACCEPT\" )\n";
                    }

                    SendStateEvent("ACCEPT");
                    return;
                }
            }
        }
        else
        {
            // cpp:1444, the console's streamed assert (its own spelling of "choice"). It fires
            // only for a mode ABOVE 1 -- mode 0 branches past it -- and it is non-gating, so
            // the row refresh below runs either way.
            CGS_ASSERT(static_cast<u32>(mpGuiCache->GetOnlineHostGameState())
                           <= static_cast<u32>(KI_VEHICLE_CHOICE_HOST_PICKS),
                       "Not a valid vehicle chouce mode : ");
        }

        // ---- the row refresh (X360 loc_824C9F0C) ------------------------------------
        if (meCurrentState != E_CARSELECT_VISIBLE_INTERACTIVE)
        {
            return;
        }

        // liPlayer is carried OUT of the fill loop into the hide sweep (X360 r31), which is
        // why it is declared here and the sweep has no initialiser. The count is a signed
        // compare and bounds the fill loop on its own, exactly as the console does it.
        s32 liPlayer = 0;
        for (; liPlayer < lpPlayerList->miNumPlayers; ++liPlayer)
        {
            const BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData& lrPlayer =
                lpPlayerList->maPlayers[liPlayer];

            // A row that is not up yet is shown and named once; the name is only ever
            // fetched on that transition.
            if (!mOnlinePlayerList.IsShowing(liPlayer))
            {
                mOnlinePlayerList.Show(liPlayer);

                // The console fetches the record twice around the assert -- that is the assert
                // macro expanding, not two reads -- and derefs it unguarded either way.
                const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo =
                    mpGuiCache->GetOnlinePlayerInfoFromPlayerId(lrPlayer.mPlayerID);

                CGS_ASSERT(lpPlayerInfo != 0,
                           "Trying to show the name of a player who isn't in our game");  // cpp:1464

                // X360 `addi r5, r3, 0x100` -- the record's name field at console +256.
                mOnlinePlayerList.SetPlayerName(liPlayer, lpPlayerInfo->mPlayerName.macName);
            }

            // Driven every refresh, shown or not: the tick and the car can change under a row
            // that is already up. ⚠️ Hex-Rays renders the car fetch as two 32-bit reads
            // (`*(v13-4)`, `*(v13-3)`) and drops the index; the asm is one `ld r5, -0x10(r30)`
            // == row+0x00, the 64-bit CgsID, with the index still in r4.
            mOnlinePlayerList.SetFinalSelection(liPlayer, lrPlayer.mbFinalSelection);  // lbz row+0x32
            mOnlinePlayerList.SetPlayerCar(liPlayer, lrPlayer.mSelectedCarID);         // ld  row+0x00
        }

        // Take down the tail, starting from the first row the roster did not fill. The rows are
        // contiguous, so the sweep stops at the first one already hidden rather than running to
        // the end of the bank. Entered even when the roster is empty (the console's `ble` jumps
        // straight in with the index still zero).
        for (; liPlayer < CarSelectOnlinePlayerList::KI_MAX_PLAYERS
               && mOnlinePlayerList.IsShowing(liPlayer); ++liPlayer)
        {
            mOnlinePlayerList.Hide(liPlayer);
        }
    }
}
