#pragma once

// ============================================================================
// b5-decomp/src/GameShared/GameClasses/System/Input/PC/CgsInputPadsPC.h
//
// FLAG PC-platform leaf: the host pad source. On the X360 the input module's
// per-frame pass (CgsInput::ManagerX360::Update @0x828F0028 ->
// CgsInput::DeviceX360Pad::Update @0x828E7AB0 -> CgsInput::InputPads::Update
// @0x828F8690 -> CgsInput::InputPads::FillRawData @0x828E7350 -> the
// gaDefaultGameInputMapping action tables) fills each port's
// CgsInput::InputIO::PadOutputInformation record in the module output buffer;
// none of that pass is reconstructed (InputPads::Update is a HOLE in the IDA
// export set and the mapping table is un-exported rodata -- see the park at the
// top of the .cpp). This leaf stands in for it on PC: it reads the host keyboard
// (focus-gated GetAsyncKeyState) and, when present, the XInput pad 0
// (XInputGetState via the dynamically loaded system XInput DLL), and publishes
// the player-0 pad record with the same observable contract the console fill
// produces:
//   - maActionInfo[k].mfValue / .muStatus (bit0 held / bit1 pressed / bit2
//     released) for the EGameInputActions slots the controller bridges consume:
//     the GUI rows (45 accept, 49 stop/back, 41 menu-next, 42 menu-prev -- the
//     ids BootLegal reads back out of the bridge's GuiEventControllerInput*
//     events) AND the DRIVING rows BridgeControllerToWorld @0x823CD890 reads
//     (0 ACCELERATE, 1 BRAKE, 2 HANDBRAKE, 3 BOOST, 5 CHANGEVIEW, 7 RESET,
//     8 START, 13 HORN, 54/55 the GUI shoulder pair that becomes mfSpin);
//   - the analogue axis block (CgsInput::EPadAxis: the two sticks carrying the
//     console deadzone/saturation curve, the two wheel axes left at 0) + the
//     connection/state tail.
// Everything downstream of this record is the real console path
// (BrnGameModule::BridgeControllerToGui -> CgsGui::GuiModule::AddGuiEvent, and
// BrnGameModule::BridgeControllerToWorld -> BrnWorld::PlayerVehicleControls).
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
