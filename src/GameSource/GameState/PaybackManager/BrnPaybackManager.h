#pragma once

#include "types.hpp"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"   // BrnNetwork::EPaybackType

// BrnGameState::PaybackManager - PLACEHOLDER layout, sized so the two dirty-trick fields land at
// the X360-attested offsets the PaybackDebugComponent reads (meActiveDirtyTrickType @ +596,
// meAwardedDirtyTrick @ +600). The full PaybackManager (DWARF
// references/DecFIGS/dwarfdump/GameSource/GameState/PaybackManager/BrnPaybackManager.h) carries
// nine leading members (TimerStatusInterface, GameStateToNetworkInterface::DirtyTrickQueue,
// Random, DirtyTrickEvent, three float32_t timers spelled f32 here per project convention, etc.)
// whose individual sizes are recovered by their own TUs. They are collapsed into the named
// reserved prefix below; when the full PaybackManager TU lands it MUST replace maReserved_0x000
// in place (keeping meActiveDirtyTrickType/meAwardedDirtyTrick at +596/+600 and the trailing
// members below them) rather than re-fork the struct. DWARF src159/src160 attest both dirty-trick
// fields as BrnNetwork::EPaybackType, adjacent.

namespace BrnGameState
{
    struct PaybackManager
    {
        // Opaque prefix collapsing the nine leading DWARF members. Sized so the two dirty-trick
        // enum fields below land at the X360-attested OnActivate offsets (+596 / +600). Provisional
        // internal split (the offset itself is authoritative from the disassembly).
        u8 maReserved_0x000[596];

        BrnNetwork::EPaybackType meActiveDirtyTrickType;   // +596  (DWARF BrnPaybackManager.h src159)
        BrnNetwork::EPaybackType meAwardedDirtyTrick;      // +600  (DWARF BrnPaybackManager.h src160)
    };
}
