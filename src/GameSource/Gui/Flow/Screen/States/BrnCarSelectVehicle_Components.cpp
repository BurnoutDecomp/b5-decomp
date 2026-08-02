// ===================================================================================
// BrnGui::CarSelectVehicle  -- partfile 02: the screen build + the per-frame pump
//   class:BrnGui::CarSelectVehicle
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (asm + pseudocode):
//   SetupComponents             @ 0x824C9978   (DWARF cpp:355)
//   SetupMenuComponents         @ 0x824BECD0   (DWARF cpp:519)
//   SetCarSelectorComponent     @ 0x824C0FE8   (DWARF cpp:425)
//   SetupStatsComponent         @ 0x824C1200   (DWARF cpp:1109)
//   SetupCarsUnlockedTextComponent               (DWARF cpp:1155 -- inlined on X360)
//   SetCarouselComponent        @ 0x824BBE90   (DWARF cpp:1176)
//   SetSliderBarComponent                        (DWARF cpp:1264 -- inlined on X360)
//   UpdateComponents            @ 0x824BBD18   (DWARF cpp:971)
//   UpdateCarouselTransition    @ 0x824D7D98   (DWARF cpp:854)
//
// The component vtable slot map these by-name calls stand in for (BrnSelectableGroup.h,
// re-validated against off_82073020):
//   0 SetActive  1 SetHighlightable  2 SetSelectable  3 SetHighlighted  4 Select
//   5 Update     6 Clear             7 Add            8 Enable          9 Disable
//  10 HighlightNext  11 HighlightPrevious  12 HighlightIndex
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectVehicle.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiWorldDataController.h"                     // BrnGui::WorldDataController
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectable.h"          // BrnGui::Selectable (GetId)
#include "SharedClasses/DataLists/VehicleList.h"                          // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"                     // BrnResource::VehicleListEntry

namespace BrnGui
{
    namespace
    {
        const s32 KI_CHANNEL_GUI_OUT = 40;

        // The pooled empty string @0x820046A7 the X360 passes for "no caption".
        const char KAC_EMPTY[] = "";

        // The three car-type captions SetupStatsComponent builds its "%s" from (a local
        // const array in the source; the X360 stack-builds it at 0x824C148C..0x824C1494).
        const char* const KAPC_CAR_TYPE_STRINGIDS[3] =
        {
            "$CARTYPE_DANGER",      // E_CARTYPE_DANGER
            "$CARTYPE_AGRESSION",   // E_CARTYPE_AGGRESSION (the console's spelling)
            "$CARTYPE_STUNTS",      // E_CARTYPE_STUNTS
        };

        // The two-flag ticker command SetupComponents posts as id 536:
        // { 2, 536, 12, u8 1, u8 0 } on channel 40, 16 bytes.
        struct GuiTickerFlagsWire536 : public CgsGui::GuiEvent<536>
        {
            u8 mbFlagA;   // +0x0C
            u8 mbFlagB;   // +0x0D
            GuiTickerFlagsWire536()
                : CgsGui::GuiEvent<536>(2, 12), mbFlagA(1), mbFlagB(0) {}
        };
    }

    // ================================================================================
    // SetupComponents -- the one-shot screen build
    // ================================================================================

