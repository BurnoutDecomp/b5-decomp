// ===================================================================================
// BrnGui::OnlineGameOptions -- wave-I partfile 06: the create-page presenter pair.
//   SetupHelpBar          @0x82485C78  cpp:1936 / cpp:1995 (asserts)
//   ShowGameOptionsScreen @0x8249C5C8
//
//
// The committed leaf header BrnOnlineGameOptions.h is still the MINIMAL pre-wave version
// (the GetResourcesToLoad inline plus the two resource statics), and BrnHelpBar.h still
// carries only GetItemNameHash/GetAnimator. The wave-I spec's §H1 class extension and §H3
// HelpBar declarations had not been applied when this partfile was written, and headers
// are frozen for implementers, so neither body can name meSubState / mHelpBar /
// mMenuOptions / mCreateGameToggles / mRouteInfoDisplay / the four animators / mTitleText /
// mpGuiCache / miCurrentRound / the KPC_* animation-state tables, nor call
// HelpBar::Clear / HelpBar::AppendHelpBarItem. Measured with the compile gate, not assumed.
//
// The complete partfile lives at, with a banner naming the exact declaration lines that
// unblock it:
// It is written as the finished contents of THIS file (one `namespace BrnGui { ... }`,
// the spec §7 include set) and drops in verbatim once §H1/§H3 land -- delete this banner
// and copy it over.
//
// stands in the §H1/§H3 declarations and #includes the real component headers compiles
// them clean through the same cl /c gate (and a deliberately misspelled TextField::SetText
// made that probe fail, so the probe genuinely compiles the bodies).
//
// SPEC CORRECTION for the conductor (measured): wave-I spec §3 says the help-bar pad-button
// ids 4/5/6 "have no ButtonIconComponent::EPadButton home in the tree". They do --
// GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h declares that enum, and 4/5/6 are
// exactly E_PADBUTTON_SELECT / E_PADBUTTON_BACK / E_PADBUTTON_OPTION0, with the append
// call's third argument 15 == E_PADBUTTON_INVISIBLE (the same {text, button, INVISIBLE}
// shape BrnOnlineGameRoomPlayerInfo's HelpItem::SetItem call sites use -- so that third
// file uses the real home; §H1's `HelpBarItem::meButton` could take the same type.
//
// LINK NOTE for the conductor: the two saved/received counters SetupHelpBar tests are
// cache+76672 / +76676 loads onto that type's own public accessors,
// OptionsDataProfile::GetNumCreatedOnlineGameOptions() / GetNumReceivedOnlineGameOptions().
// Both are DECLARED there (h:212/213) and DEFINED NOWHERE in the tree yet -- cl /c cannot
// see that, so it will surface only at link time.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEventWrapper / GuiEventQueueLarge
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface (out-queue)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"                      // BrnGui::OptionsDataProfile (the saved/received counters)
#include "GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkRouteInfo.h"  // BrnGui::GuiNetworkRouteInfo::EState
#include "GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h"          // BrnGui::ButtonIconComponent::EPadButton

