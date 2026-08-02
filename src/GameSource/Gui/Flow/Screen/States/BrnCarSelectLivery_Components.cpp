// ===================================================================================
// BrnGui::CarSelectLivery  -- partfile 02: the screen build + the component pumps
//   class:BrnGui::CarSelectLivery
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (asm + pseudocode):
//   SetupComponents         @ 0x824C8240   (DWARF cpp:462)
//   UpdateComponents        @ 0x824C7CB0   (DWARF cpp:275)
//   UpdateMenuComponents    @ 0x824C0AA0   (DWARF cpp:384)
//   SetupPaintColourToggle  @ 0x824C0B88   (DWARF cpp:611)
//   SetupCar                @ 0x824C80A8   (DWARF cpp:402)
//   SendPlayerCarColourEvent@ 0x824C87F0   (DWARF cpp:1097)
//
// ⚠️ EMPTY-GROUP CONTRACT. Without a GUI event 413 producer muNumUnlockedLiveryCars is 0,
// mCarSelector ends up empty, and SelectableGroup::GetHighlighted() has no lower bound on
// miHighlightedIndex -- on an empty group it has already been observed returning the
// NON-NULL garbage 0x0000FF00, which DEFEATS the console's own `!= 0` assert and
// access-violates on the ->GetId() that follows (see the long note in BrnCarSelectVehicle.h).
// Every GetHighlighted() site below therefore tests miHighlightedIndex > -1 first -- the
// group's real emptiness contract, and the console's own idiom.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectLivery.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiWorldDataController.h"                     // BrnGui::WorldDataController
#include "GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h"          // ButtonIconComponent::EPadButton
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // gpDebugPrint (the palette FLAG)
#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectVehicle.h"        // the SHARED carousel counter, see below
#include "SharedClasses/DataLists/VehicleList.h"                          // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"                     // BrnResource::VehicleListEntry
#include "SharedClasses/Graphics/BrnGlobalColourPalette.h"                // BrnWorld::PlayerCarColourPalette

#include <cstring>   // std::memset / std::strncpy

namespace BrnGui
{
    namespace
    {
        const s32 KI_CHANNEL_GUI_OUT = 40;
        const char KAC_EMPTY[] = "";

        // The three paint FINISHES the second toggle row offers, and their option ids
        // (the X360 builds both arrays on the stack: three string pointers and the qwords
        // 0 / 1 / 2).
        const s32 KI_NUM_PAINT_FINISHES = 3;

        // The livery option strings the first toggle row offers, indexed by livery slot.
        // Entries whose VehicleListEntry livery-type byte reads 3 or 4 are overridden to the
        // chrome / gold names.
        const s32 KI_LIVERY_TYPE_CHROME = 3;
        const s32 KI_LIVERY_TYPE_GOLD   = 4;

        // SetupPaintColourToggle's palette cap (X360 `cmplwi 60 / ble`).
        const s32 KI_MAX_PALETTE_COLOURS = 60;

        // The PresentationAction words TriggerSound is called with from this screen
        // (X360 `li r4, 7 / 8 / 9`). FLAG: named for their call sites -- the
        // AttribSys::Enums::PresentationAction enum slice with these three values is not in
        // the recovered set.
        const s32 KI_AUDIO_ACTION_SCROLL = 7;
        const s32 KI_AUDIO_ACTION_BACK   = 8;
        const s32 KI_AUDIO_ACTION_SELECT = 9;

        // ---- out-queue wire records --------------------------------------------------

        // The car-selection trigger SetupComponents and SetupCar both post:
        // { 8, 411, 16, <pad>, CgsID }, channel 40, 24 bytes.
        struct GuiCarSelectTriggerWire411 : public CgsGui::GuiEvent<411>
        {
            u32   muPad0C;    // +0x0C (the payload is 8-aligned past the header)
            CgsID mCarId;     // +0x10
            GuiCarSelectTriggerWire411()
                : CgsGui::GuiEvent<411>(8, 16), muPad0C(0), mCarId(0) {}
        };

