#include "GameSource/Game/BrnGlobalCpuMonitors.h"

#include <cstddef>

// Embed check for BrnGame::BrnCpuMonitors::Construct @ 0x823A90A8.
// Forty contiguous s32 handle slots occupy offsets +0x00..+0x9C (size 160).
// NetworkAIRaceCar is at +0x08 and Replay is at +0x40; there is no reserved hole.
using BrnGame::BrnCpuMonitors;

static_assert(sizeof(BrnCpuMonitors) == 160, "BrnCpuMonitors has 40 contiguous s32 handle slots");
static_assert(offsetof(BrnCpuMonitors, miUT_TotalUpdate) == 0, "first handle at +0");
static_assert(offsetof(BrnCpuMonitors, miUT_NetworkAIRaceCar) == 0x08, "Network + AI + Racecar handle at +0x08");
static_assert(offsetof(BrnCpuMonitors, miUT_Network) == 0x0C, "Network handle at +0x0C");
static_assert(offsetof(BrnCpuMonitors, miUT_Replay) == 0x40, "Replay handle at +0x40");
static_assert(offsetof(BrnCpuMonitors, miUT_SoundUpdate) == 0x9C, "last handle at +0x9C");

void BrnGlobalCpuMonitors_embed_check()
{
    BrnCpuMonitors mon;
    mon.Construct();
    // The +0x08 handle is intentionally not seeded by ARTIST Construct; this
    // compile-only probe checks its layout and does not read its value.
    (void)mon.miUT_TotalUpdate;
    (void)mon.miUT_SoundUpdate;
}
