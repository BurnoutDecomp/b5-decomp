#pragma once

// ============================================================================
// b5-decomp/src/GameShared/GameClasses/System/Input/PC/CgsInputPadsPC.h
//
// FLAG PC-platform leaf: the host pad source. On the X360 the input module's
// per-frame pass (CgsInput::InputPads::Update -> the per-device reads -> the
// action binding tables) fills each port's CgsInput::InputIO::PadOutputInformation
// record in the module output buffer; none of that pass is reconstructed yet
// (the binding tables are opaque data blobs). This leaf stands in for it on PC:
// it reads the host keyboard (focus-gated GetAsyncKeyState) and, when present,
// the XInput pad 0 (XInputGetState via the dynamically loaded system XInput DLL),
// and publishes the player-0 pad record with the same observable contract the
// console fill produces:
//   - maActionInfo[k].muStatus edge bits (bit0 held / bit1 pressed / bit2 released)
//     for the GUI action ids the controller bridge consumes
//     (45 accept, 49 stop/back, 41 menu-next, 42 menu-prev -- the ids BootLegal
//     reads back out of the bridge's GuiEventControllerInput* events);
//   - the analogue stick words + the connection/state tail (muConnectionWord 0,
//     meControllerState, mbDisconnected 0).
// Everything downstream of this record is the real console path
// (BrnGameModule::BridgeControllerToGui -> CgsGui::GuiModule::AddGuiEvent).
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"   // CgsInput::InputIO::OutputBuffer

namespace CgsInput
{
    class InputPadsPC
    {
    public:
        // Fill the player-0 pad record in lpOutput from the host keyboard + XInput pad 0.
        // Must be called with lpOutput write-locked (it writes the pad record the way the
        // console module fill does). Edge state is kept across calls (file-static).
        static void UpdatePlayer0(InputIO::OutputBuffer* lpOutput);
    };
}
