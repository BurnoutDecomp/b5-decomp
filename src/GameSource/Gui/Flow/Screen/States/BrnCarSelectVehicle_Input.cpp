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
        // FLAG: EGameInputActions has no recovered enum home in the tree; these five are
        // named for what THIS handler does with them (the two "_ALT" pair are the ids the
        // DOWN/RELEASED arms use, the plain pair are the ids the PRESSED/RELEASED arms use).
        const s32 KI_ACTION_CAROUSEL_PREV_ALT = 39;   // 0x27
        const s32 KI_ACTION_CAROUSEL_NEXT_ALT = 40;   // 0x28
        const s32 KI_ACTION_CAROUSEL_PREV     = 43;   // 0x2B
        const s32 KI_ACTION_CAROUSEL_NEXT     = 44;   // 0x2C
        const s32 KI_ACTION_ACCEPT            = 49;   // 0x31

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

        // ---- out-queue wire records -------------------------------------------------

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

        // ⚠️ SAME FLAGGED SUBSTITUTION as SetupComponents' badge line: the X360 re-fetches
        // through mpGuiCache->GetWorldDataController()->GetVehicleList(), which is the same
        // pointer CarSelectMain::UpdateGuiCache latched into mpVehicleList.
        if (mpVehicleList != 0)
            mManufacturerLogo.Set(mpVehicleList, lpSetupInfo->mCarId);

        SetTicker(lpSetupInfo->mCarId);
    }

    // ---- SetTicker @ 0x824C9BC8 ----------------------------------------------------
    // Clear whatever the ticker is showing, then post the car's blurb line: the unlocked
    // form for a car the player can pick, the "how to win it" form otherwise.
    void CarSelectVehicle::SetTicker(CgsID lCarId)
    {
        // cpp:1074 -- ⚠️ PC-BUILD GUARD (see BrnCarSelectVehicle.h): the console asserts
        // mpVehicleList and then dereferences it.
        if (mpVehicleList == 0)
            return;

        const s32 liVehicleIndex = mpVehicleList->GetVehicleIndex(lCarId);
        const BrnResource::VehicleListEntry* lpVehicleData =
            (liVehicleIndex < 0) ? 0 : mpVehicleList->GetVehicleData(liVehicleIndex);
        // cpp:1082 -- the console's assert; suppressed for the same reason.

        {
            GuiTickerFlagsWire536 lTickerClear;
            mpStateInterface->GetOutputEventQueue()->AddEvent(&lTickerClear, KI_CHANNEL_GUI_OUT,
                                                              static_cast<s32>(sizeof(lTickerClear)));
        }

        // ⚠️ PC-BUILD GUARD -- see "PC-BUILD GUARDS" in BrnCarSelectVehicle.h. The console
        // dereferences lpVehicleData unconditionally below.
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
    // ⛔ NOT RECONSTRUCTED, and deliberately NOT a silent {}. Event 244 is the ONLINE lobby
    // player table: the X360 body walks the event's player records and drives
    // CarSelectOnlinePlayerList::Show / Hide / SetPlayerName / SetPlayerCar /
    // SetFinalSelection (@0x8241B1C8 / 0x8241B2A0 / 0x82427948 / 0x82434B70 / 0x8241B0C8),
    // none of which has a reconstructed body, and it reads a GuiEventNetworkLobbyPlayerList
    // record with no recovered home. The offline Junkyard flow this wave brings up never
    // observes event 244 (it is only raised by the online lobby), so the assert is a
    // tripwire rather than a live path: if it ever fires, the online car-select lobby table
    // is missing and says so instead of silently showing nothing.
    void CarSelectVehicle::HandleLobbyPlayerList(const GuiEventNetworkLobbyPlayerList* /*lpEvent*/)
    {
        CGS_ASSERT(false,
                   "CarSelectVehicle::HandleLobbyPlayerList (0x824C9E58) is not reconstructed -- "
                   "the online lobby player table is missing. Recover CarSelectOnlinePlayerList's "
                   "Show/Hide/SetPlayerName/SetPlayerCar/SetFinalSelection first.");
    }
}
