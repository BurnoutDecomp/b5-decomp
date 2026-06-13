#include "BrnNetworkMatchMakingManager.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827E4FA8
//   BrnNetwork::MatchMakingManager::MatchMakingManager
//
// Installs the shared dispatch tables into the three match slots and their
// seven sub-objects, the obj0-only extra dispatch table, then clears the
// status word and timer.

namespace BrnNetwork
{
namespace
{
    void* const KP_SlotVtable = reinterpret_cast<void*>(0x82083740);
    void* const KP_SubVtable  = reinterpret_cast<void*>(0x82083550);
    void* const KP_ExtraTable = reinterpret_cast<void*>(0x82083C84);
}

MatchMakingManager::MatchMakingManager()
{
    for (int i = 0; i < 3; ++i)
    {
        mSlots[i].mpVtable = KP_SlotVtable;
        for (int j = 0; j < 7; ++j)
        {
            mSlots[i].mpSub[j] = KP_SubVtable;
        }
    }

    mpExtra = KP_ExtraTable;

    mStatus = 0;
    mfTimer = 0.0f;
}
}
