#include "BrnCheckpointData.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8231C2C8
//   BrnGameState::CheckpointData::CheckpointData
//
// Initialises the checkpoint id to the "none" sentinel (-1).

namespace BrnGameState
{
CheckpointData::CheckpointData()
{
    /* X360 0x8231C2C8: block-section list left unconstructed; the +40 count word is the
       KI_UNCONSTRUCTED(-1) sentinel until Construct(). */
}
}
