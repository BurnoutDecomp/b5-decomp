#pragma once
// ===================================================================================
// BrnGui::SelectRoutes  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnSelectRoutes.h
//
// The route-selection component BrnGui::OnlineSelectRoute embeds: per round, the start
// point, the checkpoints and the finish point picked for the online race, shown as a
// scrolling menu with an up and a down arrow.
//
// Class shape and member order are the debug-info's (BrnSelectRoutes.h:46), checked
// against the console: OnlineSelectRoute's constructor builds this component's vtable, the
// menu at +0x730 and the two arrow animators at +0x17F0 / +0x187C; the getters below read
// the menu highlight at +0x7D5, miCurrentRound at +0x190C, miStartItem at +0x1910,
// maiNumCheckpoints at +0x1914 and the landmark lane at +0x8C + 100 * checkpoint +
// 2 * round. Members are reached by name; the console offsets are documentary.
// ===================================================================================
#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"         // CgsGui::GuiComponent (base)
#include "GameSource/GameState/BrnGameStateTypes.h"                         // BrnGameState::LandmarkIndex
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"    // BrnGui::AnimationComponent
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"         // BrnGui::MenuComponent
#include "SharedClasses/World/BrnWorldRegion.h"                             // BrnWorld::ECounty
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT_AT (header-inline SetCurrentRound)
#define CGS_ARTIST_BRNSELECTROUTES_H_FILE "..\\..\\..\\GameSource\\Gui/Flow/Screen/Components/BrnSelectRoutes.h"

namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;
    struct GuiEventNetworkGameParams;

    struct SelectRoutes : public CgsGui::GuiComponent
    {
        // debug-info BrnSelectRoutes.h:54.
        enum EChangedItem
        {
            E_CHANGED_ITEM_LANDMARK            = 0,
            E_CHANGED_ITEM_CHECKPOINT          = 1,
            E_CHANGED_ITEM_EDIT_CHECKPOINT     = 2,
            E_CHANGED_ITEM_EDIT_NEW_CHECKPOINT = 3,
            E_CHANGED_ITEM_FINISHED_EDIT       = 4,
            E_CHANGED_ITEM_COUNT               = 5,
        };

        // debug-info BrnSelectRoutes.h:65.
        enum EMenuItemType
        {
            E_MENU_ITEM_TYPE_START_POINT    = 0,
            E_MENU_ITEM_TYPE_CHECKPOINT     = 1,
            E_MENU_ITEM_TYPE_ADD_CHECKPOINT = 2,
            E_MENU_ITEM_TYPE_FINISH_POINT   = 3,
            E_MENU_ITEM_TYPE_DONE           = 4,
            E_MENU_ITEM_TYPE_COUNT          = 5,
        };

        // debug-info BrnSelectRoutes.h:50 (GetCheckpointLandmark's bound).
        static const s32 KI_MAX_CHECKPOINTS = 17;
        // debug-info BrnSelectRoutes.h:51/:52, defined cpp:38/:39 (ARTIST .rdata 0x8204CA50 = 1000.0f,
        // 0x8204CA54 = 200.0f). Public: OnlineSelectRoute reads both from their symbols
        // (DisplayTooShortRouteMessage 0x8249EFB4, UpdateAfterToggleChange 0x8249E558).
        static const f32 KF_MIN_ROUTE_DISTANCE;
        static const f32 KF_MIN_CHECKPOINT_DISTANCE;
        // The per-round array length of CheckpointData and maiNumCheckpoints (debug-info [10]).
        static const s32 KI_MAX_ROUNDS = 10;

        // debug-info BrnSelectRoutes.h:193 -- one checkpoint slot across every round (100 bytes).
        struct CheckpointData
        {
            BrnGameState::LandmarkIndex maLandmarkIndex[KI_MAX_ROUNDS];   // +0x00
            // debug-info types these LightTriggerId; that handle is the u32 typedef of
            // BrnGameModeParams.h, spelled as its underlying type here (as BrnMapIconManager.h does).
            u32                         maStartLightTriggerID[KI_MAX_ROUNDS];   // +0x14
            BrnWorld::ECounty           maeSelectedCounty[KI_MAX_ROUNDS];       // +0x3C
        };

        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName) ;
        void Setup(const GuiEventNetworkGameParams* lpGameParams, GuiCache* lpGuiCache);
        void Update();
        void AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache);
        void Show();
        void Hide();
        void SetupMenuText();
        void HighlightAddCheckpoint();

        // PC state-queue boundary: ARTIST reads the header-stripped 8-byte
        // {padId, buttonId} record. DWARF spells GuiEventControllerInputPressed*;
        // the canonical PC event also contains its explicit 12-byte queue header.
        EChangedItem HandleControllerInput(const CgsModule::Event* lpControllerEvent);

        s32 GetNumberCheckpoints(s32 liRoundNumber) const;
        u32 GetStartLightTriggerID(s32 liRoundNumber) const;
        u32 GetStartPointEventIndex(s32 liRoundNumber) const;
        void SetCurrentRound(s32 liRoundNumber);
        s32 GetCurrentRound() const;
        EMenuItemType GetCurrentMenuItemType() const;
        void SetStartLightTriggerID(s32 liRoundNumber, u32 luStartLightTriggerID);
        void SetCurrentlySelectedCheckpointLandmark(s32 liRoundNumber,
                                                    BrnGameState::LandmarkIndex lLandmarkIndex);
        void RemoveLastCheckpoint();

        // absolute menu-item index of the highlighted row, or 0 when no row
        // is highlighted.
        s32 GetCurrentlySelectedCheckpointIndex() const;

        // the landmark stored for (round, checkpoint).
        BrnGameState::LandmarkIndex GetCheckpointLandmark(s32 liRoundNumber,
                                                          s32 liCheckpointIndex) const;

        // the landmark for the checkpoint the highlighted row stands for.
        BrnGameState::LandmarkIndex GetCurrentlySelectedCheckpointLandmark(s32 liRoundNumber) const;

        // classify an absolute menu row for the current round.
        EMenuItemType GetMenuItemType(s32 liItemIndex) const;

        // Copy a preset event's route (start trigger, checkpoints, counties) into the current
        // round. Body in the .cpp.
        void SetRouteFromPresetEvent(s32 liPresetEventID);

    private:
        // debug-info BrnSelectRoutes.h:200, defined cpp:36 = 6 (ARTIST .rdata 0x8204CA4C).
        static const s32 KI_NUM_DISPLAYABLE_CHECKPOINTS;
        // DWARF BrnSelectRoutes.cpp:48, a private static array of (non-const) pointers: ARTIST keeps it in
        // .data at 0x82F25298 = {"invisible", "visible", "animate"} and loads an entry per use (Hide 0x82418D18).
        static const char* KPC_ARROW_ANIMATION_STATES[3];
        bool SelectRandomLandmark(s32 liCheckpointItem, BrnWorld::ECounty leCounty);

        // Pick a random online landmark (checkpoint) / finish point for the given checkpoint row
        // of liRoundNumber, in leCounty (E_COUNTY_COUNT == anywhere), and return its landmark;
        // -1 when nothing qualifies. Bodies in the .cpp.
        BrnGameState::LandmarkIndex GenerateRandomLandmark(s32 liRoundNumber, s32 liCheckpointItem,
                                                           BrnWorld::ECounty leCounty);
        BrnGameState::LandmarkIndex GenerateRandomFinishPoint(s32 liRoundNumber, s32 liCheckpointItem,
                                                              BrnWorld::ECounty leCounty);
        // Pick a random event start in leCounty far enough from the first checkpoint and return
        // its traffic-light trigger id (the LightTriggerId handle); -1 when nothing qualifies.
        u32 GenerateRandomStartPoint(s32 liRoundNumber, BrnWorld::ECounty leCounty);

        // ---- data members (debug-info order; console offsets documentary) --------------------
        CheckpointData     maCheckpointData[KI_MAX_CHECKPOINTS];   // +0x008C
        MenuComponent      mMenuOptions;                           // +0x0730
        AnimationComponent mUpArrowAnimator;                       // +0x17F0
        AnimationComponent mDownArrowAnimator;                     // +0x187C
        GuiCache*          mpGuiCache;                             // +0x1908
        s32                miCurrentRound;                         // +0x190C
        s32                miStartItem;                            // +0x1910
        s32                maiNumCheckpoints[KI_MAX_ROUNDS];       // +0x1914
    };

    // Header-inline accessors: DWARF places their definitions in BrnSelectRoutes.h (lines
    // cited per function) and ARTIST has no out-of-line copy of any of them; SetCurrentRound's
    // asserts name the header (OnlineSelectRoute::SelectRound 0x82493610 / 0x82493634).

    // DWARF h:277.
    inline s32 SelectRoutes::GetNumberCheckpoints(s32 liRoundNumber) const
    {
        return maiNumCheckpoints[liRoundNumber];
    }

    // DWARF h:320.
    inline u32 SelectRoutes::GetStartLightTriggerID(s32 liRoundNumber) const
    {
        return maCheckpointData[0].maStartLightTriggerID[liRoundNumber];
    }

    // DWARF h:338.
    inline void SelectRoutes::SetCurrentRound(s32 liRoundNumber)
    {
        CGS_ASSERT_AT(liRoundNumber >= 0, "liRound >= 0", CGS_ARTIST_BRNSELECTROUTES_H_FILE, 340);
        CGS_ASSERT_AT(static_cast<u32>(liRoundNumber) < KI_MAX_ROUNDS,
                      "((uint32_t)liRound) < BrnGameState::GameStateModuleIO::KU_MAX_ONLINE_ROUNDS_IN_MODE",
                      CGS_ARTIST_BRNSELECTROUTES_H_FILE, 341);
        miCurrentRound = liRoundNumber;
    }

    // DWARF h:347.
    inline s32 SelectRoutes::GetCurrentRound() const
    {
        return miCurrentRound;
    }

    // DWARF h:209, header-inline: listed without a line in the BrnGuiComponentUnity and
    // BrnGuiOnlineScreenUnity CUs; ARTIST inlines it into OnlineSelectRoute::UpdateIconManager
    // (0x824A95FC..0x824A9618) and keeps the COMDAT 0x82418B90 for HandleControllerInput.
    // Absolute menu index = first-checkpoint item + highlighted row; 0 when unselected.
    inline s32 SelectRoutes::GetCurrentlySelectedCheckpointIndex() const
    {
        if (mMenuOptions.GetHighlightedIndex() == -1)
            return 0;

        return miStartItem + mMenuOptions.GetHighlightedIndex();
    }

    // DWARF h:438.
    inline SelectRoutes::EMenuItemType SelectRoutes::GetCurrentMenuItemType() const
    {
        return GetMenuItemType(GetCurrentlySelectedCheckpointIndex());
    }
}
