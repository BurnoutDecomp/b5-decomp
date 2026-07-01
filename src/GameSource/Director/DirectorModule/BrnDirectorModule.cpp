// ============================================================================
// GameSource/Director/DirectorModule/BrnDirectorModule.cpp
//
// BrnDirector::DirectorModule -- the top-level director engine module. Of the 7
// functions this build's ARTIST export recovers for this TU, 3 are reconstructed here:
//   - Construct  @ 0x8225C590  [EXECUTED in goal trace]
//   - Destruct   @ 0x82250D98  [recovered; not executed in goal trace]
//   - Release    @ 0x82239198  [recovered; not executed in goal trace]
//
// Prepare (0x822712D8), Update (0x82275300), PreSceneQueryUpdate (0x8225C768) and the
// nested BrnDirector::CameraFinaliser::Update (0x82250440) remain declaration-only --
// every one of them reaches at least one un-landed callee (MainDirector::Prepare/
// Update/PreSceneQueryUpdate are themselves declaration-only in BrnMainDirector.h;
// BrnDirector::WorldMap has no homed type at all; BrnReplays::DirectorSerialiser /
// BrnDirector::ReplayDirector / BrnDirector::InertiaController /
// BrnDirector::KeyAnimShakeController are all [todo]) -- per AGENTS.md the
// reconstruction rules forbid bodying a function whose callee graph isn't resolved.
// See BrnDirectorModule.h for their per-function BLOCKED notes.
//
// LAYOUT: DirectorModule is modelled with NAMED members at asm-proven offsets (real
// committed sub-object types where they exist) + explicit padding for untouched spans
// -- see the header for the full rationale (this TU was previously reconstructed, and
// reverted after fresh-eyes review, as one opaque byte buffer accessed via raw
// reinterpret_cast offset pokes throughout; that pattern is not used here).
// ============================================================================

#include "GameSource/Director/BrnDirectorModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu::AddMonitor

