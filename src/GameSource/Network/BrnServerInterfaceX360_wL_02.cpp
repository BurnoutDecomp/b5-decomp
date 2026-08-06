#include "GameSource/Network/X360/BrnServerInterfaceX360.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"   // CgsDev::PerfMonCpu

// ===========================================================================
// Reconstructed from BURNOUT_X360_ARTIST.XEX (wave L, group 2 partfile 02)
//   BrnNetwork::BrnServerInterfaceX360::Update @ 0x82593AB0
//
// This partfile carries ONLY Update. Its sibling in the same group,
// BrnServerInterfaceX360::Construct @ 0x8258B100, is complete but cannot be
// compiled yet: it needs the perfmon page enumerator the X360 passes in r4
// (`li r4, 9`), and CgsDev::PerfMonCpuPage
// (GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h:36-41)
// currently declares only E_PMP_GENERAL / E_PMP_4 / E_PMP_MAX. The finished
// body is parked verbatim at
//   scratchpad/waveL/parked/BrnServerInterfaceX360_02_Construct.cpp
// and merges the moment `E_PMP_9 = 9,` lands in that enum.
//
// The X360 console byte offsets quoted in the comments below (+0xC2C, +0x14C4,
// +0x14C8) are 32-bit-console layout facts used only to IDENTIFY which member
// each access touches. They are NOT reproduced on the host: every pointer and
// vptr widens under LLP64, so the same members sit at different byte offsets
// here. The code is a by-name member walk; nothing depends on the immediates.
// ===========================================================================

namespace BrnNetwork
{

// ---------------------------------------------------------------------------
// BrnNetwork::BrnServerInterfaceX360::Update @ 0x82593AB0
//
// The Brn-introduced Update(PostSimulationInputBuffer*) overload (most-derived
// vtable off_820D0F40 slot +0x28). It is the platform layer's perfmon
// instrumentation point: two CPU-monitor brackets, one around the whole base
// interface update, one around the embedded Xbox-LIVE games component's update.
//
// RAW ASM (measured, complete -- prologue/epilogue elided):
//     0x82593AC4  mr    r31, r3              ; this
//     0x82593AC8  mr    r30, r4              ; lpInput (passed straight through)
//     0x82593ACC  lwz   r3, 0x14C4(r31)      ; miNetworkServerInterfaceX360PM1
//     0x82593AD0  bl    CgsDev::PerfMonCpu::StartMonitor
//     0x82593AD4  mr    r4, r30
//     0x82593AD8  mr    r3, r31
//     0x82593ADC  bl    BrnNetwork::BrnServerInterfaceBase::Update
//     0x82593AE0  lwz   r3, 0x14C4(r31)
//     0x82593AE4  bl    CgsDev::PerfMonCpu::StopMonitor
//     0x82593AE8  lwz   r3, 0x14C8(r31)      ; miNetworkServerInterfaceX360PM2
//     0x82593AEC  bl    CgsDev::PerfMonCpu::StartMonitor
//     0x82593AF0  lwz   r11, 0xC2C(r31)      ; mGames' vptr
//     0x82593AF4  addi  r3, r31, 0xC2C       ; &mGames
//     0x82593AF8  lwz   r11, 0x20(r11)       ; games vtable slot +0x20 == Update()
//     0x82593AFC  mtctr r11
//     0x82593B00  bctrl
//     0x82593B04  lwz   r3, 0x14C8(r31)
//     0x82593B08  bl    CgsDev::PerfMonCpu::StopMonitor
//     0x82593B20  blr                        ; no result is formed -> void
//
// MEASURED: the argument register that feeds each Start/StopMonitor call is a
// reload of the handle field (never a cached copy), the base update is reached
// by a DIRECT `bl` (a qualified, non-virtual call), and the games update is a
// vtable dispatch on the by-value mGames sub-object.
//
// INFERENCE (grounded, but not directly visible in this function's bytes):
//   * slot +0x20 of the games-leaf vtable off_820CFBF8 is
//     CgsNetwork::ServerInterfaceGamesX360::Update @ 0x8287F748 -- a real body,
//     not a forwarding thunk (vtable dumped this wave, see
//     scratchpad/waveL/ServerInterfaceX360.spec.md). mGames' dynamic type is
//     fixed at ServerInterfaceGamesX360 by BrnNetworkManager's ctor
//     (0x827E557C stw off_820CFBF8, 0xC2C(r29)), so the by-name member call
//     below reaches the identical body.
//   * The trailing `bl StopMonitor` is a plain call, not a tail call, and the
//     epilogue forms no return value -- so this returns void. The dossier's
//     `return CgsDev::PerfMonCpu::StopMonitor(...)` is a Hex-Rays artefact of
//     StopMonitor leaving its own r3 live across the epilogue.
// ---------------------------------------------------------------------------
void BrnServerInterfaceX360::Update( const BrnNetworkModuleIO::PostSimulationInputBuffer * lpInput )
{
    // Bracket 1 -- "Int - Base Update" (the handle Construct parks in PM1).
    CgsDev::PerfMonCpu::StartMonitor( miNetworkServerInterfaceX360PM1 );
    BrnServerInterfaceBase::Update( lpInput );
    CgsDev::PerfMonCpu::StopMonitor( miNetworkServerInterfaceX360PM1 );

    // Bracket 2 -- "Int - Games Update" (PM2), around the embedded Xbox-LIVE
    // games component's own update (games vtable slot +0x20).
    CgsDev::PerfMonCpu::StartMonitor( miNetworkServerInterfaceX360PM2 );
    mGames.Update();
    CgsDev::PerfMonCpu::StopMonitor( miNetworkServerInterfaceX360PM2 );
}

}
