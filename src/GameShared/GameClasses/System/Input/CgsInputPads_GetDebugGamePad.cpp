#include "GameShared/GameClasses/System/Input/CgsInputPads.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsInput::InputPads::GetDebugGamePad @ 0x823A5F38 (DecFIGS DWARF CgsInputPads.h:251).
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Range-asserts liPortIndex (the two asserts are
// non-gating tripwires under the house assert substitution), then returns the address of the
// liPortIndex-th physical pad record: `452 * liPortIndex + this + 120`, i.e. &maPads[liPortIndex].
// Sole caller: BrnGame::BrnGameModule::HandleInputBindResult @0x823A8C38, which forwards the
// result to CgsDev::DebugManager::SetGamePad as the active debug-controller pad.

namespace CgsInput
{
    DeviceX360Pad* InputPads::GetDebugGamePad(s32 liPortIndex)
    {
        CGS_ASSERT(liPortIndex >= 0, "liPortIndex >= 0");                          // h:251
        CGS_ASSERT(liPortIndex < (s32)KU_NUMBER_OF_PADS, "liPortIndex < (int32_t)KU_NUMBER_OF_PADS"); // h:252
        return &maPads[liPortIndex];
    }
}
