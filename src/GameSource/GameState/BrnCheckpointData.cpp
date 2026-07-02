#include "BrnCheckpointData.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8231C2C8
//   BrnGameState::CheckpointData::CheckpointData
//
// Initialises the checkpoint id to the "none" sentinel (-1).

namespace BrnGameState
{
CheckpointData::CheckpointData()
{
    // X360 0x8231C2C8: stw r11(-1), 0x28(r3) -- stamps the block-section list's count word
    // (+40) with the KI_UNCONSTRUCTED(-1) sentinel until Construct() runs.
    mauBlockSectionIds.MarkUnconstructed();
}
}
