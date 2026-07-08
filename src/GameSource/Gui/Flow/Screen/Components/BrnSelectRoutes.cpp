// ===================================================================================
// BrnGui::SelectRoutes -- out-of-line bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// This slice covers the three functions that ground on the recovered layout:
//   GetCurrentlySelectedCheckpointIndex     @ 0x82418B90
//   GetCheckpointLandmark                   @ 0x824837B8
//   GetCurrentlySelectedCheckpointLandmark  @ 0x824899E8
//
// RemoveLastCheckpoint (@0x82483920), SetStartLightTriggerID (@0x82483878) and
// SetCurrentlySelectedCheckpointLandmark (@0x82489B18) are intentionally NOT reconstructed
// here: each branches on the two-player-mode discriminator GuiCache+0xA9B8 (== 11), an
// unrecovered foreign member, so a faithful body cannot be written without fabricating it.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/Components/BrnSelectRoutes.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{
    // ---- GetCurrentlySelectedCheckpointIndex @ 0x82418B90 -------------------------
    // Absolute menu index = first-checkpoint index + cursor offset; 0 when unselected.
    s32 SelectRoutes::GetCurrentlySelectedCheckpointIndex() const
    {
        if (mi8CursorOffset == -1)
            return 0;

        return miFirstCheckpointItemIndex + mi8CursorOffset;
    }

    // ---- GetCheckpointLandmark @ 0x824837B8 ---------------------------------------
    // Read the LandmarkIndex stored for (round, checkpoint) from the menu-item grid.
    BrnGameState::LandmarkIndex SelectRoutes::GetCheckpointLandmark(s32 liRoundNumber,
                                                                    s32 liCheckpointIndex) const
    {
        CGS_ASSERT(liCheckpointIndex > 0, "liCheckpointIndex > 0");
        CGS_ASSERT(liCheckpointIndex < KI_MAX_CHECKPOINTS, "liCheckpointIndex < KI_MAX_CHECKPOINTS");
        CGS_ASSERT(liCheckpointIndex < maiNumCheckpoints[liRoundNumber],
                   "liCheckpointIndex < maiNumCheckpoints[ liRoundNumber ]");

        // X360: *result = *(u16*)(this + 2 * (50 * checkpoint + round + 70)).
        const u16 luLandmark = mMenuData.mauHalf[50 * liCheckpointIndex + liRoundNumber + 70];
        return BrnGameState::LandmarkIndex(static_cast<s16>(luLandmark));
    }

    // ---- GetCurrentlySelectedCheckpointLandmark @ 0x824899E8 ----------------------
    // Resolve the landmark for the checkpoint the cursor sits on. Menu-item type 1 (and the
    // "next checkpoint" type 3 when the round is already full) reports the current checkpoint;
    // a not-yet-full type-3 item reports the previous checkpoint; anything else has none.
    BrnGameState::LandmarkIndex
    SelectRoutes::GetCurrentlySelectedCheckpointLandmark(s32 liRoundNumber) const
    {
        const s32 liCurrentItem =
            (mi8CursorOffset == -1) ? 0 : miFirstCheckpointItemIndex + mi8CursorOffset;

        bool lbUsePreviousCheckpoint = false;

        if (GetMenuItemType(liCurrentItem) != 1)
        {
            const s32 liItem =
                (mi8CursorOffset == -1) ? 0 : miFirstCheckpointItemIndex + mi8CursorOffset;
            if (GetMenuItemType(liItem) != 3)
                return BrnGameState::LandmarkIndex(0);

            // "Next checkpoint" slot: point at the current checkpoint when the round is full,
            // otherwise at the previous one. The fullness test indexes maiNumCheckpoints by the
            // miRoundNumber MEMBER (asm lwz r11,0x190C(r31)), not the incoming round param.
            if (maiNumCheckpoints[miRoundNumber] < KI_MAX_CHECKPOINTS)
                lbUsePreviousCheckpoint = true;
        }

        const s32 liCheckpoint =
            (mi8CursorOffset == -1) ? 0 : miFirstCheckpointItemIndex + mi8CursorOffset;

        return lbUsePreviousCheckpoint
                   ? GetCheckpointLandmark(liRoundNumber, liCheckpoint - 1)
                   : GetCheckpointLandmark(liRoundNumber, liCheckpoint);
    }
}