    // ---- SetupComponents @ 0x824C9978 ----------------------------------------------
    void CarSelectVehicle::SetupComponents()
    {
        CGS_ASSERT(mpGuiCache != 0, "lpGuiCache");   // cpp:371

        // Blank both help prompts (invisible glyphs) until UpdateComponents decides what
        // the continue prompt should say. Back first, exactly as the console does.
        mHelpItemBack.SetItem(KAC_EMPTY, ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                          ButtonIconComponent::E_PADBUTTON_INVISIBLE);
        mHelpItemContinue.SetItem(KAC_EMPTY, ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                              ButtonIconComponent::E_PADBUTTON_INVISIBLE);

        SetupMenuComponents();
        SetCarSelectorComponent();
        CarSelectMain::SetupCarNameComponent(mCurrentSetupInfo.mCarId);

        // The manufacturer badge. ⚠️ SUBSTITUTION, flagged: the X360 re-fetches the list as
        // mpGuiCache->GetWorldDataController()->GetVehicleList() (its inlined
        // mpWorldDataController assert is BrnGuiCache.h:2324). That is provably the SAME
        // pointer as the latched mpVehicleList -- CarSelectMain::UpdateGuiCache assigns
        // mpVehicleList from exactly that expression -- and on this build the re-fetch would
        // fire GuiCache's own "mpWorldDataController" assert (the pointer is never populated;
        // see the PC-BUILD GUARD note in BrnCarSelectVehicle.h), and a dev assert BLOCKS the
        // sim. Restore the re-fetch when the GUI WorldDataController lands.
        if (mpVehicleList != 0)
            mManufacturerLogo.Set(mpVehicleList, mCurrentSetupInfo.mCarId);

        SetupCarsUnlockedTextComponent();

        mTitleText.SetText("CAR SELECT: SELECT VEHICLE");

        // The stats gauges follow the HIGHLIGHTED car, not the committed one: the X360
        // copies mCurrentSetupInfo (both qwords) into a stack CarSetupInfo and then
        // overwrites only its car id with the highlighted row's.
        CarSetupInfo lHighlightedSetupInfo = mCurrentSetupInfo;

        CGS_ASSERT(mCarSelector.GetHighlighted() != 0, "GetHighlighted()");   // BrnSelectableGroup.h:218
        const CgsID lHighlightedId = mCarSelector.GetHighlighted()->GetId();
        lHighlightedSetupInfo.mCarId = lHighlightedId;

        SetupStatsComponent(&lHighlightedSetupInfo);

        for (s32 liIcon = 0; liIcon < KI_NUMBER_VISIBLE_VEHICLE_ICONS; ++liIcon)
            mCarouselOverviewSelectableGroup.Enable(liIcon);        // group slot 8

        mCarouselOverviewSelectableGroup.HighlightIndex(2);         // group slot 12 (centre icon)

        SetCarouselComponent(lHighlightedId);

        // X360: `lis r11,1 / ori r11,r11,0x3B5E / lbzx r11, r27, r11` -- a byte in the
        // GuiCache at +0x13B5E (r27 is mpGuiCache, latched at the top of the function).
        // ⚠️ Hex-Rays renders that indexed load as `*(HIDWORD(v6) + 80734)`, a 64-bit-pair
        // artefact; the base is the cache.
        if (mpGuiCache->GetCarSelectTransitionAlreadyShown())
        {
            GuiTickerFlagsWire536 lTickerFlags;
            mpStateInterface->GetOutputEventQueue()->AddEvent(&lTickerFlags, KI_CHANNEL_GUI_OUT,
                                                              static_cast<s32>(sizeof(lTickerFlags)));
        }
        else
        {
            mMainAnimComponent.AddOutputAptViewState("apt_Transition", "transin", false);
        }
    }

    // ---- SetupMenuComponents @ 0x824BECD0 ------------------------------------------
    // ⚠️ THIS BODY COMPUTES A LIST AND DISCARDS IT, in the X360 ARTIST image as shipped.
    // It builds a 128-entry id scratch array (0,1,2,...), then walks the whole VehicleList
    // and appends each entry's display-name pointer for every car that is in maSelectedCars
    // -- and then returns without handing either array to anything. Both arrays are stack
    // locals at the SAME frame offset (r1+0x50 for both the `std` id fill and the `stw`
    // name fill), and the function contains no `bl` other than the two GetVehicleData calls.
    // The work it looks like it should do is done by SetCarSelectorComponent, which
    // SetupComponents calls on the very next line and which re-derives the same list
    // straight from maSelectedCars (and passes NULL for both of SetupTextSelection's array
    // parameters). Reproduced faithfully -- including the discard -- because the observable
    // behaviour of the shipped build is exactly this: the asserts fire, nothing else changes.
    void CarSelectVehicle::SetupMenuComponents()
    {
        // cpp:540 -- ⚠️ PC-BUILD GUARD, see "PC-BUILD GUARDS" in BrnCarSelectVehicle.h. The
        // console asserts mpVehicleList here; on this build the list is legitimately absent
        // (the GUI WorldDataController is unreconstructed) and a dev assert BLOCKS the sim,
        // so the bail replaces it. Restore the assert with the list.
        if (mpVehicleList == 0)
            return;

        CgsID       laCarIds[KI_MAX_SELECTABLE_CARS];
        const char* lapacCarNames[KI_MAX_SELECTABLE_CARS];

        for (s32 liId = 0; liId < KI_MAX_SELECTABLE_CARS; ++liId)
            laCarIds[liId] = static_cast<CgsID>(liId);

        s32 liNumNames = 0;
        for (s32 liVehicle = 0; liVehicle < mpVehicleList->GetVehicleCount(); ++liVehicle)
        {
            const CgsID lCarId = mpVehicleList->GetVehicleData(liVehicle)->GetId();

            for (s32 liSelected = 0; liSelected < gsiNumCarouselCars; ++liSelected)
            {
                if (maSelectedCars[liSelected] == lCarId)
                {
                    lapacCarNames[liNumNames++] =
                        mpVehicleList->GetVehicleData(liVehicle)->GetName();
                    break;
                }
            }
        }

        // The console drops both arrays here. Keep the compiler from warning about it
        // without changing what the function does.
        (void)laCarIds;
        (void)lapacCarNames;
        (void)liNumNames;
    }

