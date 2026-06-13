#ifndef BRN_CHECKPOINT_DATA_H
#define BRN_CHECKPOINT_DATA_H

#include "types.hpp"

namespace BrnGameState
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8231C2C8.
class CheckpointData
{
public:
    CheckpointData();

private:
    u8  mPad0[40];      // unrecovered members preceding the id field
    s32 mCheckpointId;  // guest +40, initialised to -1
};
}

#endif
