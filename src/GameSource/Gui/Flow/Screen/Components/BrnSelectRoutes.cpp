// ===================================================================================
// BrnGui::SelectRoutes -- out-of-line bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// This slice covers three of the component's functions:
//   GetCurrentlySelectedCheckpointIndex     @ 0x82418B90
//   GetCheckpointLandmark                   @ 0x824837B8
//   GetCurrentlySelectedCheckpointLandmark  @ 0x824899E8
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/Components/BrnSelectRoutes.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{
    // ---- GetCurrentlySelectedCheckpointIndex @ 0x82418B90 -------------------------
    // Absolute menu index = first-checkpoint item + highlighted row; 0 when unselected.
    s32 SelectRoutes::GetCurrentlySelectedCheckpointIndex() const
    {
        if (mMenuOptions.GetHighlightedIndex() == -1)
            return 0;

        return miStartItem + mMenuOptions.GetHighlightedIndex();
    }

    // ---- GetCheckpointLandmark @ 0x824837B8 ---------------------------------------
    BrnGameState::LandmarkIndex SelectRoutes::GetCheckpointLandmark(s32 liRoundNumber,
                                                                    s32 liCheckpointIndex) const
    {
        CGS_ASSERT(liCheckpointIndex > 0, "liCheckpointIndex > 0");
        CGS_ASSERT(liCheckpointIndex < KI_MAX_CHECKPOINTS, "liCheckpointIndex < KI_MAX_CHECKPOINTS");
        CGS_ASSERT(liCheckpointIndex < maiNumCheckpoints[liRoundNumber],
                   "liCheckpointIndex < maiNumCheckpoints[ liRoundNumber ]");

        return maCheckpointData[liCheckpointIndex].maLandmarkIndex[liRoundNumber];
    }

    // ---- GetCurrentlySelectedCheckpointLandmark @ 0x824899E8 ----------------------
    // A checkpoint row reports its own checkpoint. The finish-point row reports the current
    // checkpoint when the round is full, otherwise the previous one; any other row has none.
    BrnGameState::LandmarkIndex
    SelectRoutes::GetCurrentlySelectedCheckpointLandmark(s32 liRoundNumber) const
    {
        bool lbUsePreviousCheckpoint = false;

        if (GetMenuItemType(GetCurrentlySelectedCheckpointIndex()) != E_MENU_ITEM_TYPE_CHECKPOINT)
        {
            if (GetMenuItemType(GetCurrentlySelectedCheckpointIndex()) != E_MENU_ITEM_TYPE_FINISH_POINT)
                return BrnGameState::LandmarkIndex(0);

            // The fullness test indexes maiNumCheckpoints by the miCurrentRound MEMBER
            // (asm lwz r11,0x190C(r31)), not the incoming round param.
            if (maiNumCheckpoints[miCurrentRound] < KI_MAX_CHECKPOINTS)
                lbUsePreviousCheckpoint = true;
        }

        const s32 liCheckpoint = GetCurrentlySelectedCheckpointIndex();

        return lbUsePreviousCheckpoint
                   ? GetCheckpointLandmark(liRoundNumber, liCheckpoint - 1)
                   : GetCheckpointLandmark(liRoundNumber, liCheckpoint);
    }
}