    // ---- SetCarSelectorComponent @ 0x824C0FE8 --------------------------------------
    // Rebuild the car-name selector from maSelectedCars, then highlight the committed car
    // (or its livery parent). SetupTextSelection is given NULL for both array parameters:
    // the ids and the row texts are pushed row by row in the loop below.
    void CarSelectVehicle::SetCarSelectorComponent()
    {
        // cpp:441 -- ⚠️ PC-BUILD GUARD (same as SetupMenuComponents above).
        if (mpVehicleList == 0)
            return;

        const s32 liNumCars = gsiNumCarouselCars;

        mCarSelector.Clear();                                          // component slot 6
        mCarSelector.SetupTextSelection(liNumCars, liNumCars >= 3, 0, 0);

        for (s32 liCar = 0; liCar < liNumCars; ++liCar)
        {
            Selectable* lpSelectable = mCarSelector.GetSelectable(liCar);
            CGS_ASSERT(lpSelectable != 0, "lpSelectable");             // cpp:457

            const BrnResource::VehicleListEntry* lpVehicleData =
                mpVehicleList->GetVehicleData(maSelectedCars[liCar]);

            // ⚠️ PC-BUILD GUARD -- see "PC-BUILD GUARDS" in BrnCarSelectVehicle.h.
            if (lpVehicleData == 0)
                continue;

            lpSelectable->SetId(lpVehicleData->GetId());
            lpSelectable->SetDirty();
            lpSelectable->SetActive(true);                             // row slot 0
            lpSelectable->SetSelectable(true);                         // row slot 2

            mCarSelector.SetItemText(liCar, lpVehicleData->GetName());
        }

        // A livery variant is highlighted under its PARENT car's row.
        CgsID lHighlightId = mCurrentSetupInfo.mCarId;
        const BrnResource::VehicleListEntry* lpCurrentData =
            mpVehicleList->GetVehicleData(mCurrentSetupInfo.mCarId);

        // ⚠️ PC-BUILD GUARD -- see "PC-BUILD GUARDS" in BrnCarSelectVehicle.h. The console
        // dereferences lpCurrentData unconditionally on the next line.
        if (lpCurrentData != 0)
        {
            const u8 luLiveryType = lpCurrentData->GetLiveryType();
            if (luLiveryType == 1 || luLiveryType == 3 || luLiveryType == 4)
                lHighlightId = lpCurrentData->GetParentId();
        }

        const s32 liHighlightIndex = mCarSelector.GetIndexFromId(lHighlightId);
        if (liHighlightIndex < 0 || liHighlightIndex >= liNumCars)
            mCarSelector.HighlightIndex(0);                            // component slot 12
        else
            mCarSelector.HighlightId(lHighlightId);
    }

