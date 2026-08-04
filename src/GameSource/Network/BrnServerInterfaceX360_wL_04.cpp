
#include "GameSource/Network/X360/BrnServerInterfaceX360.h"                  // BrnNetwork::BrnServerInterfaceX360
#include "GameShared/GameClasses/Core/CgsAssert.h"                           // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"    // CgsDev::PerfMonCpu

namespace BrnNetwork
{

// ---------------------------------------------------------------------------
// BrnNetwork::BrnServerInterfaceX360::Construct @ 0x8258B100
//   (most-derived vtable off_820D0F40 slot +0x04)
//
// Bring up the platform layer: construct the base interface, arm the Prepare
// state machine, construct the embedded Xbox-LIVE games component, then register
// the two CPU perfmon monitors whose handles Update() brackets with.
//
// RAW ASM (measured, complete -- prologue/epilogue elided):
//     0x8258B118  mr    r31, r3                       ; this
//     0x8258B11C  bl    BrnNetwork::BrnServerInterfaceBase::Construct
//     0x8258B120  lwz   r11, 0xC2C(r31)               ; mGames' vptr
//     0x8258B124  li    r10, 0
//     0x8258B128  addi  r3, r31, 0xC2C                ; &mGames
//     0x8258B12C  lwz   r11, 0(r11)                   ; games vtable slot +0x00 == Construct()
//     0x8258B130  stw   r10, 0x14BC(r31)              ; mePrepareStage = 0 (E_PREPARESTAGE_START)
//     0x8258B134  mtctr r11
//     0x8258B138  bctrl                               ; mGames.Construct()
//     0x8258B13C  lis   r11, aIntBaseUpdate@ha
//     0x8258B140  li    r7,  1                        ; lbLibPerfTagged = true
//     0x8258B144  addi  r3,  r11, aIntBaseUpdate@l    ; "Int - Base Update"
//     0x8258B148  lis   r11, flt_8200426C@ha
//     0x8258B14C  li    r5,  0                        ; lbMinimum = false
//     0x8258B150  li    r4,  9                        ; lePage = 9
//     0x8258B154  lfs   f31, flt_8200426C@l(r11)      ; lfCpuBudget
//     0x8258B158  fmr   f1,  f31
//     0x8258B15C  bl    CgsDev::PerfMonCpu::AddMonitor
//     0x8258B160  mr    r10, r3
//     0x8258B164  fmr   f1,  f31                      ; same budget again
//     0x8258B168  lis   r11, aIntGamesUpdate@ha
//     0x8258B16C  li    r7,  1
//     0x8258B170  addi  r3,  r11, aIntGamesUpdate@l   ; "Int - Games Update"
//     0x8258B174  li    r5,  0
//     0x8258B178  li    r4,  9
//     0x8258B17C  stw   r10, 0x14C4(r31)              ; miNetworkServerInterfaceX360PM1 = <first result>
//     0x8258B180  bl    CgsDev::PerfMonCpu::AddMonitor
//     0x8258B184  lwz   r11, 0x14C4(r31)
//     0x8258B188  stw   r3,  0x14C8(r31)              ; miNetworkServerInterfaceX360PM2 = <second result>
//     0x8258B18C  cmpwi cr6, r11, 0
//     0x8258B190  lis   r11, aDP4B5MainBurno_188@ha
//     0x8258B194  addi  r30, r11, aDP4B5MainBurno_188@l   ; "d:\p4\b5_main\...\Network\X360\BrnServerInterfaceX360.cpp"
//     0x8258B198  bge   cr6, loc_8258B1B8
//     0x8258B19C  bl    CgsDev::Assert::BeginAssert
//     0x8258B1A4  li    r5,  0x38                     ; line 56
//     0x8258B1A8  addi r3, r11, aMinetworkserve_11@l  ; "miNetworkServerInterfaceX360PM1 >= 0"
//     0x8258B1B0  bl    CgsDev::Assert::FireAssert
//     0x8258B1B4  bl    CgsDev::Assert::EndAssert
//   loc_8258B1B8:
//     0x8258B1B8  lwz   r11, 0x14C8(r31)
//     0x8258B1BC  cmpwi cr6, r11, 0
//     0x8258B1C0  bge   cr6, loc_8258B1E0
//     0x8258B1C4  bl    CgsDev::Assert::BeginAssert
//     0x8258B1CC  li    r5,  0x39                     ; line 57
//     0x8258B1D0  addi r3, r11, aMinetworkserve_10@l  ; "miNetworkServerInterfaceX360PM2 >= 0"
//     0x8258B1D8  bl    CgsDev::Assert::FireAssert
//     0x8258B1DC  bl    CgsDev::Assert::EndAssert
//   loc_8258B1E0:  ... epilogue ... blr                ; no result is formed -> void
//
// MEASURED FACTS:
//   * flt_8200426C == 0x40A00000 == 5.0f. Read out of the ARTIST database with
//     headless IDA this wave (its neighbours are 0.0f @0x82004268 and 3.0f
//     @0x82004270), so the budget is a measured constant, not a guess.
//   * The two assert-expression strings and the two line numbers (0x38 == 56,
//     0x39 == 57) are verbatim from rodata; they are also the ONLY attestation of
//     the two handle members' names, which is why the header spells them exactly
//     so.
//   * mePrepareStage (console +0x14BC) is stored 0 and meReleaseStage
//     (console +0x14C0) is NOT touched anywhere in this function -- the release
//     machine is armed later, by Prepare's DONE case.
//   * BOTH guards are integer `cmpwi`/`bge` on a signed handle, NOT `fcmpu`, so
//     the PPC unordered/NaN polarity trap does not apply here: `bge cr6` after
//     `cmpwi cr6, r11, 0` is exactly C++ `>= 0`.
//
// INFERENCE (grounded, but not directly visible in this function's bytes):
//   * The two AddMonitor calls are the FIVE-argument
//     AddMonitor(name, page, minimum, budget, libPerfTagged). r6 is never written
//     in this function: on the PPC ABI the f1 budget argument consumes -- and
//     SKIPS -- the r6 GPR slot, which is why Hex-Rays invents a phantom 6th
//     argument here. Settled in CgsPerfMonCpu.h:96-109 off VehicleManager::
//     Construct's thirty identically shaped calls. There is no parent-handle
//     argument to supply.
//   * Games vtable slot +0x00 is CgsNetwork::ServerInterfaceGamesX360::Construct
//     @0x8287F738, a single-instruction `b ServerInterfaceGames::Construct`
//     thunk (vtable dumped this wave, see
//     scratchpad/waveL/ServerInterfaceX360.spec.md), so the by-name member call
//     below reaches the identical body. mGames' dynamic type is pinned by
//     BrnNetworkManager's ctor (0x827E557C stw off_820CFBF8, 0xC2C(r29)).
//   * Returns void: the epilogue forms no result and every call is a plain `bl`.
//     The dossier's `return CgsDev::Assert::EndAssert();` is a Hex-Rays artefact
//     of EndAssert leaving its own r3 live.
//
// The console byte offsets above (+0xC2C, +0x14BC, +0x14C4, +0x14C8) are 32-bit
// X360 layout facts, quoted only to identify which member each access touches.
// They are NOT reproduced on the host -- vptrs and pointers widen under LLP64, so
// the same members sit elsewhere. The body is a pure by-name member walk.
// ---------------------------------------------------------------------------
void BrnServerInterfaceX360::Construct()
{
    BrnServerInterfaceBase::Construct();

    // Arm the prepare machine before the component is built (the store is
    // scheduled between the vtable load and the call, but it is ordered before
    // the call itself).
    mePrepareStage = E_PREPARESTAGE_START;
    mGames.Construct();                          // games vtable +0x00 -> thunk -> ServerInterfaceGames::Construct

    // Five-argument AddMonitor: (name, page, minimum, cpuBudget, libPerfTagged).
    // page 9 == `li r4, 9`; minimum false == `li r5, 0`; budget 5.0f ==
    // flt_8200426C; libPerfTagged true == `li r7, 1`.
    miNetworkServerInterfaceX360PM1 =
        CgsDev::PerfMonCpu::AddMonitor( "Int - Base Update",  CgsDev::E_PMP_9, false, 5.0f, true );
    miNetworkServerInterfaceX360PM2 =
        CgsDev::PerfMonCpu::AddMonitor( "Int - Games Update", CgsDev::E_PMP_9, false, 5.0f, true );

    // The registry returns -1 when it is full or unbuilt; both brackets in
    // Update() would then be dead. (X360 BrnServerInterfaceX360.cpp lines 56/57.)
    CGS_ASSERT( miNetworkServerInterfaceX360PM1 >= 0, "miNetworkServerInterfaceX360PM1 >= 0" );
    CGS_ASSERT( miNetworkServerInterfaceX360PM2 >= 0, "miNetworkServerInterfaceX360PM2 >= 0" );
}

}
