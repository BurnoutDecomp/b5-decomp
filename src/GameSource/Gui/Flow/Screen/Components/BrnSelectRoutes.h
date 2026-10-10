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

namespace BrnGui
{
    class GuiCache;

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

        // absolute menu-item index of the highlighted row, or 0 when no row
        // is highlighted.
        s32 GetCurrentlySelectedCheckpointIndex() const;

        // the landmark stored for (round, checkpoint).
        BrnGameState::LandmarkIndex GetCheckpointLandmark(s32 liRoundNumber,
                                                          s32 liCheckpointIndex) const;

        // the landmark for the checkpoint the highlighted row stands for.
        BrnGameState::LandmarkIndex GetCurrentlySelectedCheckpointLandmark(s32 liRoundNumber) const;

        // classify a menu row (its own ledger function, declaration only).
        EMenuItemType GetMenuItemType(s32 liItemIndex) const;

        // Copy a preset event's route (start trigger, checkpoints, counties) into the current
        // round. Body in the .cpp.
        void SetRouteFromPresetEvent(s32 liPresetEventID);

    private:
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
}
