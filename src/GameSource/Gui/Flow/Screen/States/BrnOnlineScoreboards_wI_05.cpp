// ===================================================================================
// BrnGui::OnlineScoreboards -- wave-I partfile 05: the screen's enter/leave pair plus
// the one-shot component refresh.
//   UpdateSetupComponents @0x824918C8  (DWARF cpp:519)
//   OnEnter               @0x8249F1D0  (DWARF cpp:224)
//   OnLeave               @0x8249F3B0  (DWARF cpp:383)
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface: Register/UnRegister/PlayAptMovie/GetOutputEventQueue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiEventActivateCrashNav (id 191)
#include <cstring>                                                        // std::memset

namespace BrnGui
{

    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;   // X360 `li r5, 0x28`

        // The number of filter toggles this screen's group carries (X360 `li r6, 3` into
        // MenuToggleGroupVarSize<3>::Construct and `li r4, 3` into SetupGroup) -- the
        // category / filter / road columns. It is an item COUNT, not a layout value.
        const s32 KI_NUM_FILTER_TOGGLES = 3;

        // The level the screen's apt movie is mounted at; OnLeave clears that level by
        // asking for the empty movie name (X360 `li r11, 3` in the channel-41 record).
        const s32 KI_APT_MOVIE_LEVEL = 3;

        // The name OnLeave passes to clear the level-3 apt movie. FLAG: the X360 loads the
        // pooled rodata literal @0x820046A7, which the committed twins (BrnCarSelectMain,
        // BrnCarSelectUnlock, BrnBootLegal) all reconstruct as the empty string.
        const char KAC_NO_APT_MOVIE[] = "";

        // ---- out-queue wire record -----------------------------------------------------
        // Id 148 == BrnGui::GuiEventShowHideHud (payload size 1). Its only home is
        // BrnGuiDemangledEventTypes.h, which hard-collides with BrnGuiEventTypeDefs.h (this
        // file needs the latter for GuiEventActivateCrashNav), so the record is carried as a
        // file-local wire view -- the same accommodation BrnOnlineGameRoomPlayerInfo_wH_00 /
        // _wH_07 / _wH_08 and BrnCrashNavEnterOnline_wI_07 make.
        //
        // OnEnter posts it with the flag CLEAR (X360 `li r11, 0` then `stb r11` into the
        // payload slot): entering the leaderboards screen takes the HUD down.
        typedef CgsGui::GuiEvent<148> ShowHideHudHeader;
        struct GuiEventShowHideHudWire : public ShowHideHudHeader
        {
            bool mbShowHud;   // +0x0C

            explicit GuiEventShowHideHudWire(bool lbShowHud)
                : ShowHideHudHeader(static_cast<u32>(sizeof(bool)),
                                    static_cast<u32>(sizeof(ShowHideHudHeader)))
                , mbShowHud(lbShowHud)
            {
            }
        };