        // The player-car-colour command SendPlayerCarColourEvent posts:
        // { 8, 416, 12, u32 paintFinishIndex, u32 colourIndex }, channel 40, 20 bytes.
        //
        // ⚠️ TWO EXPLICIT WORDS, NOT ONE u64 -- the same trap the id-192 record carries. The
        // X360 writes the pair into one stack qword and stores it with a single `std`
        // (asm 0x824C8824 stores the COLOUR index at var_40+4 and 0x824C884C the PAINT-FINISH
        // index at var_40, then 0x824C8858/0x824C8860 reload and store the qword), so on the
        // big-endian console word0 is the paint-finish index and word1 the colour index.
        // Modelling it as a u64 on a little-endian host would swap them.
        struct GuiPlayerCarColourWire416 : public CgsGui::GuiEvent<416>
        {
            u32 muPaintFinishIndex;   // +0x0C (X360 word0)
            u32 muColourIndex;        // +0x10 (X360 word1)
            GuiPlayerCarColourWire416()
                : CgsGui::GuiEvent<416>(8, 12), muPaintFinishIndex(0), muColourIndex(0) {}
        };

        // The "clear the ticker" command: { 2, 536, 12, u8 1, u8 0 }, channel 40, 16 bytes.
        // Same record BrnCarSelectVehicle_Input.cpp builds; kept TU-local for the same reason
        // (the type's canonical home is mutually exclusive with BrnGuiEventTypeDefs.h).
        struct GuiTickerFlagsWire536 : public CgsGui::GuiEvent<536>
        {
            u8 mbFlagA;   // +0x0C
            u8 mbFlagB;   // +0x0D
            GuiTickerFlagsWire536()
                : CgsGui::GuiEvent<536>(2, 12), mbFlagA(1), mbFlagB(0) {}
        };

        // The 0x818-byte custom ticker message, layout recovered from
        // BrnGui::GuiEventTickerCustomMessage::AddString @0x823A6940 (its asserts bake
        // BrnGuiEventTypeDefs.h:390/391/392). Same TU-local model as the sibling screen's.
        struct GuiTickerCustomMessagePayload
        {
            static const s32 KI_MAX_NUM_STRINGS   = 4;
            static const s32 KI_MAX_STRING_LENGTH = 512;

            s32  maiStringTypes[KI_MAX_NUM_STRINGS];                       // +0x000
            char maacStrings[KI_MAX_NUM_STRINGS][KI_MAX_STRING_LENGTH];    // +0x010
            s8   mi8NumStrings;                                            // +0x810
            u8   maFlags[4];                                               // +0x811
            u8   maPad815[3];                                              // +0x815

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
    // the one-shot screen build
    // ================================================================================

