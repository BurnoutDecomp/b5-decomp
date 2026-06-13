#ifndef BRN_NETWORK_MATCH_MAKING_MANAGER_H
#define BRN_NETWORK_MATCH_MAKING_MANAGER_H

#include "types.hpp"

namespace BrnNetwork
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827E4FA8.
// Three match slots, each holding a dispatch table and seven sub-object
// dispatch tables, plus a trailing status word and timer.
class MatchMakingManager
{
public:
    MatchMakingManager();

private:
    struct MatchSlot
    {
        void* mpVtable;   // off_82083740
        void* mpSub[7];   // off_82083550, stride 160
    };

    MatchSlot mSlots[3];
    void*     mpExtra;    // obj0-only field (guest +1744) off_82083C84
    u32       mStatus;    // guest +5636
    float     mfTimer;    // guest +5640
};
}

#endif
