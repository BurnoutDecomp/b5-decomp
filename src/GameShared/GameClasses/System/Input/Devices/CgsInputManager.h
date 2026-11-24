#pragma once

// CgsInput::Manager -- the platform-neutral base of the input managers (DecFIGS DWARF
// CgsInputManager.h:49; the PS3 build derives CgsInput::ManagerPS3 from it, CgsInputManagerPS3.h).
// It carries the input-port enumeration the platform managers' BindToPort / UnbindFromPort take:
// ARTIST's ManagerX360::BindToPort @0x828E7718 tests the port with signed compares against 0 and 4
// (`cmpwi r30,0; blt; cmpwi r30,4; blt`) under the assert text
// "(lePort >= E_INPUTPORT_FIRST) && (lePort < E_INPUTPORT_COUNT)".
//
// Only the enumeration is declared here: the DWARF's Manager member functions (Construct, Prepare,
// Update, GetConnectedDeviceType, SetPause, IsPaused, ...) have no X360 body or caller that this
// tree reconstructs.

namespace CgsInput
{
    class Manager
    {
    public:
        // DWARF CgsInputManager.h:52
        enum EInputPort
        {
            E_INPUTPORT_INVALID = -1,
            E_INPUTPORT_FIRST   = 0,
            E_INPUTPORT_1       = 0,
            E_INPUTPORT_2       = 1,
            E_INPUTPORT_3       = 2,
            E_INPUTPORT_4       = 3,
            E_INPUTPORT_COUNT   = 4,
        };
    };
}
