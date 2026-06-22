#include "SharedClasses/Physics/Props/BrnPhysicsPropZoneData.h"

// BrnPhysics::Props::PropZoneData methods.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. All asserts are non-gating tripwires (execution
// continues past a failed assert), matching the X360 control flow exactly.
namespace BrnPhysics
{
namespace Props
{
    // X360 @ 0x822BB1F0. Walks the cells; within each cell the prop index is partitioned into
    // [respawn-different | dont-respawn | normal] sub-ranges. Faithful to the asm:
    //   for each cell (while liCell < muNumCells):
    //     if (liPropIndex - numRespawnDifferent  < 0) return 2  (E_RESPAWN_CHANGED)
    //     if (liPropIndex - numRespawnDifferent - numDontRespawn < 0) return 1 (E_DONT_RESPAWN)
    //     liPropIndex -= count; if (liPropIndex < 0) return 0   (E_RESPAWN)
    //   fell off the end (or muNumCells == 0): assert "Shouldnt get here", return 3.
    // The asm computes the third comparison as ((diff - count) + dontRespawn) + ((idx - diff) -
    // dontRespawn) == idx - count, then reuses that as the running index -- i.e. liPropIndex -= count.
    eRespawnType PropZoneData::GetRespawnTypeForProp(s32 liPropIndex) const
    {
        for (s32 liCell = 0; liCell < muNumCells; ++liCell)
        {
            const PropCellData& lrCell = maCells[liCell];
            s32 liNumRespawnDifferent = lrCell.GetNumberOfRespawnDifferent();
            s32 liNumDontRespawn      = lrCell.GetNumberOfDontRespawn();

            if (liPropIndex - liNumRespawnDifferent < 0)
                return E_RESPAWN_CHANGED;                                   // 2
            if (liPropIndex - liNumRespawnDifferent - liNumDontRespawn < 0)
                return E_DONT_RESPAWN;                                      // 1
            liPropIndex -= lrCell.GetCount();
            if (liPropIndex < 0)
                return E_RESPAWN;                                          // 0
        }

        CGS_ASSERT(false, "Shouldnt get here\n");                          // BrnPhysicsPropZoneData.h:460
        return E_RESPAWN_TYPE_COUNT;                                       // 3
    }

    // X360 @ 0x822BB2D0. Range-asserts the world XZ position (non-gating), then maps it to a
    // grid cell: cellAxis = (u16)( (s32)(s64)(axis + KI_WORLD_*_HALF_EXTENT) / KI_CELL_SIZE ).
    // The asm uses fctidz (truncate-toward-zero to int64) then a signed word divide by 100; the
    // X360 reuses the half-extent float (5000.0) as the additive bias for both X and Z.
    PropCellId PropZoneData::GetCellId(f32 lfX, f32 lfZ)
    {
        CGS_ASSERT(lfX > -KI_WORLD_X_HALF_EXTENT, "lfX > -KI_WORLD_X_HALF_EXTENT");                       // :515
        CGS_ASSERT(lfX < KI_WORLD_X_HALF_EXTENT + KI_WORLD_X_EXTENSION, "lfX < KI_WORLD_X_HALF_EXTENT + KI_WORLD_X_EXTENSION"); // :516
        CGS_ASSERT(lfZ > -KI_WORLD_Z_HALF_EXTENT, "lfZ > -KI_WORLD_Z_HALF_EXTENT");                       // :517
        CGS_ASSERT(lfZ < KI_WORLD_Z_HALF_EXTENT, "lfZ < KI_WORLD_Z_HALF_EXTENT");                         // :518

        PropCellId lId;
        lId.muX = static_cast<u16>(static_cast<s32>(static_cast<s64>(lfX + KI_WORLD_X_HALF_EXTENT)) / KI_CELL_SIZE);
        lId.muZ = static_cast<u16>(static_cast<s32>(static_cast<s64>(lfZ + KI_WORLD_Z_HALF_EXTENT)) / KI_CELL_SIZE);
        return lId;
    }
}
}