    // ---- SetupStatsComponent @ 0x824C1200 ------------------------------------------
    // Push the three gauges and the car-type caption for lpSetupInfo's car. Every lookup
    // goes through the CACHE's vehicle list (the X360 re-fetches it once and re-resolves
    // the entry four separate times -- reproduced as one lookup by name).
    void CarSelectVehicle::SetupStatsComponent(const CarSetupInfo* lpSetupInfo)
    {
        CGS_ASSERT(mpGuiCache != 0, "lpGuiCache");   // cpp:1125

        // ⚠️ SAME FLAGGED SUBSTITUTION as SetupComponents' badge line above: the X360 reads
        // mpGuiCache->GetWorldDataController()->GetVehicleList() here, which is the same
        // pointer CarSelectMain::UpdateGuiCache latched into mpVehicleList.
        const BrnResource::VehicleList* lpVehicleList = mpVehicleList;

        if (lpVehicleList == 0)
        {
            // cpp:1128 (streamed form). ⚠️ The console asserts here; on this build the list
            // is legitimately absent until the GUI WorldDataController lands, and a dev
            // assert BLOCKS the sim -- so the bail is kept and the assert is not raised.
            return;
        }

        const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(lpSetupInfo->mCarId);
        const BrnResource::VehicleListEntry* lpVehicleData =
            (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);

        // ⚠️ PC-BUILD GUARD -- see "PC-BUILD GUARDS" in BrnCarSelectVehicle.h. The console
        // dereferences lpVehicleData unconditionally from here on (it has no assert at all
        // on this pointer), which it can afford because the car it is handed is always a
        // live VehicleList id.
        if (lpVehicleData == 0)
            return;

        const u8 luStrengthStat = lpVehicleData->GetStrengthStat();   // entry +0x9B
        const u8 luCarType      = lpVehicleData->GetCarType();        // entry +0xE8

        CGS_ASSERT(luCarType < 3, "Invalid car type");   // cpp:1136 (streamed form)

        const u32 luBarColour = mauBoostColours[luCarType];

        mSpeedStatsBar.SetCar(lpVehicleData->GetSpeedStat(), luBarColour);   // entry +0xEC
        mBoostStatsBar.SetCar(lpVehicleData->GetBoostStat(), luBarColour);   // entry +0xED
        mStrengthStatsBar.SetCar(luStrengthStat, luBarColour);

        char lacCarType[128];
        lacCarType[31] = 0;
        CgsCore::SPrintf(lacCarType, 31, "%s", KAPC_CAR_TYPE_STRINGIDS[luCarType]);
        mCarType.SetText(lacCarType);
    }

    // ---- SetupCarsUnlockedTextComponent (DWARF cpp:1155) ---------------------------
    // ⛔ NOT WIRED UP THIS WAVE, and deliberately NOT a silent {}. The X360 body is one
    // call, inlined at both of its sites (SetupComponents @0x824C9A74 and SetupCar
    // @0x824D81EC):
    //
    //     mCarsUnlocked.SetLocalisedText(KAC_CARS_AVAILABLE_STRINGID,
    //                                    LanguageManager::E_FORMAT_ID_LOOKUP,          // 9
    //                                    gsiNumCarouselCars,      E_FORMAT_INTEGER,    // 11
    //                                    gsiNumCarsUnlockedTotal, E_FORMAT_INTEGER);   // 11
    //
    // -- the two-integer-parameter TextField::SetLocalisedText overload (sub_824E7F78,
    // BrnTextField.cpp:418), which resolves through LanguageManager's two-value
    // FormatTextFromInt (sub_828659E8, CgsLanguageManager.cpp:1259) and thence through
    // LanguageManager::FormatText(char*, u32, s32, ParameterFormatType) for each
    // E_FORMAT_INTEGER parameter.
    //
    // That last leaf is `LanguageManager::FormatIntegerString`, and on this build it is an
    // ARMED __debugbreak() trap-stub (CgsLanguageManager.cpp:686, one of the FLAG trap-stub
    // block). Making the call would break into the debugger on the very screen this wave is
    // bringing up, every time a car is highlighted. Neither the TextField overload nor the
    // LanguageManager overload has a reconstructed body, so wiring it now would ALSO mean
    // inventing two formatters.
    //
    // The only visible consequence is that the "<n> of <m> cars available" caption stays
    // blank; nothing else on the screen reads mCarsUnlocked. Restore the call above verbatim
    // when FormatIntegerString lands.
    void CarSelectVehicle::SetupCarsUnlockedTextComponent()
    {
    }

