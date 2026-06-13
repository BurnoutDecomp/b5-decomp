#include "BrnCheckpointData.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8231C2C8
//   BrnGameState::CheckpointData::CheckpointData
//
// Initialises the checkpoint id to the "none" sentinel (-1).

namespace BrnGameState
{
CheckpointData::CheckpointData()
{
    mCheckpointId = -1;
}
}
