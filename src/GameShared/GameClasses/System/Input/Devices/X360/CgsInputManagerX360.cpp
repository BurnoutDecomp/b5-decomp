// ============================================================================
// CgsInput::ManagerX360 -- the Xbox 360 input manager. Reconstructed from
// BURNOUT_X360_ARTIST.XEX (asm authoritative; no DecFIGS DWARF for this X360-only class).
//
// WAVE54: only BindToPort (@0x828E7718) is homed here. UpdateConnectedDevices
// (@0x828DC480) and UnbindFromPort (@0x828E7808) are declared-only in the header this
// wave (their proposed bodies were rejected by the verifier); they land in a later wave.
// ============================================================================

#include "GameShared/GameClasses/System/Input/Devices/X360/CgsInputManagerX360.h"
#include "GameShared/GameClasses/System/Input/Devices/X360/CgsInputDeviceX360Pad.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "types.hpp"

namespace CgsInput
{
    // ============================================================================
    // BindToPort @0x828E7718. Bind port lePort to physical pad lpPad. Returns true (and binds)
    // only when the slot record has a resolved device type; false otherwise.
    // ============================================================================
    bool ManagerX360::BindToPort(DeviceX360Pad* lpPad, u32 lePort)
    {
        CGS_ASSERT(lpPad != nullptr, "lpPad");                                              // line 288
        CGS_ASSERT(lePort < KU_NUMBER_OF_INPUT_PORTS,
                   "(lePort >= E_INPUTPORT_FIRST) && (lePort < E_INPUTPORT_COUNT)");        // line 289
        CGS_ASSERT(mapBoundPads[lePort] == nullptr, "mapBoundPads[lePort] == NULL");        // line 290

        if (maDevices[lePort].meDeviceType == 0)
            return false;

        mapBoundPads[lePort] = lpPad;                                    // stwx lpPad, mapBoundPads[lePort]
        ++miNumBoundPads;                                                // ++*(this+0x70)
        lpPad->BindToPort(lePort, maDevices[lePort].meDeviceType);       // pad.BindToPort(lePort, subType)
        return true;
    }
}