namespace BrnDirector
{

// ----------------------------------------------------------------------------
// Construct  @ 0x8225C590  (EXECUTED in goal trace)
//
// Chains to the ModuleSingleBuffered base Construct, builds the debug component and the
// active camera, seeds the prepare/release stage words and a handful of scattered
// scratch fields the padding regions absorb, constructs the top-level MainDirector and
// the director replay serialiser, registers three CgsDev::PerfMonCpu monitors, and
// finally marks the module as a "new module" (the inherited CgsModule::Module base
// flag).
//
// FLAGS (see the header for the full writeup):
//   * mDirectorResourceManager.Construct() is currently a stub in the already-landed
//     BrnDirectorResourceManager.h -- the X360's inlined EventReceiverQueue<512,16>
//     init that this call site performs on the real binary is not reproduced by it.
//   * the X360 also stores &mDebugComponent into a word at this+0xB40 (inside
//     MainDirector's own opaque storage, +0x40 from MainDirector's own base) -- not
//     reproduced (no exposed field on MainDirector to write it through by name).
//   * the release-stage seed value is the literal 4 (asm-proven store), which is
//     EReleaseStage::E_RELEASESTAGE_MANAGER, not E_RELEASESTAGE_DONE -- see the Release
//     comment below for the same asm-literal-vs-DONE-enumerator observation.
//   * the 6th AddMonitor argument (liParentHandle) is read from a register the asm
//     never explicitly sets at two of the three call sites (a decompiler-visible
//     uninitialised-local artifact on the X360 side); passed as 0 here (the
//     conventional "no parent" sentinel for this API family).
// ----------------------------------------------------------------------------
void DirectorModule::Construct(f32 lfTime)
{
    CgsModule::ModuleSingleBuffered::Construct();

    mePrepareStage = E_PREPARESTAGE_START;                 // this+0x38B10 = 0
    meReleaseStage = static_cast<EReleaseStage>(4);         // this+0x38B14 = 4 (asm literal; see FLAG above)

    mDebugComponent.Construct(this);

    // FLAG: DirectorResourceManager::Construct() is currently a stub (see header FLAG);
    // the X360's inlined event-queue init at this call site is not reproduced.
    mDirectorResourceManager.Construct();

    mCamera.Construct();

    mMainDirector.Construct(&mDirectorResourceManager, lfTime);

    mDirectorSerialiser.Construct(
        /* liId               */ 4,
        /* liMode             */ 0,
        /* liBufferSize       */ 1024,
        /* liStaticBufferSize */ 384,
        /* lpcName            */ "DirectorModule",
        /* liFlag             */ 0);

    // Budget literals are the X360 rodata floats flt_82004014 (Pre SQ / Post Gui) and
    // flt_82001C98 (Main) -- both cross-confirmed elsewhere in this codebase as 0.1f and
    // 1.0f respectively (BrnMathUtils.cpp / BrnDirectorModuleDebugPrinter.cpp for
    // flt_82004014==0.1f; GameShared/.../CgsCamera.cpp for flt_82001C98==1.0f), not
    // decompiler guesses.
    miPerfCount_PreSQUpdate  = CgsDev::PerfMonCpu::AddMonitor("Pre SQ Update",   13, 0, 0.1, 0, 1);
    miPerfCount_MainUpdate   = CgsDev::PerfMonCpu::AddMonitor("Main Update",     13, 0, 1.0, 0, 1);
    miPerfCount_PostGuiUpdate = CgsDev::PerfMonCpu::AddMonitor("Post Gui Update", 13, 0, 0.1, 0, 1);

    mbIsNewModule = true;   // this+0x004 (inherited CgsModule::Module member)
}

// ----------------------------------------------------------------------------
// Release  @ 0x82239198  (recovered; not executed in goal trace)
//
// Staged RELEASE state machine, the same shape as MainDirector::Release: the stage word
// (meReleaseStage) selects the case; the cases fall through (0/1->2->3->4). On the
// first (stage 0 or 1) it releases the owned MainDirector; on the last (stage 3) it
// releases the ModuleSingleBuffered base and, on success, advances to stage 4 and
// clears mePrepareStage. An out-of-range stage asserts and reports failure. Faithful to
// the asm: NO added control flow; the fall-through chain matches the jump table.
//
// NOTE: the asm's final stage literal is 4, not EReleaseStage::E_RELEASESTAGE_DONE (6)
// -- kept faithful to the asm value rather than "corrected" to the DWARF DONE
// enumerator (this TU's Release only progresses local bookkeeping to stage 4; the
// WORLDMAP/DONE stages the DWARF enum lists are driven by other, not-yet-reconstructed
// code paths).
// ----------------------------------------------------------------------------
bool DirectorModule::Release()
{
    switch (meReleaseStage)
    {
    case 0:
    case 1:
        meReleaseStage = static_cast<EReleaseStage>(1);
        if (!mMainDirector.Release())
            return false;
        // fall through
    case 2:
        meReleaseStage = static_cast<EReleaseStage>(3);
        // fall through
    case 3:
        meReleaseStage = static_cast<EReleaseStage>(3);
        if (!CgsModule::ModuleSingleBuffered::Release())
            return false;
        meReleaseStage = static_cast<EReleaseStage>(4);
        mePrepareStage = E_PREPARESTAGE_START;
        return true;

    default:
        CGS_ASSERT(false, "Invalid Stage\n");
        return false;
    }
}

// ----------------------------------------------------------------------------
// Destruct  @ 0x82250D98  (recovered; not executed in goal trace)
//
// Destructs the owned MainDirector, then the ModuleSingleBuffered base. Faithful to the
// asm (two calls, in order, no other work).
// ----------------------------------------------------------------------------
void DirectorModule::Destruct()
{
    mMainDirector.Destruct();
    CgsModule::ModuleSingleBuffered::Destruct();
}

}
