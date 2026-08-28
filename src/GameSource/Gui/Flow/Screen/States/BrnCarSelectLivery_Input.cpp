// ===================================================================================
// BrnGui::CarSelectLivery  -- partfile 03: controller input + the game-state events
//   class:BrnGui::CarSelectLivery
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (asm + pseudocode):
//   HandleControllerInput               @ 0x824D6D10  (DWARF cpp:672)
//   HandleCarInfoResponseEvent          @ 0x824BBA10  (DWARF cpp:927)
//   HandlePlayerInfoResponse            @ 0x824C8758  (DWARF cpp:1002)
//   HandleUnlockedLiveryResponseEvent   @ 0x824BBA80  (DWARF cpp:1031)
//   HandlePlayerCarColourResponseEvent  @ 0x824C0E18  (DWARF cpp:1066)
//
// ⭐⭐ THIS FILE CLOSES THE JUNKYARD HANDOVER. HandleControllerInput's case 0x31 is the
// accept: IsLoading() (state vtable +0x2C) gates it, mbExiting latches it once, and
// ExitCarSelection (state vtable +0x5C, CarSelectMain's) posts the {4,1} activate record,
// the accept event and the "ACCEPT" state event that takes BRNSCREENFSM's
// 34 CS_LIVERY -> 4 INGAME edge.
//
// ⓘ CarSelectMain's dispatcher passes the OBSERVED EVENT ID as the second parameter, and for
// events 5..8 that id is the input event KIND, not a controller port (5 Down / 6 Pressed /
// 7 Released / 8 Axis). This screen only reacts to kind 6 -- PRESSED.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectLivery.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // AddEvent
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectVehicle.h"        // the SHARED carousel counter
#include "SharedClasses/DataLists/VehicleList.h"                          // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"                     // BrnResource::VehicleListEntry

namespace BrnGui
{
    namespace
    {
        const s32 KI_CHANNEL_GUI_OUT = 40;

        // ---- observed input event kinds (CgsGuiEventTypeDefs.h) ---------------------
        const s32 KI_EVENT_CONTROLLER_PRESSED = 6;

        // ---- the seven action ids this screen reacts to -----------------------------
        // FLAG: EGameInputActions has no recovered enum home in the tree; the four cursor
        // ids are named for what THIS handler does with them.
        const s32 KI_ACTION_TOGGLE_PREV  = 41;   // 0x29 -- step to the previous toggle ROW
        const s32 KI_ACTION_TOGGLE_NEXT  = 42;   // 0x2A -- step to the next toggle ROW / enter it
        const s32 KI_ACTION_OPTION_PREV  = 43;   // 0x2B -- previous option within the row
        const s32 KI_ACTION_OPTION_NEXT  = 44;   // 0x2C -- next option within the row
        const s32 KI_ACTION_ACCEPT       = 49;   // 0x31 -- ⭐ EGameInputActions GUI_SELECT
        const s32 KI_ACTION_BACK         = 50;   // 0x32 -- GUI_CANCEL
        const s32 KI_ACTION_RESTORE      = 52;   // 0x34 -- restore the factory paint

        // ([input vocabulary repair 2026-08-27] the KI_ACTION_ACCEPT_PC == 45 compensation is
        // RETIRED: CgsInputPadsPC binds the accept key / pad-A to the console's 49 GUI_SELECT.)

        // The PresentationAction words TriggerSound is called with (X360 `li r4, 7 / 8 / 9`).
        const s32 KI_AUDIO_ACTION_SCROLL = 7;
        const s32 KI_AUDIO_ACTION_BACK   = 8;
        const s32 KI_AUDIO_ACTION_SELECT = 9;

        // ---- in-queue payload views (the queue delivers the header-stripped payload) ----

        // CgsGui::GuiEventControllerInputPressed, minus the header.
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (EGameInputActions)
        };