namespace BrnGui
{

//
// BrnGui::OnlineGameOptions -- wave-I partfile 06: the create-page presenter pair.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   SetupHelpBar           @ 0x82485C78 (BrnOnlineGameOptions.cpp asserts at :1936 / :1995)
//   ShowGameOptionsScreen  @ 0x8249C5C8
//
// The class shape, the rodata tables and the sibling bodies live in the wave-I scaffold
// BrnOnlineGameOptions.{h,cpp} and the other BrnOnlineGameOptions_wI_XX.cpp partfiles.
//
// X360 offsets appear in comments only -- the host layout is name-based (pointers and
// component vptrs widen on x64, so no console offset, stride or record size is
// reproduced as a literal here). The one console byte count that does show up, the
// 24-byte id-213 view/internal-state record, is written as sizeof(the wrapper record):
// CgsGui::GuiEventWrapper is three s32 header words plus a pointer-free 12-byte
// payload, so the host size equals the console size.
//
// HEADER-CLASH NOTE (wave-H BrnOnlineGameRoomPlayerInfo_wH_09.cpp precedent): the id-213
// payload's real home is BrnGuiDemangledEventTypes.h, which hard-collides with
// BrnGuiEventTypeDefs.h (pulled in through BrnGuiCache.h). The two view-state records
// this file posts therefore use the same file-local payload view wH_09 uses.
//
// SPEC CORRECTION (measured, not reasoned): wave-I spec §3 says the help-bar pad-button
// ids "have no ButtonIconComponent::EPadButton home in the tree". They do --
// GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h declares the enum, and its
// values line up exactly with the three ids this function stores (4 == SELECT,
// 5 == BACK, 6 == OPTION0) and with the 15 the append call passes for its third
// argument (15 == E_PADBUTTON_INVISIBLE -- the same {text, button, INVISIBLE} shape
// BrnOnlineGameRoomPlayerInfo's HelpItem::SetItem calls use). No local id enum is
// forked; the real home is included.
// ===================================================================================
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) ---------------------------
        const s32 KI_CHANNEL_VIEW_STATE   = 41;   // OutputViewState
        const s32 KI_CHANNEL_GUI_INTERNAL = 42;   // OutputInternalState

        // The apt view-state key every animation component on this screen is driven with.
        const char KPC_APT_TRANSITION[] = "apt_Transition";

        // ---- help-bar items: the class's own KA_HELPBAR_ITEMS table is the home ---------
        // MEASURED (headless IDA): KA_HELPBAR_ITEMS @0x8205F1B4 is four
        // {const char*, s32} records --
        //   [0] {"$CAPS_BUTTON_BACK_UP",  5 == E_PADBUTTON_BACK}
        //   [1] {"$CAPS_BUTTON_CONTINUE", 4 == E_PADBUTTON_SELECT}
        //   [2] {"$CAPS_BUTTON_LOAD",     4 == E_PADBUTTON_SELECT}
        //   [3] {"$CAPS_BUTTON_LOAD",     6 == E_PADBUTTON_OPTION0}
        // -- which are precisely and only the four pairs SetupHelpBar stores at
        // 0x82485CD4..0x82485D70, in the order 2 / 1 / 3 / 0. The table itself has ZERO
        // xrefs in the whole image and SetupHelpBar reaches the strings through
        // `lis aCapsButtonLoad@ha` with the button ids as `li` immediates, so nothing is
        // read from 0x8205F1B4 at run time. The reading that fits both facts is that the
        // X360 compiler constant-folded the element reads (the array is a same-TU const
        // with a visible initialiser, defined at DWARF BrnOnlineGameOptions.cpp:223) and
        // still emitted the array because a class static has external linkage. That is an
        // inference from the fold, not something the asm states -- but it is the only
        // account of why an otherwise unreferenced rodata table carries exactly these four
        // pairs, so the pairs are sourced from the table rather than forked as file-local
        // literals. Emitted values are byte-identical either way. NOTE FOR A LATER SWEEP:
        // "no xrefs" is NOT evidence that KA_HELPBAR_ITEMS is dead -- do not drop it.

        // ---- out-queue payload view -----------------------------------------------------
        // GuiEventShowHideSatNav (wire id 213, payload 12 bytes). Its real home cannot be
        // included here (see the banner) and models the type as an empty struct, so the
        // three payload words are named as wave-H named them. FLAG: word roles not
        // recovered; the receiving CustomRendererManager keys event 213 by a sub-mode word
        // and a renderable flag.
        struct GuiEventShowHideSatNavPayload
        {
            s32  miSubMode;    // +0x00  X360 stores 0
            f32  mfValue;      // +0x04  X360 stores 0.0f (flt_82001CC0)
            bool mbFlag;       // +0x08  X360 stores 0 here (ShowLoadScreen stores 1)
            u8   maPad[3];     // +0x09  never written by the X360; modelled zeroed

            GuiEventShowHideSatNavPayload(s32 liSubMode, f32 lfValue, bool lbFlag)
                : miSubMode(liSubMode), mfValue(lfValue), mbFlag(lbFlag)
            {
                maPad[0] = maPad[1] = maPad[2] = 0;
            }

            s32 GetEventType() const { return 213; }
        };
    }

    // ================================================================================
    //  SetupHelpBar  @ 0x82485C78  (BrnOnlineGameOptions.cpp, asserts at :1936 / :1995)
    //
    //  Rebuild the bottom help bar for whichever page is currently up. The load page
    //  offers only "load"; the create page offers "continue", plus a second "load"
    //  prompt when the player actually has saved or received option sets to load from.
    //  Both pages always end with the back-up prompt.
    //
    //  The DWARF signature carries a bool parameter and both call sites pass 0, but the
    //  X360 body never reads r4 -- the flag is dead on this build.
    // ================================================================================
    void OnlineGameOptions::SetupHelpBar(bool /*lbUnused*/)
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1936 (non-fatal)

        // The X360 builds two parallel stack arrays (buttons then texts, both walked with
        // a word-scaled index) rather than an array of the class's HelpBarItem pairs.
        const char*                     lapcHelpBarText[KI_MAX_HELP_BAR_ITEMS];
        ButtonIconComponent::EPadButton laeHelpBarButton[KI_MAX_HELP_BAR_ITEMS];
        s32                             liHelpBarItemCount;

        // The X360 tests the common create-page sub-state first (`cmpwi 2 / beq`) and only
        // then the load sub-state; the two compares collapse to this single test because
        // the create arm and the fall-through arm are the same block.
        //
        // The static_cast on every meButton read is the header's doing, not the data's:
        // BrnOnlineGameOptions.h models HelpBarItem::meButton as s32 while the DWARF types
        // it BrnGui::ButtonIconComponent::EPadButton. The values are the enumerators.
        if (meSubState == E_SUBSTATE_LOAD_OPTIONS)
        {
            lapcHelpBarText[0]  = KA_HELPBAR_ITEMS[2].mpcText;    // "$CAPS_BUTTON_LOAD"
            laeHelpBarButton[0] = static_cast<ButtonIconComponent::EPadButton>(
                                      KA_HELPBAR_ITEMS[2].meButton);   // 4 SELECT
            liHelpBarItemCount  = 1;
        }
        else
        {
            lapcHelpBarText[0]  = KA_HELPBAR_ITEMS[1].mpcText;    // "$CAPS_BUTTON_CONTINUE"
            laeHelpBarButton[0] = static_cast<ButtonIconComponent::EPadButton>(
                                      KA_HELPBAR_ITEMS[1].meButton);   // 4 SELECT
            liHelpBarItemCount  = 1;

            // The two counters live in the cache's player-options profile block: the X360
            // folds both the profile fetch and the two counter reads into single
            // displacements off the cache pointer (cache+76672 / +76676 == the profile at
            // cache+0xB878 plus its +0x7308 / +0x730C). Un-inlined back onto the profile's
            // own public accessors rather than poking the (private) members.
            const OptionsDataProfile* lpOptionsDataProfile = mpGuiCache->GetOptionsDataProfile();
            if (lpOptionsDataProfile->GetNumCreatedOnlineGameOptions() > 0 ||
                lpOptionsDataProfile->GetNumReceivedOnlineGameOptions() > 0)
            {
                lapcHelpBarText[1]  = KA_HELPBAR_ITEMS[3].mpcText;    // "$CAPS_BUTTON_LOAD"
                laeHelpBarButton[1] = static_cast<ButtonIconComponent::EPadButton>(
                                          KA_HELPBAR_ITEMS[3].meButton);   // 6 OPTION0
                liHelpBarItemCount  = 2;
            }
        }

        // The back-up prompt is written at the running count and only then counted in --
        // which is why the bound check below runs on the incremented count.
        lapcHelpBarText[liHelpBarItemCount]  = KA_HELPBAR_ITEMS[0].mpcText;  // "$CAPS_BUTTON_BACK_UP"
        laeHelpBarButton[liHelpBarItemCount] = static_cast<ButtonIconComponent::EPadButton>(
                                                   KA_HELPBAR_ITEMS[0].meButton);   // 5 BACK
        ++liHelpBarItemCount;

        CGS_ASSERT(liHelpBarItemCount <= KI_MAX_HELP_BAR_ITEMS,
                   "liHelpBarItemCount <= KI_MAX_HELP_BAR_ITEMS");   // cpp:1995 (non-fatal)

        mHelpBar.Clear();   // this+0x5C40
        for (s32 liItem = 0; liItem < liHelpBarItemCount; ++liItem)
        {
            // Third argument 15: the append's optional SECOND button icon, left invisible
            // (E_PADBUTTON_INVISIBLE). The callee asserts both button arguments < 0x10.
            // Both are ButtonIconComponent::EPadButton in the DWARF signature (s32 return,
            // BrnHelpBar.h:103), so the enumerators go through untouched; the s32 return is
            // the appended item's index and the X360 discards it here.
            mHelpBar.AppendHelpBarItem(lapcHelpBarText[liItem],
                                       laeHelpBarButton[liItem],
                                       ButtonIconComponent::E_PADBUTTON_INVISIBLE);
        }
    }

    // ================================================================================
    //  ShowGameOptionsScreen  @ 0x8249C5C8
    //
    //  Bring up the create-match page: drop the load-slot menu, re-arm the five-row
    //  create-game toggle group, hide the route-info panel, rebuild the option toggles
    //  from the common option list and re-highlight them from the current match
    //  parameters, then push the page's title, its help bar and its apt view states.
    // ================================================================================
    void OnlineGameOptions::ShowGameOptionsScreen()
    {
        meSubState = E_SUBSTATE_SELECTING_PARAMS;   // this+0x38

        mMenuOptions.Clear();                                            // this+0x40, group vtable slot 6
        mCreateGameToggles.SetupGroup(KI_MAX_CREATE_GAME_OPTIONS, false); // this+0x1100
        mRouteInfoDisplay.SetState(GuiNetworkRouteInfo::E_STATE_INVISIBLE); // this+0x7F10, X360 `li r4, 1`

        SetupCommonCreateGameOptions();
        HighlightCreateGameOptions();
        SetupHelpBar(false);   // X360 `li r4, 0` -- the dead flag

        mTitleText.SetText(KAC_GAME_OPTIONS_TITLE_STRING_ID);   // this+0x7DE8
        miCurrentRound = 0;                                     // this+0xA508

        // The id-213 show/hide record { payload size 12, type 213, payload offset 12,
        // { 0, 0.0f, false } }, published on the view-state channel and then again on the
        // internal-state channel (X360 record size 24 both times).
        CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpOutQueue =
            mpStateInterface->GetOutputEventQueue();

        GuiEventShowHideSatNavPayload lShowHideSatNav(0, 0.0f, false);

        CgsGui::GuiEventWrapper<GuiEventShowHideSatNavPayload, KI_CHANNEL_VIEW_STATE>
            lViewStateRecord(lShowHideSatNav);
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lViewStateRecord),
                             lViewStateRecord.GetChannel(),
                             static_cast<s32>(sizeof(lViewStateRecord)));

        CgsGui::GuiEventWrapper<GuiEventShowHideSatNavPayload, KI_CHANNEL_GUI_INTERNAL>
            lInternalStateRecord(lShowHideSatNav);
        lpOutQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lInternalStateRecord),
                             lInternalStateRecord.GetChannel(),
                             static_cast<s32>(sizeof(lInternalStateRecord)));

        // The load page's furniture goes away with the page (this+0x7BA8 / this+0x7C34).
        mLoadHeaderAnimator.AddOutputAptViewState(KPC_APT_TRANSITION,
                                                  KPC_LOAD_HEADER_ANIMATION_STATES[0], false);
        mMapBorderAnimator.AddOutputAptViewState(KPC_APT_TRANSITION,
                                                 KPC_MAP_BORDER_ANIMATION_STATES[0], false);

        // The scroll arrows only appear when there is more than one option group to scroll
        // through. The X360 drives the DOWN arrow first (this+0x7B1C), then the UP arrow
        // (this+0x7A90), and reloads the same state string for the second call.
        if (GetNumberOptions() > 1)
        {
            mDownArrowAnimator.AddOutputAptViewState(KPC_APT_TRANSITION,
                                                     KPC_ARROW_ANIMATION_STATES[1], false);
            mUpArrowAnimator.AddOutputAptViewState(KPC_APT_TRANSITION,
                                                   KPC_ARROW_ANIMATION_STATES[1], false);
        }
        else
        {
            mDownArrowAnimator.AddOutputAptViewState(KPC_APT_TRANSITION,
                                                     KPC_ARROW_ANIMATION_STATES[0], false);
            mUpArrowAnimator.AddOutputAptViewState(KPC_APT_TRANSITION,
                                                   KPC_ARROW_ANIMATION_STATES[0], false);
        }
    }
}