    // ---- SetupComponents @ 0x824C8240 ----------------------------------------------
    void CarSelectLivery::SetupComponents()
    {
        CGS_ASSERT(mpVehicleList != 0, "mpVehicleList");   // cpp:464
        CGS_ASSERT(mpGuiCache != 0, "lpGuiCache");         // cpp:467

        // The whole car-modification surface is suppressed when the flow has already shown
        // its car-select transition, or when this car has nothing to modify (one livery and
        // no repaint). In that mode the screen is a pure "press A to continue" confirmation --
        // which is exactly the Junkyard starter-car case.
        if (mpGuiCache->GetCarSelectTransitionAlreadyShown())
        {
            mbDisableCarModifyInteraction = true;
        }

        if (muNumUnlockedLiveryCars <= 1u)
        {
            const s32 liIndex = mpVehicleList->GetVehicleIndex(mCurrentSetupInfo.mCarId);
            const BrnResource::VehicleListEntry* lpEntry =
                (liIndex < 0) ? 0 : mpVehicleList->GetVehicleData(liIndex);
            if (!CanCarBePainted(lpEntry))
            {
                mbDisableCarModifyInteraction = true;
            }
        }

        CarSelectMain::SetupCarNameComponent(mCurrentSetupInfo.mCarId);

        // The eight livery option labels, built on the stack exactly as the console does.
        const char* lapacLiveryOptions[KI_MAX_LIVERIES_PER_MODEL] =
        {
            "$LIVERY_0", "$LIVERY_1", "$LIVERY_2", "$LIVERY_3",
            "$LIVERY_4", "$LIVERY_5", "$LIVERY_6", "$LIVERY_7",
        };

        mCarSelector.SetupTextSelection(static_cast<s32>(muNumUnlockedLiveryCars), true, 0, 0);

        u64 lau64LiveryIds[KI_MAX_LIVERIES_PER_MODEL];
        std::memset(lau64LiveryIds, 0, sizeof(lau64LiveryIds));

        u32 luHighlightSlot = 0;
        for (u32 luLivery = 0; luLivery < muNumUnlockedLiveryCars; ++luLivery)
        {
            const s32 liIndex = mpVehicleList->GetVehicleIndex(maUnlockedLiveryCars[luLivery]);
            const BrnResource::VehicleListEntry* lpVehicleData =
                (liIndex < 0) ? 0 : mpVehicleList->GetVehicleData(liIndex);
            CGS_ASSERT(lpVehicleData != 0, "lpVehicleData");   // cpp:508

            lau64LiveryIds[luLivery] = luLivery;

            Selectable* lpSelectable = mCarSelector.GetSelectable(static_cast<s32>(luLivery));
            CGS_ASSERT(lpSelectable != 0, "lpSelectable");     // cpp:514

            // ⚠️ ENTRY guard (not in the X360 body -- it dereferences lpVehicleData on the
            // next line). Removed with the stand-in car-id producer; see CanCarBePainted.
            if (lpVehicleData == 0 || lpSelectable == 0)
            {
                continue;
            }

            lpSelectable->SetId(lpVehicleData->GetId());
            lpSelectable->SetDirty();
            lpSelectable->SetActive(true);       // item slot 0
            lpSelectable->SetSelectable(true);   // item slot 2

            mCarSelector.SetItemText(static_cast<s32>(luLivery), lpVehicleData->GetName());

            if (lpVehicleData->GetId() == mCurrentSetupInfo.mCarId)
            {
                luHighlightSlot = luLivery;
            }

            const u8 luLiveryType = lpVehicleData->GetLiveryType();
            if (luLiveryType == KI_LIVERY_TYPE_CHROME)
            {
                lapacLiveryOptions[luLivery] = "$LIVERY_CHROME";
            }
            if (luLiveryType == KI_LIVERY_TYPE_GOLD)
            {
                lapacLiveryOptions[luLivery] = "$LIVERY_GOLD";
            }
        }

        if (!mbDisableCarModifyInteraction)
        {
            mOptionToggles.SetupToggle(E_OPTIONTOGGLE_LIVERY,
                                       static_cast<s32>(muNumUnlockedLiveryCars), true,
                                       KAPC_OPTIONS[E_OPTIONTOGGLE_LIVERY],
                                       lapacLiveryOptions, lau64LiveryIds);
            mOptionToggles.HighlightItem(E_OPTIONTOGGLE_LIVERY,
                                         static_cast<s32>(luHighlightSlot));
        }

        mCarSelector.HighlightId(mCurrentSetupInfo.mCarId);

        // The three paint finishes and their option ids (X360: three stack pointers + the
        // qwords 0 / 1 / 2).
        const char* lapacPaintOptions[KI_NUM_PAINT_FINISHES] =
        {
            "$PAINT_GLOSS", "$PAINT_METALLIC", "$PAINT_PEARLESCENT",
        };
        u64 lau64PaintIds[KI_NUM_PAINT_FINISHES] = { 0u, 1u, 2u };

        if (!mbDisableCarModifyInteraction)
        {
            mOptionToggles.SetupToggle(E_OPTIONTOGGLE_PAINT_FINISH, KI_NUM_PAINT_FINISHES, true,
                                       KAPC_OPTIONS[E_OPTIONTOGGLE_PAINT_FINISH],
                                       lapacPaintOptions, lau64PaintIds);
            mOptionToggles.HighlightItem(E_OPTIONTOGGLE_PAINT_FINISH, 0);
            mOptionToggles.SetHighlightable(true);   // group slot 1
            mOptionToggles.SetHighlighted(true);     // group slot 3
        }

        if (!mbDisableCarModifyInteraction)
        {
            SetupPaintColourToggle(0, false);

            // With a single livery the LIVERY row has nothing to offer, so the highlight
            // starts on the PAINT FINISH row.
            if (muNumUnlockedLiveryCars <= 1u)
            {
                mOptionToggles.HighlightIndex(E_OPTIONTOGGLE_PAINT_FINISH);   // group slot 12
            }
        }

        CGS_ASSERT(mpGuiCache->GetWorldDataController() != 0,
                   "mpWorldDataController");   // BrnGuiCache.h:2324
        mManufacturerLogo.Set(mpGuiCache->GetWorldDataController()->GetVehicleList(),
                              mCurrentSetupInfo.mCarId);

        {
            GuiCarSelectTriggerWire411 lTrigger;
            lTrigger.mCarId = mCurrentSetupInfo.mCarId;
            mpStateInterface->GetOutputEventQueue()->AddEvent(&lTrigger, KI_CHANNEL_GUI_OUT,
                                                              static_cast<s32>(sizeof(lTrigger)));
        }

        UpdateMenuComponents();
    }