    // ---- SetCarouselComponent @ 0x824BBE90 -----------------------------------------
    // Rebuild the five overview icons so that lCarId sits in the centre slot. The window
    // is clamped at both ends of the car list, so near the ends fewer than five icons are
    // bound and the rest stay empty.
    void CarSelectVehicle::SetCarouselComponent(CgsID lCarId)
    {
        // cpp:1191 -- the console asserts mpVehicleList here AND guards the whole body on it
        // (`lwz 0x28C / beq skip`), so the bail is the console's own; only the assert is
        // suppressed. ⚠️ PC-BUILD GUARD, see BrnCarSelectVehicle.h.
        if (mpVehicleList == 0)
            return;

        mCarouselOverviewSelectableGroup.muFlags =
            static_cast<u8>(mCarouselOverviewSelectableGroup.muFlags | SelectableGroup::KU_FLAG_QUERIED);

        // Blank every icon first.
        for (s32 liIcon = 0; liIcon < KI_NUMBER_VISIBLE_VEHICLE_ICONS; ++liIcon)
        {
            RivalTableCell& lrCell = maCarouselOverviewSelectable[liIcon];
            lrCell.SetCarID(0);
            lrCell.SetDriven(false);
            lrCell.SetWrecked(false);
            lrCell.SetEmpty(true);
            lrCell.SetDirty();
        }

        const s32 liNumCars = gsiNumCarouselCars;

        // Where is the selected car in the list? (Not found -> index 0.)
        s32 liSelectedIndex = 0;
        for (s32 liCar = 0; liCar < liNumCars; ++liCar)
        {
            if (maSelectedCars[liCar] == lCarId)
            {
                liSelectedIndex = liCar;
                break;
            }
        }

        // The window of icon slots that map onto real cars: slot 2 is the selected car, so
        // slot i shows car (liSelectedIndex + i - 2), clamped to [0, liNumCars).
        s32 liFirstIcon = 2 - liSelectedIndex;
        if (liFirstIcon < 0)
            liFirstIcon = 0;

        s32 liLastIcon = (liNumCars - liSelectedIndex) + 1;
        if (liLastIcon >= 4)
            liLastIcon = 4;

        if (liFirstIcon <= liLastIcon)
        {
            s32 liVehicleIndex = (liFirstIcon + liSelectedIndex) - 2;

            for (s32 liIcon = liFirstIcon; liIcon <= liLastIcon; ++liIcon, ++liVehicleIndex)
            {
                RivalTableCell& lrCell = maCarouselOverviewSelectable[liIcon];

                lrCell.SetEmpty(false);

                CGS_ASSERT(liVehicleIndex >= 0 && liVehicleIndex < liNumCars,
                           "carousel vehicle index out of range ");   // cpp:1239 (streamed form)

                lrCell.SetCarID(maSelectedCars[liVehicleIndex]);

                // Both bit arrays are indexed with the same assert the X360 inlines from
                // CgsBitArray.h:203 (`index < 128`).
                CGS_ASSERT(liVehicleIndex < 128, "invalid index : ");
                lrCell.SetDriven(maSelectedCarsDrivenState.IsBitSet(static_cast<u32>(liVehicleIndex)));

                CGS_ASSERT(liVehicleIndex < 128, "invalid index : ");
                lrCell.SetWrecked(maSelectedCarsWreckedState.IsBitSet(static_cast<u32>(liVehicleIndex)));
            }
        }

        SetSliderBarComponent();
    }

    // ---- SetSliderBarComponent (DWARF cpp:1264) ------------------------------------
    // Inlined at SetCarouselComponent's tail (the closing CarouselSliderBar::SetVisible
    // call at 0x824BC404..0x824BC418): the scroll bar only appears once the list is longer
    // than the three cars that fit without scrolling.
    void CarSelectVehicle::SetSliderBarComponent()
    {
        mCarouselSliderBar.SetVisible(gsiNumCarouselCars >= 3);
    }

    // ================================================================================
    // per-frame
    // ================================================================================

    // ---- UpdateComponents @ 0x824BBD18 ---------------------------------------------
    void CarSelectVehicle::UpdateComponents()
    {
        mCarSelector.Update();                          // component slot 5 (this class's override)
        mCarouselOverviewSelectableGroup.Update();      // component slot 5 (the base's)

        // Find the highlighted car's position in the list; when it is not in the list the
        // loop falls out with the index == the count (the X360 uses ONE register for both
        // the loop counter and the result).
        const s32 liNumCars = gsiNumCarouselCars;
        s32 liHighlightedIndex = 0;
        while (liHighlightedIndex < liNumCars)
        {
            CGS_ASSERT(mCarSelector.GetHighlighted() != 0, "GetHighlighted()");  // BrnSelectableGroup.h:218
            if (maSelectedCars[liHighlightedIndex] == mCarSelector.GetHighlighted()->GetId())
                break;
            ++liHighlightedIndex;
        }

        // ⓘ Positional, per the asm (r4 = the car count, r5 = the highlighted index). The
        // committed CarouselSliderBar::Update spells its parameters (liFirstItem,
        // liItemCount) -- the NAMES are transposed relative to every caller, but its own
        // extent maths (`liItemCount / (liFirstItem + 4)`) is right for this argument order.
        mCarouselSliderBar.Update(liNumCars, liHighlightedIndex);

        // The continue prompt disappears while a car change is still resolving.
        if (IsLoading())
        {
            mHelpItemContinue.SetItem(KAC_EMPTY, ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                                  ButtonIconComponent::E_PADBUTTON_INVISIBLE);
        }
        else
        {
            mHelpItemContinue.SetItem("$GENERAL_OPTION_CONTINUE",
                                      ButtonIconComponent::E_PADBUTTON_SELECT,
                                      ButtonIconComponent::E_PADBUTTON_INVISIBLE);
        }

        // Slide the five icons by the live carousel scroll offset.
        for (s32 liIcon = 0; liIcon < KI_NUMBER_VISIBLE_VEHICLE_ICONS; ++liIcon)
        {
            Vector2 lv2ScreenPosition;
            lv2ScreenPosition.SetZero();
            lv2ScreenPosition.x = mfCarouselXOffset + mafCarouselOriginalXPos[liIcon];
            maCarouselOverviewSelectable[liIcon].SetScreenPosition(lv2ScreenPosition);
        }
    }

