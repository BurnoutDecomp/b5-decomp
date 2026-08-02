// ===================================================================================
// PARKED (wave-I group 08). This is the ready-to-drop-in partfile for
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions_wI_08.cpp
// It is BLOCKED only because the frozen leaf header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h
// is still the 30-line minimal version: spec §H1 (the full class extension) and §H2
// (the CreateMatchOption element/enum the extended header names) have NOT been applied.
// Every compile error the real gate reports is "X is not a member of
// BrnGui::OnlineGameOptions" / "undeclared identifier" for an §H1 declaration.
//
// PROOF THIS COMPILES: an overlay copy of the header with §H1+§H2 applied lives at
//   scratchpad/waveI/probe08/GameSource/Gui/Flow/Screen/States/{BrnOnlineGameOptions,
//   BrnCreateMatchOption}.h
// and `scratchpad/waveI/probe08/run_probe.py` runs the repo's own compile gate with that
// directory prepended to the include path. Result: PROBE_STATUS=pass (cl /c clean).
// The faithfulness lint is clean on this file too (0 new findings).
//
// EXACT DECLARATIONS THAT UNBLOCK THIS FILE (all inside
// `struct BrnGui::OnlineGameOptions : public CgsGui::State`, per spec §H1 -- plus the
// §H1 include set and §H2's CreateMatchOption::EOption, which §H1's own member/method
// rows reference):
//     enum EOptionsToLoad { E_OPTIONS_TO_LOAD_SAVED = 0, E_OPTIONS_TO_LOAD_RECENT = 1,
//                           E_OPTIONS_TO_LOAD_COUNT = 2 };
//     enum ESubState { E_SUBSTATE_LOADING_SCREEN = 0, E_SUBSTATE_LOADING_COMPONENTS = 1,
//                      E_SUBSTATE_SELECTING_PARAMS = 2, E_SUBSTATE_LOAD_OPTIONS = 3,
//                      E_SUBSTATE_WAIT_IN_GAME = 4, E_SUBSTATE_COUNT = 5 };
//     static const s32 KI_MAX_LOAD_OPTIONS = 6;
//     void StoreCreateGameOptions();
//     void SetupHelpBar(bool lbUnused);
//     void ShowLoadScreen();
//     void ShowLoadOptions();
//     static const char KAC_LOAD_OPTIONS_TITLE_STRING_ID[32];
//     static const char KAC_CREATED_OPTIONS_STRING_ID[28];
//     static const char KAC_RECENT_OPTIONS_STRING_ID[27];
//     static const char KPC_SLOT_STRING_FORMAT_ID[24];
//     static const char KPC_SLOT_STRING_ID[28];
//     static const char* const KPC_ARROW_ANIMATION_STATES[3];
//     static const char* const KPC_LOAD_HEADER_ANIMATION_STATES[3];
//     static const char* const KPC_MAP_BORDER_ANIMATION_STATES[2];
//     ESubState                 meSubState;
//     MenuComponent             mMenuOptions;
//     MenuToggleGroupVarSize<5> mCreateGameToggles;
//     AnimationComponent        mUpArrowAnimator;
//     AnimationComponent        mDownArrowAnimator;
//     AnimationComponent        mLoadHeaderAnimator;
//     AnimationComponent        mMapBorderAnimator;
//     TextField                 mLoadHeaderText;
//     TextField                 mTitleText;
//     GuiNetworkRouteInfo       mRouteInfoDisplay;
//     GuiCache*                 mpGuiCache;
//     EOptionsToLoad            meOptionsToLoad;
//     s32                       miCurrentRound;
//     s32                       miStartItem;
//
// NOTE FOR THE CONDUCTOR: this file only READS the KAC_/KPC_ class statics; it does not
// DEFINE them (defining them here would collide at link with whichever sibling partfile
// the consolidation gives them to). Exactly one .cpp in the consolidated TU must carry
// the spec §H1 static definitions.
//
// The two per-function splits of this file (identical bodies, standalone TUs, both
// probe-compiled) are BrnOnlineGameOptions_08_ShowLoadScreen.cpp and
// BrnOnlineGameOptions_08_ShowLoadOptions.cpp in this directory.
// ===================================================================================
//
// BrnGui::OnlineGameOptions -- wave-I partfile 08: the load-page pair.
//   ShowLoadScreen   @0x8249C760  cpp:2078 (assert)
//   ShowLoadOptions  @0x8248C878  cpp:2151 (assert)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode arbitrated by
// the raw asm). ShowLoadScreen commits whatever the create page was editing, flips the
// state machine into the load sub-state and dresses the 6-slot save-slot page; it then
// hands the per-slot text/enable work to ShowLoadOptions, which also primes the route
// preview panel from save-route [0].
//
// VTABLE SLOT -> METHOD, DUMPED not inferred. Both functions dispatch through the
// MenuComponent component vtable @0x82074068 (this partfile's dump, via idat on
// BURNOUT_X360_ARTIST.XEX -- scratchpad/waveI/ogo_vt.txt):
//     +0x18 (slot  6)  MenuComponent::Clear            (the toggle-group face on <5>)
//     +0x20 (slot  8)  SelectableGroup::Enable   @0x824E3490
//     +0x24 (slot  9)  SelectableGroup::Disable  @0x824E3620
//     +0x30 (slot 12)  SelectableGroup::HighlightIndex @0x824E3CE0
// The wave-I spec hedged the empty-slot call toward MenuComponent::DisableSelectable
// (@0x824E2E38); the dumped table says slot 9 is SelectableGroup::Disable, which is a
// DIFFERENT function (Disable clears the Active flag and asserts the row is not the last
// enabled one; DisableSelectable clears Highlightable+Selectable). The dump is the
// authority, so ShowLoadOptions calls Enable/Disable.
//
// HOST-vs-CONSOLE. Every X360 immediate in these two bodies is either a *count* (6 slots,
// 128-byte text buffers, the 41/42 channel selectors, the localisation format words 0/9/11)
// or a *member offset the console inlined* (0x12B80/0x12B84 == the created/received option
// counters at GuiCache+0xB878+0x7308/+0x730C; +0x8 / +0x3988 == maCreated/maReceived[0]).
// None of the offsets are written here: the inlined GuiCache::GetOptionsDataProfile() and
// the four OptionsDataProfile accessors are restored as real calls (inlining reversal --
// their "lpGuiCache" asserts at BrnGuiOptionsDataProfile.h:611/632 are exactly what the
// X360 inlined into ShowLoadOptions). The one AddEvent size is the host sizeof of the
// record type, not the console's 24.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue / language manager
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // LanguageManager::FormatAndAddText
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // GuiCache::GetOptionsDataProfile
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"                      // OptionsDataProfile online-save-route accessors
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"           // the 480-byte params the route fills

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------
        const s32 KI_CHANNEL_VIEW_STATE   = 41;   // OutputViewState
        const s32 KI_CHANNEL_GUI_INTERNAL = 42;   // internal/HUD-component channel

        // The apt view-state animation channel every screen component drives.
        const char KAC_APT_TRANSITION[] = "apt_Transition";

        // The "%d" the slot-number parameter is printed through before the localisation
        // manager formats it into the slot label.
        const char KAC_NUMBER_FORMAT[] = "%d";

        // The source text an EMPTY save slot is given: a single space, formatted raw.
        const char KAC_EMPTY_SLOT_TEXT[] = " ";

        // Both SPrintf targets are 128-byte stack buffers on the X360 (`li r4, 0x80`).
        const u32 KU_SLOT_TEXT_BUFFER_SIZE = 128;

        // Id 213 == BrnGui::GuiEventShowHideSatNav (BrnGuiDemangledEventTypes.h:371), which
        // hard-collides with BrnGuiEventTypeDefs.h, so it is carried here as a file-local
        // wire view -- the same 24-byte { 12, 213, 12, s32, f32, u8 } record
        // BrnFBurnMainHudState.cpp's GuiShowHideEvent24 and BrnOnlineGameRoomPlayerInfo_wH_01
        // post. ShowLoadScreen publishes it with the trailing flag SET (X360 `stb r25(1)`),
        // where the create page publishes the same record with the flag clear.
        typedef CgsGui::GuiEvent<213> ShowHideHeader;
        struct GuiEventShowHideWire : public ShowHideHeader
        {
            s32 miShow;      // +0x0C
            f32 mfValue;     // +0x10
            u8  mu8Flag;     // +0x14
            u8  maPad[3];    // +0x15 (the record is 24 bytes on the wire)

            GuiEventShowHideWire(s32 liShow, f32 lfValue, u8 lu8Flag)
                : ShowHideHeader(static_cast<u32>(sizeof(GuiEventShowHideWire) -
                                                  sizeof(ShowHideHeader)),
                                 static_cast<u32>(sizeof(ShowHideHeader)))
                , miShow(liShow)
                , mfValue(lfValue)
                , mu8Flag(lu8Flag)
            {
                maPad[0] = 0;
                maPad[1] = 0;
                maPad[2] = 0;
            }
        };

        // Record-size pin. The payload is pointer-free, so the X360's AddEvent size
        // immediate must survive unchanged on the x64 host.
        static_assert(sizeof(GuiEventShowHideWire) == 24, "show/hide record is 24 bytes");
    }

    // ---- ShowLoadScreen @ 0x8249C760 -----------------------------------------------
    // Leave the create page (committing its toggles) and bring the load page up: pick the
    // save-slot family that actually has entries -- the player's own saved options when
    // there are any, otherwise the received/recent ones -- title the list accordingly,
    // rebuild the 6-slot menu through ShowLoadOptions, and put the page's help bar,
    // scroll arrows and view-state up.
    void OnlineGameOptions::ShowLoadScreen()
    {
        StoreCreateGameOptions();

        meSubState = E_SUBSTATE_LOAD_OPTIONS;

        // The X360 inlines the cache's profile fetch and reads the two counters straight
        // out of the far block; restored as the real accessor chain.
        OptionsDataProfile* lpOptionsData = mpGuiCache->GetOptionsDataProfile();

        s32         liNumRoutes;
        const char* lpacHeaderTextId;

        if (lpOptionsData->GetNumCreatedOnlineGameOptions() > 0)
        {
            liNumRoutes      = lpOptionsData->GetNumCreatedOnlineGameOptions();
            lpacHeaderTextId = KAC_CREATED_OPTIONS_STRING_ID;
            meOptionsToLoad  = E_OPTIONS_TO_LOAD_SAVED;
        }
        else
        {
            // cpp:2078 -- non-fatal; the X360 falls straight through into the recent list.
            CGS_ASSERT(lpOptionsData->GetNumReceivedOnlineGameOptions() > 0,
                       "mpGuiCache->GetMenuOptionsData()->GetNumReceivedOnlineGameOptions() > 0");

            liNumRoutes      = lpOptionsData->GetNumReceivedOnlineGameOptions();
            lpacHeaderTextId = KAC_RECENT_OPTIONS_STRING_ID;
            meOptionsToLoad  = E_OPTIONS_TO_LOAD_RECENT;
        }

        mLoadHeaderText.SetText(lpacHeaderTextId);

        // The header only offers its "switch list" buttons when BOTH families have
        // entries to switch between.
        const char* lpacLoadHeaderState =
            ((lpOptionsData->GetNumCreatedOnlineGameOptions() <= 0) ||
             (lpOptionsData->GetNumReceivedOnlineGameOptions() <= 0))
                ? KPC_LOAD_HEADER_ANIMATION_STATES[1]    // "withoutbuttons"
                : KPC_LOAD_HEADER_ANIMATION_STATES[2];   // "withbuttons"

        mLoadHeaderAnimator.AddOutputAptViewState(KAC_APT_TRANSITION, lpacLoadHeaderState, false);
        mMapBorderAnimator.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                 KPC_MAP_BORDER_ANIMATION_STATES[1], false);   // "visible"

        miStartItem = 0;
        mCreateGameToggles.Clear();

        mMenuOptions.SetupMenu(KI_MAX_LOAD_OPTIONS, false);
        ShowLoadOptions();
        mMenuOptions.HighlightIndex(0);

        mTitleText.SetText(KAC_LOAD_OPTIONS_TITLE_STRING_ID);
        SetupHelpBar(false);

        // A single-entry list cannot scroll, so both arrows go away; otherwise both show.
        // (X360 order: the DOWN animator first, then the UP one.)
        const char* lpacArrowState = (liNumRoutes <= 1) ? KPC_ARROW_ANIMATION_STATES[0]    // "invisible"
                                                        : KPC_ARROW_ANIMATION_STATES[1];   // "visible"
        mDownArrowAnimator.AddOutputAptViewState(KAC_APT_TRANSITION, lpacArrowState, false);
        mUpArrowAnimator.AddOutputAptViewState(KAC_APT_TRANSITION, lpacArrowState, false);

        // The view-state pair: the same record on the view-state channel and then on the
        // internal one, with the trailing flag set (the load page's form of the record).
        const GuiEventShowHideWire lShowHide(0, 0.0f, 1);

        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lShowHide), KI_CHANNEL_VIEW_STATE,
            static_cast<s32>(sizeof(lShowHide)));
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lShowHide), KI_CHANNEL_GUI_INTERNAL,
            static_cast<s32>(sizeof(lShowHide)));
    }

    // ---- ShowLoadOptions @ 0x8248C878 ----------------------------------------------
    // Fill the six visible save-slot rows from the currently selected save-route family,
    // starting at the scroll window's first item: a slot backed by a route gets the
    // localised "Slot <n>" label and is enabled, a slot past the end gets a blank label
    // and is disabled. Then reset the round cursor and preview route [0] in the route
    // info panel.
    void OnlineGameOptions::ShowLoadOptions()
    {
        OptionsDataProfile* lpOptionsData = mpGuiCache->GetOptionsDataProfile();

        s32 liNumRoutes;
        if (meOptionsToLoad == E_OPTIONS_TO_LOAD_SAVED)
        {
            liNumRoutes = lpOptionsData->GetNumCreatedOnlineGameOptions();
        }
        else
        {
            // cpp:2151 -- non-fatal; the X360 reads the received counter either way.
            CGS_ASSERT(meOptionsToLoad == E_OPTIONS_TO_LOAD_RECENT,
                       "meOptionsToLoad == E_OPTIONS_TO_LOAD_RECENT");

            liNumRoutes = lpOptionsData->GetNumReceivedOnlineGameOptions();
        }

        char lacSlotId[KU_SLOT_TEXT_BUFFER_SIZE];
        char lacNumber[KU_SLOT_TEXT_BUFFER_SIZE];

        for (s32 liSlot = 0; liSlot < KI_MAX_LOAD_OPTIONS; ++liSlot)
        {
            const s32 liRouteIndex = miStartItem + liSlot;

            // "$ONLINE_GAME_OPTION_SLOT_<slot>" -- keyed by the VISIBLE row, not the route.
            CgsCore::SPrintf(lacSlotId, KU_SLOT_TEXT_BUFFER_SIZE, KPC_SLOT_STRING_ID, liSlot);

            // The label is registered under lacSlotId + 1: dropping the leading '$' makes
            // the added string's id match the '$'-prefixed lookup MenuComponent::SetText
            // performs below.
            if (liRouteIndex < liNumRoutes)
            {
                CgsCore::SPrintf(lacNumber, KU_SLOT_TEXT_BUFFER_SIZE, KAC_NUMBER_FORMAT,
                                 liRouteIndex + 1);

                mpStateInterface->GetLanguageManager()->FormatAndAddText(
                    lacSlotId + 1, KPC_SLOT_STRING_FORMAT_ID,
                    CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 1,
                    lacNumber, CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
            }
            else
            {
                mpStateInterface->GetLanguageManager()->FormatAndAddText(
                    lacSlotId + 1, KAC_EMPTY_SLOT_TEXT,
                    CgsLanguage::LanguageManager::E_FORMAT_TEXT);
            }

            mMenuOptions.SetText(liSlot, lacSlotId);

            // Component vtable slot 8 / slot 9 (dumped -- see the banner).
            if (liRouteIndex < liNumRoutes)
            {
                mMenuOptions.Enable(liSlot);
            }
            else
            {
                mMenuOptions.Disable(liSlot);
            }
        }

        miCurrentRound = 0;
        mRouteInfoDisplay.SetState(GuiNetworkRouteInfo::E_STATE_VISIBLE);

        // Preview the first route of the selected family. The X360 inlined both profile
        // accessors (their "lpGuiCache" asserts are what survives at
        // BrnGuiOptionsDataProfile.h:611 and :632); restored as the real calls.
        GuiEventNetworkGameParams lParams;
        if (meOptionsToLoad == E_OPTIONS_TO_LOAD_SAVED)
        {
            lpOptionsData->GetCreatedOnlineGameOptions(0, mpGuiCache, &lParams);
        }
        else
        {
            lpOptionsData->GetReceivedOnlineGameOptions(0, mpGuiCache, &lParams);
        }

        mRouteInfoDisplay.SetInfo(miCurrentRound, &lParams);
    }
}
