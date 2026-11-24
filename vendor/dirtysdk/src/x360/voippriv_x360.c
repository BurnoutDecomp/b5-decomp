// DirtySDK 5.5.3 voice private state: the voice module's memory group.
// A module of its own, as in the original (the PS3 twin is DWARF core/source/voip/ps3/voipprivps3.c:
// _Voip_iMemGroup :27, VoipMemGroupQuery :50, VoipMemGroupSet :70): ARTIST places it after the
// voice-connection module (VoipMemGroupQuery 0x82B3AB88, VoipMemGroupSet 0x82B3AB98) and every
// caller (VoipShutdown, VoipConnectionStartup, VoipConnectionShutdown) calls it out of line.

#include "types.hpp"
#include "voip.h"

// ARTIST 0x82F84B78 (.data) holds 'dflt' until VoipStartup sets the group.
int32_t _Voip_iMemGroup = 0x64666c74; // 'dflt'

int32_t VoipMemGroupQuery(void)
{
    return _Voip_iMemGroup;
}

void VoipMemGroupSet(int32_t iMemGroup)
{
    _Voip_iMemGroup = iMemGroup;
}
