#include "GameSource/Gui/Flapt/BrnFlaptManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"   // CgsDev::PerfMonCpu

// BrnFlapt::FlaptManager member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU (class:BrnFlapt::FlaptManager) bodies the one
// X360-emitted function:
//
//   GetFile @ 0x82473078
//
// X360 body: forms the element address `&maFlaptFileInstances[luFile]`
// (mulli r5,0x34 ; add base ; addi 8 — i.e. this + 8 + 52*index), asserts the
// entry is active ("maFlaptFileInstances[leFile].IsActive()") and that the
// element pointer is non-null ("lpFileInst", the inlined FileRef constructor),
// then writes the FileRef into the caller-provided out buffer
// (`*out = &maFlaptFileInstances[luFile]`). The X360-baked file/line cites are
// discarded per project convention.
//
// Note the asm reads the IsActive byte from element+0 (lbz 0(r31)); since the
// element pointer is taken as &maFlaptFileInstances[luFile], element+0 is the
// FlaptFileInstance's mbIsActive flag.

namespace BrnFlapt
{

// ---- GetFile @ 0x82473078 ------------------------------------------------
FileRef* FlaptManager::GetFile(FileRef* lpOutRef, u32 luFile)
{
    FlaptFileInstance* lpFileInst = &maFlaptFileInstances[luFile];

    CGS_ASSERT(lpFileInst->mbIsActive, "maFlaptFileInstances[leFile].IsActive()");
    CGS_ASSERT(lpFileInst != 0, "lpFileInst");

    lpOutRef->mpFileInstance = lpFileInst;
    return lpOutRef;
}

// FLAG: the two CPU perf-monitor handles bracketing the flapt UPDATE region (the X360
// read them from the globals dword_82F2765C / dword_82FB3B0C). They are registered via
// CgsDev::PerfMonCpu::AddMonitor by the perf-monitor setup TU; declared extern here so
// the bracket compiles (the per-TU gate does not link). The console wraps the same
// region in both monitors (a specific + an enclosing total).
extern s32 giFlaptUpdateMonitor;        // dword_82F2765C
extern s32 giFlaptUpdateMonitorTotal;   // dword_82FB3B0C

// ---- Update @ 0x82472120 -------------------------------------------------
// Per-frame tick: bracket the work in the two CPU perf monitors, then -- if the single
// (HUD) file instance is active -- advance it by the time step. The X360 returns the
// inner StopMonitor's r3; the caller (BrnGui::ViewModule::Update) ignores it, and the
// header declares Update void, so the return is dropped.
void FlaptManager::Update(f32 lfTimeStep)
{
    CgsDev::PerfMonCpu::StartMonitor(giFlaptUpdateMonitor);
    const s32 liTotalMonitor = giFlaptUpdateMonitorTotal;
    CgsDev::PerfMonCpu::StartMonitor(liTotalMonitor);

    if (maFlaptFileInstances[0].mbIsActive)
        maFlaptFileInstances[0].Update(lfTimeStep);

    CgsDev::PerfMonCpu::StopMonitor(giFlaptUpdateMonitor);
    CgsDev::PerfMonCpu::StopMonitor(liTotalMonitor);
}

}