        // Event 406 (GuiPlayerInfoResponse). This screen reads the "car needs repair" BYTE at
        // +0x38 (X360 `lwz r11, 0x38(r4)`); the base's dispatcher has already taken the car id
        // at +0x20 out of the same record.
        struct GuiPlayerInfoResponsePayload : public CgsModule::Event
        {
            u8  maHead[0x38];        // +0x00 (the car id at +0x20 -- taken by the base)
            u8  mbCarNeedsRepair;    // +0x38 -- BYTE: the X360 uses lbz r11, 0x38(r30)
                                     // @0x824C87B0 and stores it back with stbx. On the
                                     // big-endian console an lbz here would read the HIGH
                                     // byte of an s32, so the producer field really is a
                                     // byte. Modelling it as s32 on the little-endian host
                                     // makes != 0 true for any non-zero byte in +0x38..3B.
        };

        // Event 413 (the unlocked-livery list). The transport is a
        // CgsContainers::Array<s64,8>: elements at +0x00, count at +0x40 (see CgsArrayS64_8.cpp).
        struct GuiUnlockedLiveryPayload : public CgsModule::Event
        {
            CgsID maCarIds[8];   // +0x00
            s32   miCount;       // +0x40 (-1 == the array was never Construct/Clear'ed)
        };

        // Event 414 (the player's current car colour). The X360 reads one word at +0x04 and
        // hands it to SetupPaintColourToggle as the colour index.
        struct GuiPlayerCarColourPayload : public CgsModule::Event
        {
            s32 miPaintFinishIndex;   // +0x00
            s32 miColourIndex;        // +0x04
        };

        // ---- out-queue wire records --------------------------------------------------

