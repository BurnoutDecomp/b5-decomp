// wave-I partfile

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::PlayAptMovie
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache / GuiFlow
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / the out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiEventActivateCrashNav (id 191)

namespace BrnGui
{

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x824A7758.json, asm arbitrated over Hex-Rays).
//
// Notes taken from the asm rather than the pseudocode:
//  * EnsureResourcesAreLoaded is the THREE-ARG face, not the no-arg one the spec's triage
//    line guessed: 0x824A77B0..0x824A77BC set r4 = &maResourceTuplesToLoad (unk_8205F004)
//    and r5 = 2, matching BrnGuiCache.h:210
//    `bool EnsureResourcesAreLoaded(const CgsGui::sResourceTuple*, u32)`. The 2 is the
//    same-TU constant-fold of miNumResourcesToLoad, so the source form is the static.
//  * Its result is consumed as a BYTE (`clrlwi r11, r3, 24`), which is what the bool
//    return already gives; likewise AreAllAptComponentsInitialised.
//  * The X360 re-loads AND re-tests mpGuiCache on the substate-1 arm (0x824A77FC /
//    0x824A7800) even though the assert at the top already covered it -- CGS_ASSERT is
//    non-fatal, so that second test is real behaviour and is kept.
//  * `stwx r11, r31, 0xA0B0` (41136) is mRouteInfoDisplay.miNumComponentsLoaded
//    (32528 + 8608), and the value stored is the loaded word dword_8204C7A0 ==
//    GuiNetworkRouteInfo::KI_NUM_COMPONENTS_TO_LOAD (11). Both are public members, so this
//    is a named store, NOT an offset poke -- the console offsets are recorded only.
//  * `stwx r11, r31, 0xA4FC` (42236) is mpCommonOptions = &unk_8205F14C == KAE_COMMON_OPTIONS.
//  * The GuiFlow argument is `li r4, 0` == E_GUIFLOW_SCREEN on both cache calls.
//  * No floats anywhere in this body, so there is no NaN-polarity decision to make.
    namespace
    {
        // ---- the create-match apt movie ------------------------------------------------
        // PlayAptMovie(off_82F27BA0[0], 3) @0x824A77E4. (Shared with the OnEnter partfile --
        // keep only one copy on merge.)
        const char KAC_APT_MOVIE_NAME[] = "ON_CREA";
        const s32  KI_APT_MOVIE_LEVEL   = 3;
    }

    // ================================================================================
    //  CheckForCompletedLoads  @ 0x824A7758
    //
    //  Driven from Update once per frame while the page is still coming up: first wait
    //  for the screen's two resources, then for the apt components the cache is watching.
    // ================================================================================
    void OnlineGameOptions::CheckForCompletedLoads()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:413 (non-fatal)

        switch (meSubState)
        {
            case E_SUBSTATE_LOADING_SCREEN:
                // The screen's own resource list (the header's GetResourcesToLoad pair);
                // the X360 folds miNumResourcesToLoad to its value here.
                if (mpGuiCache->EnsureResourcesAreLoaded(maResourceTuplesToLoad,
                                                         static_cast<u32>(miNumResourcesToLoad)))
                {
                    mpStateInterface->PlayAptMovie(KAC_APT_MOVIE_NAME, KI_APT_MOVIE_LEVEL);
                    meSubState = E_SUBSTATE_LOADING_COMPONENTS;
                }
                break;

            case E_SUBSTATE_LOADING_COMPONENTS:
                // The X360 re-loads and re-tests mpGuiCache on this arm (the assert above is
                // non-fatal, so the pointer really can still be null here).
                if (mpGuiCache != 0 &&
                    mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
                {
                    mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

                    // The route panel's components came up with the rest of the screen, so
                    // mark it fully loaded rather than counting them again.
                    mRouteInfoDisplay.miNumComponentsLoaded =
                        GuiNetworkRouteInfo::KI_NUM_COMPONENTS_TO_LOAD;

                    mHelpBar.SetupComponent();

                    // The always-present half of the option list (just the game-mode row).
                    mpCommonOptions = KAE_COMMON_OPTIONS;

                    ShowGameOptionsScreen();
                }
                break;

            default:
                break;
        }
    }

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8249C188.json, asm arbitrated over Hex-Rays).
//
// Notes taken from the asm rather than the pseudocode:
//  * CONSTRUCTION ORDER is the asm's, not the DWARF declaration order: menu, toggles,
//    route info, up arrow (+0x7A90), down arrow (+0x7B1C), load-header animator (+0x7BA8),
//    load-header text (+0x7CC0), title text (+0x7DE8), THEN the help bar (+0x5C40), and
//    the map border (+0x7C34) LAST. The five middle calls are `lwz r10, <this+off>` /
//    `lwz r11, 0(r10)` / `bctrl` -- the GuiComponent Construct virtual (vtable slot 0),
//    written here as the plain named call.
//  * The two group Constructs pass `li r8, -1` + `clrldi r8, r8, 32`: a 32-bit all-ones
//    apt id ZERO-EXTENDED into the u64 parameter, i.e. 0xFFFFFFFFull -- NOT (u64)-1.
//    BrnOnlinePlay.cpp writes the same literal for the same parameter.
//  * The console member offsets in the comments (+56, +42236, ...) are recorded only.
//    Every access below is by name, so the host's own layout applies. `stbx r28, r31, r9`
//    at +42256 is the byte store into the bool mbIsCreating.
//  * The four out-queue records are stack-built at sp+0x50 and reuse the same slots, which
//    is why Hex-Rays shows the `ld`/`std` doubleword shuffle: what actually ends up on the
//    wire is decoded per record below. All four go out on channel 0x28 == 40.
//  * meOptionsToLoad (+42244) is stored AFTER the four AddEvent calls (0x8249C438) -- keep
//    that order; it is the only store in the epilogue.
//  * No floats anywhere in this body, so there is no NaN-polarity decision to make.
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) ----------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;   // 0x28 -- the only channel OnEnter posts on

        // ---- the create-match apt movie ------------------------------------------------
        // PlayAptMovie(off_82F27BA0[0], 3) @0x824A77E4 (used by CheckForCompletedLoads).

        // The apt id the two group Constructs pass. The X360 builds it with
        // `li r8, -1` + `clrldi r8, r8, 32`, i.e. the 32-bit all-ones id zero-extended
        // into the u64 parameter -- NOT a 64-bit -1.
        const u64 KU_INVALID_APT_ID = 0xFFFFFFFFull;

        // ---- out-queue wire records -----------------------------------------------------
        // Each is a CgsGui::GuiEvent<N> header { payload bytes, event id, payload offset }
        // followed by the payload, published through
        // mpStateInterface->GetOutputEventQueue()->AddEvent(record, channel, sizeof(record)).
        // The X360 record sizes quoted are the CONSOLE sizes; all three payloads below are
        // pointer-free so the host record has the same shape, but the header words are
        // still DERIVED from sizeof/offsetof rather than stamped with the console literal
        // (BrnLicenseComponent.cpp precedent).

        // Id 264 == BrnGui::GuiEventNetworkOutputPlayerTexture -- the same record
        // BrnLicenseComponent.cpp / BrnPhotoBoothComponent.cpp build: { mode, playerIndex },
        // X360 record size 20. The create-match page asks for mode 1 on player -1 (the X360
        // packs the pair as one doubleword before overwriting the header words in place).
        struct GuiEventNetworkOutputPlayerTextureRecord : public CgsGui::GuiEvent<264>
        {
            s32 meMode;          // payload +0x00
            s32 miPlayerIndex;   // payload +0x04

            GuiEventNetworkOutputPlayerTextureRecord(s32 leMode, s32 liPlayerIndex)
                : CgsGui::GuiEvent<264>(), meMode(leMode), miPlayerIndex(liPlayerIndex)
            {
                // The subtraction is only safe here because the two-s32 payload leaves no
                // trailing padding, so sizeof - offsetof really is the payload byte count
                // (X360 `li r27, 8` at 0x8249C368). Do NOT copy this form onto a record
                // whose payload is narrower than the record's alignment -- see the two
                // bool records below, which take the payload size directly.
                const size_t luOffset = offsetof(GuiEventNetworkOutputPlayerTextureRecord, meMode);
                muHeader0 = static_cast<u32>(sizeof(*this) - luOffset);   // X360 8
                muHeader2 = static_cast<u32>(luOffset);                   // X360 12
            }
        };

        // Id 148 == BrnGui::GuiEventShowHideHud (payload size 1). OnEnter drops the HUD as
        // the create-match page comes up. NOTE the channel: this screen posts it on the GUI
        // OUT channel (40), not the internal channel (42) the game-room screen uses.
        // X360 record size 16.
        //
        // HEADER0 IS THE PAYLOAD BYTE COUNT, NOT sizeof(record) - offsetof(payload). On the
        // 8-byte payload above the two agree, but a lone bool pads this record out to 16,
        // so the subtraction would publish 4 where the X360 publishes 1: 0x8249C33C
        // `li r28, 1`, stored as word0 at 0x8249C3EC (this record) and 0x8249C424 (the id-65
        // twin), with word2 = r29 = 12 and the AddEvent size `li r6, 0x10` == 16. So header0
        // is taken from the payload MEMBER, the way the wave-H twin does it
        // (BrnOnlineGameRoomPlayerInfo_wH_07.cpp builds the same id-148 record as (1, 12)).
        struct GuiEventShowHideHudWire : public CgsGui::GuiEvent<148>
        {
            bool mbShowHud;   // payload +0x00

            explicit GuiEventShowHideHudWire(bool lbShowHud)
                : CgsGui::GuiEvent<148>(
                      static_cast<u32>(sizeof(bool)),                                 // X360 1
                      static_cast<u32>(offsetof(GuiEventShowHideHudWire, mbShowHud)))  // X360 12
                , mbShowHud(lbShowHud)
            {
            }
        };

        // Id 65 == the car-control-change request (BrnGuiDemangledEventTypes.h names it
        // GuiRequestCarControlChangeEvent; that header cannot be included alongside
        // BrnGuiEventTypeDefs.h, so the record is rebuilt here). One payload byte, cleared
        // -- the create-match page takes car control away while the menu is up.
        // X360 record size 16.
        struct GuiRequestCarControlChangeWire : public CgsGui::GuiEvent<65>
        {
            bool mbCarControlEnabled;   // payload +0x00

            // Same payload-byte-count rule as the id-148 record above (X360 word0 = 1 from
            // the shared r28, AddEvent size 16 at 0x8249C40C).
            explicit GuiRequestCarControlChangeWire(bool lbCarControlEnabled)
                : CgsGui::GuiEvent<65>(
                      static_cast<u32>(sizeof(bool)),                                            // X360 1
                      static_cast<u32>(offsetof(GuiRequestCarControlChangeWire,
                                                mbCarControlEnabled)))                           // X360 12
                , mbCarControlEnabled(lbCarControlEnabled)
            {
            }
        };
    }

    // ================================================================================
    //  OnEnter  @ 0x8249C188
    //
    //  Build the create-match option table, register the screen's event set, construct
    //  every embedded component in the X360's order, reset the screen's own state and
    //  publish the four entry-time records.
    // ================================================================================
    void OnlineGameOptions::OnEnter()
    {
        // The option table ({name, option} rows) has to exist before anything reads it.
        BuildGameOptions();

        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // ---- components, in the X360's construction order ------------------------------
        // The six load slots and the five create-game toggle rows.
        mMenuOptions.Construct(KAC_MENU_OPTIONS_COMPONENT, mpStateInterface,
                               KI_MAX_LOAD_OPTIONS, 0, KU_INVALID_APT_ID);
        mCreateGameToggles.Construct(KAC_CREATE_GAME_TOGGLE_COMPONENT, mpStateInterface,
                                     KI_MAX_CREATE_GAME_OPTIONS, 0, KU_INVALID_APT_ID);

        // The route panel: (name, map type 0, state interface, no sat-nav icon base name).
        mRouteInfoDisplay.Construct(KAC_ROUTE_INFO_NAME, 0, mpStateInterface, 0);

        // The plain components go through the GuiComponent Construct virtual (vtable slot 0).
        mUpArrowAnimator.Construct(KAC_UP_ARROW_COMPONENT, mpStateInterface, 0);
        mDownArrowAnimator.Construct(KAC_DOWN_ARROW_COMPONENT, mpStateInterface, 0);
        mLoadHeaderAnimator.Construct(KAC_LOAD_HEADER_COMPONENT, mpStateInterface, 0);
        mLoadHeaderText.Construct(KAC_LOAD_HEADER_TEXT_COMPONENT, mpStateInterface, 0);
        mTitleText.Construct(KAC_TITLE_TEXT_COMPONENT, mpStateInterface, 0);

        // The help bar sits between the text fields and the map border in the X360's order.
        mHelpBar.Construct(KAC_HELP_BAR_COMPONENT, KI_MAX_HELP_BAR_ITEMS, mpStateInterface, 0);

        mMapBorderAnimator.Construct(KAC_MAP_BORDER_ANIMATION_COMPONENT, mpStateInterface, 0);

        // ---- the screen's own state ------------------------------------------------------
        meSubState      = E_SUBSTATE_LOADING_SCREEN;
        mpGuiCache      = 0;
        mpCommonOptions = 0;
        miCurrentRound  = 0;
        miStartItem     = 0;
        mbIsCreating    = true;   // X360 stbx -- a byte store

        mGameOptions.Construct();

        // ---- the four entry-time records, all on channel 40 -------------------------------
        // Start outputting the local player's gamer picture into the create-match page.
        GuiEventNetworkOutputPlayerTextureRecord lOutputPlayerTexture(1, -1);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lOutputPlayerTexture), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lOutputPlayerTexture)));   // X360 record size 20

        // Take the front-end map (CrashNav) down while the create-match page owns the screen.
        GuiEventActivateCrashNav lActivateCrashNav(false);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lActivateCrashNav), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lActivateCrashNav)));      // X360 record size 20

        GuiEventShowHideHudWire lHideHud(false);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lHideHud), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lHideHud)));               // X360 record size 16

        GuiRequestCarControlChangeWire lDisableCarControl(false);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lDisableCarControl), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lDisableCarControl)));     // X360 record size 16

        // The X360 stores this LAST, after the four posts -- keep the order.
        meOptionsToLoad = E_OPTIONS_TO_LOAD_SAVED;
    }
}