        // Record-size pins. Both payloads are pointer-free, so the X360's AddEvent size
        // immediates (20 and 16) must survive unchanged on the x64 host; if a host layout
        // ever drifts, it is caught here rather than on the wire.
        static_assert(sizeof(GuiEventActivateCrashNav) == 20, "activate-crashnav record is 20 bytes");
        static_assert(sizeof(GuiEventShowHideHudWire) == 16, "show/hide-hud record is 16 bytes");
    }

    // ---- UpdateSetupComponents @ 0x824918C8 ----------------------------------------
    // The one-shot refresh the screen runs once the leaderboard request chain has produced
    // every list it needs: rebuild the three filter toggles from the freshly-filled name
    // tables, restore the filter row's highlight, redraw the table and re-advertise the
    // button prompts that match what is now on screen.
    void OnlineScoreboards::UpdateSetupComponents()
    {
        SetupCategories();
        SetupIndexes();
        SetupVariations();

        // X360 `(*(*(this + 0xE08) + 0x30))(this + 0xE08, *(this + 0xDE8))` -- component
        // vtable slot 12 on the toggle group, which is SelectableGroup::HighlightIndex
        // (BrnSelectableGroup.h's slot map).
        mFilterToggleGroup.HighlightIndex(miCurrentFilterHighlighted);

        mTable.DrawScoreboard();

        SetupButtons();
    }

    // ---- OnEnter @ 0x8249F1D0 ------------------------------------------------------
    // Bring the leaderboards screen up: observe the twelve events it lives on, wipe every
    // list the request chain will refill, construct the five apt components, reset the
    // per-visit view state, take the CrashNav map and the HUD down, and pre-sort the road
    // names so the per-road filter can be presented alphabetically.
    void OnlineScoreboards::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        meInternalState = E_INTERNALSTATE_GETCACHE;
        mpGuiCache      = 0;

        // The three name tables are cleared with their HOST byte counts; the X360 XMemSet
        // immediates (465 == 15*31, 310 == 10*31, 2046 == 66*31) are the console's own
        // sizeof of the same char arrays and are documentary only.
        miMaxCategories = 0;
        std::memset(maacCategories, 0, sizeof(maacCategories));

        miMaxIndexes = 0;
        std::memset(maacIndexes, 0, sizeof(maacIndexes));

        miMaxVariations = 0;
        std::memset(maacVariations, 0, sizeof(maacVariations));

        meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_GET_CATEGORIES;

        // The filter row: three toggles, no parent component, and the "no id" sentinel for
        // the group's apt id. The X360 builds that sentinel as `li r8, -1 ; clrldi r8, r8, 32`
        // -- 0x00000000FFFFFFFF, NOT an all-ones 64-bit word, which is exactly
        // SelectableGroup::KU64_NO_ID. (Writing static_cast<u64>(-1) here would widen the
        // console value into a different id on the x64 host.)
        mFilterToggleGroup.Construct(KAC_FILTER_GROUP_NAME, mpStateInterface,
                                     KI_NUM_FILTER_TOGGLES, 0, SelectableGroup::KU64_NO_ID);
        mFilterToggleGroup.SetupGroup(KI_NUM_FILTER_TOGGLES, 0);

        // The remaining four components, in the X360's Construct order. All are parented to
        // the movie root (null parent name); the loading icon is given no state-identifier
        // table, so its initial "apt_state" push is suppressed. The two animators go through
        // the GuiComponent base Construct (the X360 dispatches both through component vtable
        // slot 0, as it does for the table).
        mTable.Construct(KAC_LEADERBOARD_TABLE_NAME, mpStateInterface, 0);
        mLoadingIcon.Construct(KAC_LOADING_ICON_NAME, mpStateInterface, 0, 0);
        mButtonAnimator.Construct(KAC_BUTTON_ANIMATOR_NAME, mpStateInterface, 0);
        mTableEmptyAnimator.Construct(KAC_TABLE_EMPTY_ANIMATOR_NAME, mpStateInterface, 0);

        // Per-visit view state.
        mbTableEmptyShowing             = false;
        mfCurrentTableWaitTime          = 0.0f;
        mbUsingFilters                  = true;
        miCurrentFilterHighlighted      = 0;
        mbPerRoadLeaderboard            = false;
        mbEventLeaderboard              = false;
        mbWaitingForTargetScoreResponse = false;

        // X360 `stb r30, 0xDF0(r31)` -- a single BYTE store on the name's first character,
        // i.e. the screen enters with no challenge target (PlayerName::IsEmpty()).
        mCurrentTargetScorePlayerName.macName[0] = 0;

        // { 8, 191, 12, 0, 0 } on channel 40, X360 record size 20: leave the CrashNav map.
        GuiEventActivateCrashNav lDeactivateCrashNav(false);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lDeactivateCrashNav),
            KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lDeactivateCrashNav)));

        // { 1, 148, 12, 0 } on channel 40, X360 record size 16: take the HUD down.
        GuiEventShowHideHudWire lHideHud(false);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lHideHud),
            KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lHideHud)));

        BuildAlphabeticalRoadIndexes();
    }

    // ---- OnLeave @ 0x8249F3B0 ------------------------------------------------------
    // Tear the screen down: stop observing, unmount the screen's apt movie, clear the
    // filter row and drop the expected-component list the cache is still holding for us.
    void OnlineScoreboards::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // The X360 inlines StateInterface::PlayAptMovie here: the GuiEventPlayAptMovie
        // record { 8, 18, 12, name, level } pushed on the view-state channel 41, size 20.
        // An empty name at level 3 is "clear level 3".
        mpStateInterface->PlayAptMovie(KAC_NO_APT_MOVIE, KI_APT_MOVIE_LEVEL);

        // X360 `(*(*(this + 0xE08) + 0x18))(this + 0xE08)` -- component vtable slot 6 on the
        // toggle group, which is SelectableGroup::Clear.
        mFilterToggleGroup.Clear();

        ClearExpectedComponent();
    }
}