    // ================================================================================
    // per-frame pumps
    // ================================================================================

    // ---- UpdateComponents @ 0x824C7CB0 ---------------------------------------------
    void CarSelectLivery::UpdateComponents()
    {
        if (mbFirstUpdateComponentsFrame)
        {
            mbFirstUpdateComponentsFrame = false;
        }

        if (mbFadeScreenPending)
        {
            mbFadeScreenPending = false;
            mFadeScreenAnimComponent.AddOutputAptViewState("apt_Transition", "fade", false);

            // Clear the ticker, then post the "car needs repair" line.
            {
                GuiTickerFlagsWire536 lTickerClear;
                mpStateInterface->GetOutputEventQueue()->AddEvent(&lTickerClear, KI_CHANNEL_GUI_OUT,
                                                                  static_cast<s32>(sizeof(lTickerClear)));
            }
            {
                char lacKey[32];
                CgsCore::SPrintf(lacKey, 31, "%s", "CAR_REPAIR_REQUIRED");
                lacKey[31] = 0;

                GuiTickerCustomMessageWire lTicker;
                lTicker.mMessage.AddString(lacKey, 2);
                mpStateInterface->GetOutputEventQueue()->AddEvent(&lTicker, KI_CHANNEL_GUI_OUT,
                                                                  static_cast<s32>(sizeof(lTicker)));
            }
        }

        if (!mbDisableCarModifyInteraction)
        {
            mOptionToggles.Update();      // group slot 5
            mColourMenuToggle.Update();   // toggle slot 5
        }

        // The two help items: CONTINUE is offered whenever nothing is loading, BACK only when
        // there is more than one car to go back to.
        if (IsLoading())
        {
            mHelpItemContinue.SetItem(KAC_EMPTY, ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                      ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            mHelpItemBack.SetItem(KAC_EMPTY, ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                  ButtonIconComponent::E_PADBUTTON_INVISIBLE);
        }
        else
        {
            mHelpItemContinue.SetItem("$GENERAL_OPTION_CONTINUE",
                                      ButtonIconComponent::E_PADBUTTON_SELECT,
                                      ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            // ⭐ THE CAROUSEL COUNT IS SHARED WITH THE SIBLING SCREEN. The X360 reads
            // dword_82FB4958 here, and that is the SAME global BrnGui::CarSelectVehicle's
            // HandleCarInfoResponseEvent writes (it is a linker-visible counter, not a
            // file-scope static -- two different TUs read the one address). "Back" is only
            // offered when there is more than one car to go back to.
            if (CarSelectVehicle::gsiNumCarouselCars > 1)
            {
                mHelpItemBack.SetItem("$GENERAL_OPTION_BACK",
                                      ButtonIconComponent::E_PADBUTTON_BACK,
                                      ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            }
            else
            {
                mHelpItemBack.SetItem(KAC_EMPTY, ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                      ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            }
        }

        // ⚠️ EMPTY-GROUP CONTRACT (see the banner). The console asserts on GetHighlighted()
        // and then dereferences it; on an empty selector that assert does NOT fire.
        CGS_ASSERT(mCarSelector.miHighlightedIndex > -1 && mCarSelector.GetHighlighted() != 0,
                   "GetHighlighted()");   // BrnSelectableGroup.h:218
        const BrnResource::VehicleListEntry* lpVehicleData = 0;
        if (mCarSelector.miHighlightedIndex > -1)
        {
            const Selectable* lpHighlighted = mCarSelector.GetHighlighted();
            const s32 liIndex = mpVehicleList->GetVehicleIndex(lpHighlighted->GetId());
            lpVehicleData = (liIndex < 0) ? 0 : mpVehicleList->GetVehicleData(liIndex);
        }

        // "Restore default paint" is offered only when the car CAN be painted, the modify
        // surface is live, and the current finish/colour pair differs from the car's default.
        const bool lbRestoreAvailable =
            !IsLoading()
            && CanCarBePainted(lpVehicleData)
            && !mbDisableCarModifyInteraction
            && lpVehicleData != 0
            && !(mOptionToggles.GetSelectable(E_OPTIONTOGGLE_PAINT_FINISH)
                     ->mItemText.miHighlightedIndex == lpVehicleData->GetDefaultPaintFinish()
                 && mColourMenuToggle.GetHighlightedColourIndex()
                        == lpVehicleData->GetDefaultPaintColour());

        if (!lbRestoreAvailable)
        {
            mHelpItemRestoreDefaultPaint.SetItem(KAC_EMPTY,
                                                 ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                                 ButtonIconComponent::E_PADBUTTON_INVISIBLE);

            // The help BAR animation follows the number of prompts on screen: zero while
            // loading, two once the screen is live.
            if (IsLoading())
            {
                if (mHelpBarButtonCount != 0)
                {
                    mHelpBarAnimComponent.AddOutputAptViewState("apt_Transition", "zero", false);
                    mHelpBarButtonCount = 0;
                }
            }
            else if (mHelpBarButtonCount != 2)
            {
                mHelpBarAnimComponent.AddOutputAptViewState("apt_Transition", "two", false);
                mHelpBarButtonCount = 2;
            }
        }
        else
        {
            mHelpItemRestoreDefaultPaint.SetItem("$GENERAL_OPTION_RESTORE",
                                                 ButtonIconComponent::E_PADBUTTON_OPTION1,
                                                 ButtonIconComponent::E_PADBUTTON_INVISIBLE);
            if (mHelpBarButtonCount != 3)
            {
                mHelpBarAnimComponent.AddOutputAptViewState("apt_Transition", "three", false);
                mHelpBarButtonCount = 3;
            }
        }
    }

    // ---- UpdateMenuComponents @ 0x824C0AA0 -----------------------------------------
    // Re-enable (or disable) the whole paint surface for whichever livery car is highlighted.
    void CarSelectLivery::UpdateMenuComponents()
    {
        CGS_ASSERT(mCarSelector.miHighlightedIndex > -1 && mCarSelector.GetHighlighted() != 0,
                   "GetHighlighted()");   // BrnSelectableGroup.h:218
        if (mCarSelector.miHighlightedIndex <= -1)
        {
            return;
        }

        const Selectable* lpHighlighted = mCarSelector.GetHighlighted();
        const s32 liIndex = mpVehicleList->GetVehicleIndex(lpHighlighted->GetId());
        const BrnResource::VehicleListEntry* lpVehicleData =
            (liIndex < 0) ? 0 : mpVehicleList->GetVehicleData(liIndex);

        const bool lbPaintable = CanCarBePainted(lpVehicleData);

        mColourMenuToggle.SetActive(lbPaintable);                                   // toggle slot 0
        mOptionToggles.GetSelectable(E_OPTIONTOGGLE_PAINT_FINISH)->SetActive(lbPaintable);  // row slot 0
    }

    // ---- SetupPaintColourToggle @ 0x824C0B88 ---------------------------------------
    // (Re)build the five-picker colour toggle from the palette the CURRENT paint finish
    // selects, then re-seat it on liColourIndex.
    void CarSelectLivery::SetupPaintColourToggle(s32 liColourIndex, bool lbHighlighted)
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:613

        const s32 liPaintFinish =
            mOptionToggles.GetSelectable(E_OPTIONTOGGLE_PAINT_FINISH)->mItemText.miHighlightedIndex;

        CGS_ASSERT(mpGuiCache->GetWorldDataController() != 0,
                   "mpWorldDataController");   // BrnGuiCache.h:2324

        // ⚠️⚠️ FLAG PC bring-up guard -- NOT in the X360 body, and it is load-bearing.
        // GetColourPaletteFromType hands back `&mpPlayerCarColours->maPalettes[type]` with NO
        // readiness check of its own; the console can do that because its WorldDataController
        // always walks through E_WORLDDATACONTROLLERSTATE_PLAYERCARCOLOURS (10) /
        // WFPLAYERCARCOLOURS (11) before any car-select screen is interactive. On THIS build
        // the acquire machine parks at state 7 (PREPARING_ACQUIRING_PROGRESSION) because no PC
        // producer answers the progression request, so the palette resource is never loaded and
        // the returned pointer is into unmapped storage. Boot-measured: WER fault offset
        // 0x1011D9 == ColourSelection::SetupColourSelectionGradient + 0x239, i.e. the first
        // read of the palette's colour columns. Remove this guard when the acquire machine
        // reaches state 11 -- the assert below will say so.
        BrnGui::WorldDataController* lpWorldData = mpGuiCache->GetWorldDataController();
        if (lpWorldData->GetState() < WorldDataController::E_WORLDDATACONTROLLERSTATE_WFPLAYERCARCOLOURS)
        {
            static bool s_bLoggedNoPalette = false;
            if (!s_bLoggedNoPalette && CgsDev::Log::gpDebugPrint != 0)
            {
                s_bLoggedNoPalette = true;
                *CgsDev::Log::gpDebugPrint
                    << "[CarSelectLivery] FLAG: the player-car-colour palette is not loaded "
                       "(WorldDataController state "
                    << static_cast<s32>(lpWorldData->GetState())
                    << " < 11) -- the colour picker stays empty this run.\n";
            }
            return;
        }

        const BrnWorld::PlayerCarColourPalette* lpPalette =
            lpWorldData->GetColourPaletteFromType(
                static_cast<BrnWorld::EPalettesTypes>(liPaintFinish));

        s32 liNumColours = KI_MAX_PALETTE_COLOURS;
        if (lpPalette->miNumColours <= KI_MAX_PALETTE_COLOURS)
        {
            liNumColours = lpPalette->miNumColours;
        }

        // The three parallel arrays the toggle wants: the top colour, the bottom colour and
        // the option id, one per palette entry.
        const rw::math::vpu::Vector4* lapTopColours[KI_MAX_PALETTE_COLOURS];
        const rw::math::vpu::Vector4* lapBottomColours[KI_MAX_PALETTE_COLOURS];
        u64                           lau64Ids[KI_MAX_PALETTE_COLOURS];

        for (s32 liColour = 0; liColour < liNumColours; ++liColour)
        {
            // The X360 walks both pointer columns by 16 bytes (one Vector4) per entry:
            // `*&v22[i] = *palette + 16*i` / `*&v23[i] = palette[1] + 16*i`.
            lau64Ids[liColour]         = static_cast<u64>(liColour);
            lapTopColours[liColour]    = &lpPalette->mpPaintColours[liColour];
            lapBottomColours[liColour] = &lpPalette->mpPearlColours[liColour];
        }

        mColourMenuToggle.Clear();                        // toggle slot 6
        mColourMenuToggle.SetActive(true);                // toggle slot 0
        mColourMenuToggle.SetHighlightable(true);         // toggle slot 1
        mColourMenuToggle.SetHighlighted(lbHighlighted);  // toggle slot 3
        mColourMenuToggle.SetSelectable(true);            // toggle slot 2

        mColourMenuToggle.SetupMenuToggleGradient(liNumColours, true, "$CAR_MOD_PAINT_COLOUR",
                                                  lapTopColours, lapBottomColours, lau64Ids);
        mColourMenuToggle.HighlightIndex(liColourIndex);
        mColourMenuToggle.SetDirty();                     // *(toggle + 0xC) |= 0x10
        mColourMenuToggle.Update();                       // toggle slot 5

        CGS_ASSERT(mpVehicleList != 0, "mpVehicleList");   // cpp:654
        CGS_ASSERT(mCarSelector.miHighlightedIndex > -1 && mCarSelector.GetHighlighted() != 0,
                   "GetHighlighted()");                    // BrnSelectableGroup.h:218
        if (mCarSelector.miHighlightedIndex <= -1)
        {
            return;
        }

        const Selectable* lpHighlighted = mCarSelector.GetHighlighted();
        const s32 liIndex = mpVehicleList->GetVehicleIndex(lpHighlighted->GetId());
        const BrnResource::VehicleListEntry* lpVehicleData =
            (liIndex < 0) ? 0 : mpVehicleList->GetVehicleData(liIndex);

        mColourMenuToggle.SetActive(CanCarBePainted(lpVehicleData));   // toggle slot 0
    }

    // ================================================================================
    // selection
    // ================================================================================

    // ---- SetupCar @ 0x824C80A8 -----------------------------------------------------
    void CarSelectLivery::SetupCar(const CarSetupInfo* lpSetupInfo)
    {
        // cpp:404 -- streamed on the console.
        CGS_ASSERT(lpSetupInfo != 0, "Invalid CarSetupInfo structure");

        mDesiredSetupInfo = *lpSetupInfo;
        // cpp:600 -- the assert's baked file is BrnCarSelectMain.cpp (the base's own copy).
        CGS_ASSERT(mDesiredSetupInfo.mbSelectable, "mDesiredSetupInfo.mbSelectable");

        mbCarChangeInProgress = true;

        CarSelectMain::SetupCarNameComponent(lpSetupInfo->mCarId);

        GuiCarSelectTriggerWire411 lTrigger;
        lTrigger.mCarId = lpSetupInfo->mCarId;
        mpStateInterface->GetOutputEventQueue()->AddEvent(&lTrigger, KI_CHANNEL_GUI_OUT,
                                                          static_cast<s32>(sizeof(lTrigger)));
    }

    // ---- SendPlayerCarColourEvent @ 0x824C87F0 -------------------------------------
    void CarSelectLivery::SendPlayerCarColourEvent()
    {
        const s32 liColourIndex = mColourMenuToggle.GetHighlightedColourIndex();
        const s32 liPaintFinish =
            mOptionToggles.GetSelectable(E_OPTIONTOGGLE_PAINT_FINISH)->mItemText.miHighlightedIndex;

        GuiPlayerCarColourWire416 lColour;
        lColour.muPaintFinishIndex = static_cast<u32>(liPaintFinish);   // X360 word0
        lColour.muColourIndex      = static_cast<u32>(liColourIndex);   // X360 word1
        mpStateInterface->GetOutputEventQueue()->AddEvent(&lColour, KI_CHANNEL_GUI_OUT,
                                                          static_cast<s32>(sizeof(lColour)));

        SetupPaintColourToggle(liColourIndex, mColourMenuToggle.IsHighlighted());
    }
}