        // The player-info REQUEST HandlePlayerInfoResponse posts back:
        // { 8, 410, 16, <pad>, CgsID }, channel 40, 24 bytes.
        struct GuiPlayerCarColourRequestWire410 : public CgsGui::GuiEvent<410>
        {
            u32   muPad0C;   // +0x0C
            CgsID mCarId;    // +0x10
            GuiPlayerCarColourRequestWire410()
                : CgsGui::GuiEvent<410>(8, 16), muPad0C(0), mCarId(0) {}
        };
    }

    // ================================================================================
    // controller input
    // ================================================================================

    // ---- HandleControllerInput @ 0x824D6D10 ----------------------------------------
    void CarSelectLivery::HandleControllerInput(const CgsModule::Event* lpEvent, s32 liEventKind)
    {
        CGS_ASSERT(lpEvent != 0, "lpEvent");   // cpp:674

        CarSelectMain::HandleControllerInput(lpEvent, liEventKind);

        // Only PRESSED reaches this screen's own ladder.
        if (liEventKind != KI_EVENT_CONTROLLER_PRESSED)
        {
            return;
        }

        const ControllerButtonPayload* lpInput =
            reinterpret_cast<const ControllerButtonPayload*>(lpEvent);

        switch (lpInput->miButtonId)
        {
        case KI_ACTION_TOGGLE_PREV:
        {
            if (mbDisableCarModifyInteraction)
                break;

            if (mColourMenuToggle.IsHighlighted())
            {
                // Leave the colour picker: hand the highlight back to the toggle rows, with
                // the PAINT FINISH row selected.
                mColourMenuToggle.SetHighlighted(false);                                       // toggle slot 3
                mOptionToggles.SetHighlighted(true);                                           // group slot 3
                mOptionToggles.GetSelectable(E_OPTIONTOGGLE_LIVERY)->SetHighlighted(false);    // row slot 3
                mOptionToggles.GetSelectable(E_OPTIONTOGGLE_PAINT_FINISH)->SetHighlighted(true);
                SendPlayerCarColourEvent();
                TriggerSound(KI_AUDIO_ACTION_BACK);
            }
            else if (muNumUnlockedLiveryCars > 1u)
            {
                // Step up through the toggle rows (only meaningful when the livery row has
                // something to offer).
                if (mOptionToggles.HighlightPrevious(false))   // group slot 11
                {
                    TriggerSound(KI_AUDIO_ACTION_BACK);
                }
            }
            break;
        }

        case KI_ACTION_TOGGLE_NEXT:
        {
            if (mbDisableCarModifyInteraction || IsLoading() || !mOptionToggles.IsHighlighted())
                break;

            if (mOptionToggles.GetSelectable(E_OPTIONTOGGLE_LIVERY)->IsHighlighted())
            {
                // On the LIVERY row: step down onto the PAINT FINISH row, but only when that
                // row is live and this car can actually be painted.
                MenuToggle* lpPaintRow = mOptionToggles.GetSelectable(E_OPTIONTOGGLE_PAINT_FINISH);
                if (lpPaintRow->IsActive())
                {
                    const CgsID lHighlightedId = mCarSelector.GetHighlightedId();
                    const BrnResource::VehicleListEntry* lpVehicleData =
                        mpVehicleList->GetVehicleData(lHighlightedId);
                    if (CanCarBePainted(lpVehicleData))
                    {
                        mOptionToggles.HighlightNext(false);   // group slot 10
                        TriggerSound(KI_AUDIO_ACTION_SELECT);
                    }
                }
            }
            else
            {
                // On the PAINT FINISH row: drop INTO the colour picker.
                mOptionToggles.GetSelectable(E_OPTIONTOGGLE_PAINT_FINISH)->SetHighlighted(false);
                mOptionToggles.SetHighlighted(false);          // group slot 3
                mColourMenuToggle.SetHighlighted(true);        // toggle slot 3
                SendPlayerCarColourEvent();
                TriggerSound(KI_AUDIO_ACTION_SELECT);
            }
            break;
        }

        case KI_ACTION_OPTION_PREV:
        {
            if (mbDisableCarModifyInteraction)
                break;

            if (!mOptionToggles.IsHighlighted())
            {
                // The colour picker owns the cursor.
                if (mColourMenuToggle.HighlightPrevious())
                {
                    TriggerSound(KI_AUDIO_ACTION_SCROLL);
                    SendPlayerCarColourEvent();
                }
                break;
            }

            mOptionToggles.HighlightPreviousItem();   // group slot 14 (+0x38)

            if (mOptionToggles.miHighlightedIndex == E_OPTIONTOGGLE_PAINT_FINISH)
            {
                // A paint-finish change only needs the colour re-published.
                SendPlayerCarColourEvent();
                TriggerSound(KI_AUDIO_ACTION_SCROLL);
            }
            else if (mOptionToggles.miHighlightedIndex == E_OPTIONTOGGLE_LIVERY)
            {
                // A livery change is a CAR change: step the selector and commit it.
                mCarSelector.HighlightPrevious(false);   // selector slot 11

                CarSetupInfo lSetupInfo = mCurrentSetupInfo;
                lSetupInfo.mCarId       = mCarSelector.GetHighlightedId();
                lSetupInfo.mbSelectable = true;
                SetupCar(&lSetupInfo);                   // state vtable +0x58

                UpdateMenuComponents();
                TriggerSound(KI_AUDIO_ACTION_SCROLL);
            }
            break;
        }

        case KI_ACTION_OPTION_NEXT:
        {
            if (mbDisableCarModifyInteraction)
                break;

            if (!mOptionToggles.IsHighlighted())
            {
                if (mColourMenuToggle.HighlightNext())
                {
                    TriggerSound(KI_AUDIO_ACTION_SCROLL);
                    SendPlayerCarColourEvent();
                }
                break;
            }

            mOptionToggles.HighlightNextItem();   // group slot 13 (+0x34)

            if (mOptionToggles.miHighlightedIndex == E_OPTIONTOGGLE_PAINT_FINISH)
            {
                SendPlayerCarColourEvent();
                TriggerSound(KI_AUDIO_ACTION_SCROLL);
            }
            else if (mOptionToggles.miHighlightedIndex == E_OPTIONTOGGLE_LIVERY)
            {
                mCarSelector.HighlightNext(false);       // selector slot 10

                CarSetupInfo lSetupInfo = mCurrentSetupInfo;
                lSetupInfo.mCarId       = mCarSelector.GetHighlightedId();
                lSetupInfo.mbSelectable = true;
                SetupCar(&lSetupInfo);

                UpdateMenuComponents();
                TriggerSound(KI_AUDIO_ACTION_SCROLL);
            }
            break;
        }

        case KI_ACTION_ACCEPT:
        {
            // ⭐⭐ THE VALIDATE PRESS -- the last user action of the Junkyard flow.
            // mbExiting latches it so a second press cannot re-enter ExitCarSelection, and
            // IsLoading() (this class's own vtable slot +0x2C) swallows it while a car change
            // is still in flight. ExitCarSelection is CarSelectMain's, reached through vtable
            // slot +0x5C: it posts GuiEventActivateCarSelect {4,1} (id 192) and
            // GuiEventCarSelectAccept (id 536), then sends the "ACCEPT" state event.
            if (!mbExiting && !IsLoading())
            {
                mbExiting = true;
                ExitCarSelection();
            }
            break;
        }

        case KI_ACTION_BACK:
        {
            // Back is only offered when there is more than one car to go back to (the same
            // shared carousel counter UpdateComponents gates the prompt on).
            if (!mbExiting && CarSelectVehicle::gsiNumCarouselCars > 1 && !IsLoading())
            {
                mbExiting = true;
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                {
                    *CgsDev::Log::gpDebugPrint << "RG :: CSL : SendStateEvent( \"GO_BACK\" )\n";
                }
                SendStateEvent("GO_BACK");
            }
            break;
        }

        case KI_ACTION_RESTORE:
        {
            // Put the factory paint back: re-seat whichever of the two toggles differs from
            // the car's default pair, then re-publish.
            if (IsLoading() || mbDisableCarModifyInteraction)
                break;

            const CgsID lHighlightedId = mCarSelector.GetHighlightedId();
            const BrnResource::VehicleListEntry* lpVehicleData =
                mpVehicleList->GetVehicleData(lHighlightedId);
            if (!CanCarBePainted(lpVehicleData))
                break;

            bool lbChanged = false;

            if (mOptionToggles.GetSelectable(E_OPTIONTOGGLE_PAINT_FINISH)->mItemText.miHighlightedIndex
                != lpVehicleData->GetDefaultPaintFinish())
            {
                lbChanged = true;
                mOptionToggles.HighlightItem(E_OPTIONTOGGLE_PAINT_FINISH,
                                             lpVehicleData->GetDefaultPaintFinish());
            }

            if (mColourMenuToggle.GetHighlightedColourIndex()
                != lpVehicleData->GetDefaultPaintColour())
            {
                lbChanged = true;
                mColourMenuToggle.HighlightIndex(lpVehicleData->GetDefaultPaintColour());
            }

            if (lbChanged)
            {
                SendPlayerCarColourEvent();
            }
            break;
        }

        default:
            break;
        }
    }

    // ================================================================================
    // game-state events
    // ================================================================================

    // ---- HandleCarInfoResponseEvent @ 0x824BBA10 -----------------------------------
    // Event 412. This screen overrides the handler purely to keep the base's two asserts and
    // then do NOTHING with the car list -- the livery screen's list arrives on 413 instead.
    // The X360 body is exactly the two asserts and a return.
    void CarSelectLivery::HandleCarInfoResponseEvent(const CgsModule::Event* lpEvent,
                                                     s32 /*liEventType*/)
    {
        CGS_ASSERT(lpEvent != 0, "lpEvent");   // cpp:929
        // The second assert's baked file is BrnCarSelectMain.cpp:582 (a copy-paste in the
        // original); it tests the same pointer.
        CGS_ASSERT(lpEvent != 0, "lpEvent");
    }

    // ---- HandlePlayerInfoResponse @ 0x824C8758 -------------------------------------
    // Event 406. Ask the game state for this car's colour (id 410), then latch the
    // "car needs repair" word: it BOTH disables the whole modify surface and arms the
    // fade-screen + "CAR_REPAIR_REQUIRED" ticker UpdateComponents posts.
    void CarSelectLivery::HandlePlayerInfoResponse(const CgsModule::Event* lpEvent,
                                                   s32 /*liEventType*/)
    {
        {
            GuiPlayerCarColourRequestWire410 lRequest;
            lRequest.mCarId = mCurrentSetupInfo.mCarId;
            mpStateInterface->GetOutputEventQueue()->AddEvent(&lRequest, KI_CHANNEL_GUI_OUT,
                                                              static_cast<s32>(sizeof(lRequest)));
        }

        const GuiPlayerInfoResponsePayload* lpPayload =
            reinterpret_cast<const GuiPlayerInfoResponsePayload*>(lpEvent);

        mbDisableCarModifyInteraction = (lpPayload->mbCarNeedsRepair != 0);
        if (mbDisableCarModifyInteraction)
        {
            mbFadeScreenPending = true;
        }
    }

    // ---- HandleUnlockedLiveryResponseEvent @ 0x824BBA80 ----------------------------
    // Event 413 -- the ONLY producer of this screen's livery list, and therefore the only
    // thing that makes mCarSelector non-empty. See the empty-group contract in
    // BrnCarSelectLivery_Components.cpp.
    void CarSelectLivery::HandleUnlockedLiveryResponseEvent(const CgsModule::Event* lpEvent,
                                                            s32 liEventType)
    {
        CGS_ASSERT(lpEvent != 0, "lpEvent");   // cpp:1033

        if (liEventType != 413)
        {
            return;
        }

        const GuiUnlockedLiveryPayload* lpPayload =
            reinterpret_cast<const GuiUnlockedLiveryPayload*>(lpEvent);

        // The two inlined CgsContainers::Array<s64,8> guards.
        CGS_ASSERT(lpPayload->miCount != -1,
                   "Array used before Construct/Clear was called");   // CgsArray.h:336
        CGS_ASSERT(lpPayload->miCount < static_cast<s32>(KI_MAX_LIVERIES_PER_MODEL),
                   "Too many cars for the selection");                // cpp:1042 (streamed)

        muNumUnlockedLiveryCars = 0u;
        for (s32 liCar = 0; liCar < lpPayload->miCount; ++liCar)
        {
            CGS_ASSERT(lpPayload->miCount != -1,
                       "Array used before Construct/Clear was called");   // CgsArray.h:336
            maUnlockedLiveryCars[muNumUnlockedLiveryCars] = lpPayload->maCarIds[liCar];
            muNumUnlockedLiveryCars = muNumUnlockedLiveryCars + 1u;
        }
    }

    // ---- HandlePlayerCarColourResponseEvent @ 0x824C0E18 ---------------------------
    // Event 414. The game state reports the car's live colour; re-seat the paint-finish row
    // and the picker on it (unless the modify surface is off, in which case there is nothing
    // to re-seat).
    void CarSelectLivery::HandlePlayerCarColourResponseEvent(const CgsModule::Event* lpEvent,
                                                             s32 liEventType)
    {
        CGS_ASSERT(lpEvent != 0, "lpEvent");   // cpp:1068

        if (liEventType != 414 || mbDisableCarModifyInteraction)
        {
            return;
        }

        const GuiPlayerCarColourPayload* lpPayload =
            reinterpret_cast<const GuiPlayerCarColourPayload*>(lpEvent);

        // Both index arguments ARE explicitly loaded from the payload by the X360, measured
        // at 0x824C0E74..0x824C0E8C:  li r4,1 / lwz r5,0(r30) -> HighlightItem(liItem =
        // payload+0x00), then addi r3,r31,0x2860 / lwz r4,4(r30) -> HighlightIndex(liIndex =
        // payload+0x04). (An earlier comment here claimed both were left unset -- it was
        // wrong, and the code below was already right.)
        mOptionToggles.HighlightItem(E_OPTIONTOGGLE_PAINT_FINISH, lpPayload->miPaintFinishIndex);
        mColourMenuToggle.HighlightIndex(lpPayload->miColourIndex);
        SetupPaintColourToggle(lpPayload->miColourIndex, mColourMenuToggle.IsHighlighted());
    }

}
