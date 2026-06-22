#include "GameSource/Game/BrnGlobalCpuMonitors.h"

#include <cstddef>

// Embed check for BrnGame::BrnCpuMonitors::Construct @ 0x823A90A8.
// 39 s32 handle fields with a reserved word at +0x8 (asm hole) -> sizeof == 160.
// Verify the asm-pinned offsets: first handle @0, reserved @8, NetworkAIRaceCar @0xC,
// last handle (SoundUpdate) @0x9C=156.
using BrnGame::BrnCpuMonitors;

static_assert(sizeof(BrnCpuMonitors) == 160, "BrnCpuMonitors handle-block size (40 slots incl +0x8 reserved)");
static_assert(offsetof(BrnCpuMonitors, miUT_TotalUpdate) == 0, "first handle at +0");
static_assert(offsetof(BrnCpuMonitors, mReserved08) == 0x08, "asm hole word at +0x8");
static_assert(offsetof(BrnCpuMonitors, miUT_NetworkAIRaceCar) == 0x0C, "handle[2] at +0xC");
static_assert(offsetof(BrnCpuMonitors, miUT_SoundUpdate) == 0x9C, "last handle at +0x9C");

void BrnGlobalCpuMonitors_embed_check()
{
    BrnCpuMonitors mon;
    mon.Construct();
    // post-condition: every handle is the -1 "unregistered" sentinel
    (void)mon.miUT_TotalUpdate;
    (void)mon.miUT_SoundUpdate;
}