    // ---- UpdateCarouselTransition @ 0x824D7D98 -------------------------------------
    // Advance the scrolling strip. While a decay is running the highlight is stepped one
    // car per KC_CAROUSEL_X_ADVANCE (165px) of travel; each step re-runs SetupCar for the
    // newly highlighted car. Returns whether the highlight moved.
    bool CarSelectVehicle::UpdateCarouselTransition()
    {
        bool lbStepped = false;

        // The quiet (lbQuiet == true) highlight probes only report whether a step is
        // POSSIBLE in that direction; the offset only accumulates when it is.
        if ((mfCarouselXOffsetDecay > 0.0f && mCarSelector.HighlightPrevious(true)) ||
            (mfCarouselXOffsetDecay < 0.0f && mCarSelector.HighlightNext(true)))
        {
            mfCarouselXOffset = mfCarouselXOffsetDecay + mfCarouselXOffset;
        }

        while (mfCarouselXOffset >= KC_CAROUSEL_X_ADVANCE)
        {
            lbStepped = true;

            bool lbClappers;
            if (mfCarouselXOffsetDecay != 0.0f)
            {
                // A decay was running: snap to the step and stop.
                mfCarouselXOffset      = 0.0f;
                mfCarouselXOffsetDecay = 0.0f;
                lbClappers             = false;
            }
            else
            {
                mfCarouselXOffset -= KC_CAROUSEL_X_ADVANCE;
                lbClappers         = true;
            }

            TriggerSound(lbClappers);
            mCarSelector.HighlightPrevious(false);

            CGS_ASSERT(mCarSelector.GetHighlighted() != 0, "GetHighlighted()");   // BrnSelectableGroup.h:218
            const bool lbCarChanged = (mCurrentSetupInfo.mCarId != mCarSelector.GetHighlighted()->GetId());

            CGS_ASSERT(mCarSelector.GetHighlighted() != 0, "GetHighlighted()");   // BrnSelectableGroup.h:218
            CarSetupInfo lSetupInfo;
            lSetupInfo.mCarId      = mCarSelector.GetHighlighted()->GetId();
            lSetupInfo.mbSelectable = IsCarSelectable(lSetupInfo.mCarId);

            SetupCar(&lSetupInfo, lbCarChanged);   // this class's vtable slot +0x64
        }

        while (mfCarouselXOffset <= -KC_CAROUSEL_X_ADVANCE)
        {
            lbStepped = true;

            bool lbClappers;
            if (mfCarouselXOffsetDecay != 0.0f)
            {
                mfCarouselXOffsetDecay = 0.0f;
                mfCarouselXOffset      = 0.0f;
                lbClappers             = false;
            }
            else
            {
                mfCarouselXOffset += KC_CAROUSEL_X_ADVANCE;
                lbClappers         = true;
            }

            TriggerSound(lbClappers);
            mCarSelector.HighlightNext(false);

            CGS_ASSERT(mCarSelector.GetHighlighted() != 0, "GetHighlighted()");   // BrnSelectableGroup.h:218
            const bool lbCarChanged = (mCurrentSetupInfo.mCarId != mCarSelector.GetHighlighted()->GetId());

            CGS_ASSERT(mCarSelector.GetHighlighted() != 0, "GetHighlighted()");   // BrnSelectableGroup.h:218
            CarSetupInfo lSetupInfo;
            lSetupInfo.mCarId      = mCarSelector.GetHighlighted()->GetId();
            lSetupInfo.mbSelectable = IsCarSelectable(lSetupInfo.mCarId);

            SetupCar(&lSetupInfo, lbCarChanged);
        }

        return lbStepped;
    }
}
