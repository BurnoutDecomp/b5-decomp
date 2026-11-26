// FOLDED FROM BrnServerInterfaceX360_wL_01.cpp
#include "GameSource/Network/X360/BrnServerInterfaceX360.h"
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\""../Network/X360/BrnServerInterfaceX360.cpp"
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::BrnServerInterfaceX360::Destruct @ 0x8258B3C8
//   BrnNetwork::BrnServerInterfaceX360::Suspend  @ 0x82586618
//   BrnNetwork::BrnServerInterfaceX360::Resume   @ 0x82586670
//
// (PARTFILE: these three are wave-L group 1 of the seven bodies belonging to
//  GameSource/Network/X360/BrnServerInterfaceX360.cpp -- the X360 assert path is
//  "...\gamesource\unity\../Network/X360/BrnServerInterfaceX360.cpp". Construct /
//  Prepare / Release / Update land in the sibling partfiles; their extra includes
//  -- CgsAssert.h, CgsPerfMonCpu.h, CgsServerInterfacePrepareParams.h -- are
//  deliberately NOT pulled in here because none of these three functions touches
//  an assert, a perfmon handle or the prepare params.)
//
// All three are two-call forwarders that pair the ONE component this platform
// layer adds -- mGames (CgsNetwork::ServerInterfaceGamesX360, console +0xC2C) --
// with the BrnServerInterfaceBase behaviour. MEASURED from the raw asm: the
// component-vs-base ORDER differs per function and is NOT normalised here.
//
//   Destruct  0x8258B3DC  addi r3, r31, 0xC2C        ; this-> mGames
//             0x8258B3E0  lwz  r11, 0xC2C(r31)       ; mGames vptr
//             0x8258B3E4  lwz  r11, 0x14(r11)        ; leaf vtable slot +0x14
//             0x8258B3EC  bctrl                      ; -> component FIRST
//             0x8258B3F4  bl   BrnServerInterfaceBase::Destruct   ; base SECOND
//
//   Suspend   0x82586630  mr   r30, r4               ; stash liUpdateFlags
//             0x82586634  addi r3, r31, 0xC2C        ; this-> mGames
//             0x8258663C  lwz  r11, 0x24(r11)        ; leaf vtable slot +0x24
//             0x82586644  bctrl                      ; -> component FIRST. NOTE: no
//                                                    ;   `mr r4, ..` precedes this --
//                                                    ;   see the +0x24 note below
//             0x82586648  mr   r4, r30               ; flags materialised for the BASE
//             0x82586650  bl   BrnServerInterfaceBase::Suspend    ; base SECOND
//
//   Resume    0x82586684  bl   BrnServerInterfaceBase::Resume     ; base FIRST
//             0x8258668C  addi r3, r31, 0xC2C        ; this-> mGames
//             0x82586690  lwz  r11, 0x28(r11)        ; leaf vtable slot +0x28
//             0x82586698  bctrl                      ; -> component SECOND
//
// The 0xC2C / 0x14 / 0x24 / 0x28 immediates above are X360 32-bit-layout facts
// quoted for provenance only; nothing below reproduces them -- mGames is reached
// by name and the leaf overrides by name, so the host layout is free to differ.
//
// Vtable slot -> member-call mapping (from the dumped 18-slot leaf vtable
// off_820CFBF8, scratchpad/waveL/ServerInterfaceX360.spec.md):
//   +0x14 ServerInterfaceGamesX360::Destruct 0x8287F740 -- a single-instruction
//         `b CgsNetwork::ServerInterfaceGames::Destruct` thunk. The committed leaf
//         header does not declare a Destruct override (nothing but the thunk exists
//         to model), so the direct call below binds to the inherited non-virtual
//         ServerInterfaceGames::Destruct -- the identical body the thunk branches to.
//   +0x24 ServerInterfaceGamesX360::Suspend 0x8288C0D0 -- a REAL leaf body, and it
//         takes NO argument. The mGames call below is written WITH liUpdateFlags only
//         because the committed leaf header still declares Suspend(s32); that
//         declaration is a defect awaiting a header pass, and this call site must lose
//         its argument in the same change (`mGames.Suspend();`). Do NOT cite the r4
//         liveness at the 0x82586644 bctrl as evidence of an argument pass: r4 there is
//         just this function's own incoming parameter, untouched since entry, and no
//         compiler emits a redundant `mr r4, r4`. Three independent measurements say
//         no-arg:
//          (a) LegacySrc source -- highest authority, and it DOES cover this type:
//              GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//              CgsServerInterfaceGames.h:114 `virtual void Suspend();`. DWARF agrees
//              on the base (CgsServerInterfaceGames.h:196) and on the sibling platform
//              leaf (PS3/CgsServerInterfaceGamesPS3.h:51), both `virtual void
//              Suspend();`. DWARF carries no X360 tree for this component, so the X360
//              leaf's shape is taken from the base + the PS3 sibling.
//          (b) The callee body itself: 0x8288C0E0 `mr r31, r3`, then
//              `bl ServerInterfaceGames::FreeDisplayLists` @0x8288C0E4 -- which clobbers
//              volatile r4 immediately. The incoming r4 is never read, and r4 is
//              OVERWRITTEN at 0x8288C0F8 `lwz r4, 0x87C(r31)` and again at 0x8288C104
//              `li r4, 0`.
//          (c) The decisive counter-example, BrnServerInterfaceBase::Suspend @0x82582D38.
//              It makes the identical `mr r30, r4` stash @0x82582D50, then issues seven
//              component calls -- five that IDA symbolises as CgsCollision::
//              BaseCollisionGenerator::Destruct (ICF-folded empty bodies) @0x82582D58/
//              D60/D68/D70/D78, `bl ServerInterfaceDownloadableConfig::Suspend`
//              @0x82582D80, `bl ServerInterfaceUsersets::Suspend` @0x82582D88, plus a
//              vtable dispatch @0x82582D9C -- with r4 DEAD from that first `bl` onward
//              and NEVER re-materialised for any of them. (DWARF confirms both named
//              ones: CgsServerInterfaceDownloadableConfig.h:176 and
//              CgsServerInterfaceUsersets.h:205 are each `void Suspend();`.) It
//              re-materialises r4 exactly once -- `mr r4, r30` @0x82582DA8 -- right
//              before `bl ServerInterfaceDirtySockX360::Suspend` @0x82582DB0. So this
//              compiler materialises r4 precisely when the callee takes it, and the
//              absence of that materialisation at 0x82586644 is the real signal.
//   +0x28 ServerInterfaceGamesX360::Resume 0x8288C128 -- a REAL leaf body. The
//         committed leaf declares it returning void*; the caller here discards the
//         result (the asm ignores r3 after the bctrl), so it is called as a
//         statement. CAVEAT, same class of defect as +0x24: that void* return is not
//         grounded anywhere. LegacySrc CgsServerInterfaceGames.h and DWARF
//         CgsServerInterfaceGames.h:199 both give `virtual void Resume();`, and a
//         void*-returning leaf cannot legally override a void-returning base -- it
//         would HIDE it. The statement-call below is correct under either shape, so
//         nothing here changes when the header pass fixes the leaf.

namespace BrnNetwork
{
    // 0x8258B3C8 -- component teardown FIRST, then the base (mirrors the Release
    // machine's Games-then-Base order; measured, do not normalise against Prepare).
    void BrnServerInterfaceX360::Destruct()
    {
        mGames.Destruct();
        BrnServerInterfaceBase::Destruct();
    }

    // 0x82586618 -- component FIRST, then the base. Only the BASE receives
    // The games component's Suspend takes no argument; only the base call receives the flags.
    void BrnServerInterfaceX360::Suspend( int32_t liUpdateFlags )
    {
        mGames.Suspend();
        BrnServerInterfaceBase::Suspend( liUpdateFlags );
    }

    // 0x82586670 -- base FIRST, then the component. This is the one of the three
    // whose order is inverted; it is measured (the `bl` precedes the `bctrl`).
    void BrnServerInterfaceX360::Resume()
    {
        BrnServerInterfaceBase::Resume();
        mGames.Resume();
    }
}

// FOLDED FROM BrnServerInterfaceX360_wL_02.cpp
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

// FOLDED FROM BrnServerInterfaceX360_wL_03.cpp
#include "GameSource/Network/X360/BrnServerInterfaceX360.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::BrnServerInterfaceX360::Release @ 0x8258B2E0
//
// Wave-L partfile: the eventual home of this body is the sibling
// GameSource/Network/X360/BrnServerInterfaceX360.cpp (the X360 assert path in the
// binary is "...\gamesource\unity\../Network/X360/BrnServerInterfaceX360.cpp");
// it is landed here as a per-implementer partfile purely to avoid write
// collisions inside this wave, and is meant to be folded into that .cpp.
//
// NOT IN THIS FILE -- BrnServerInterfaceX360::Prepare @ 0x8258B200 is the mirror
// of this machine and belongs beside it, but its very first (unconditional) store
// parks &mGames in the prepare-params block and the committed
// CgsNetwork::ServerInterfacePrepareParams still models that word as part of an
// opaque `u8 maHead[48]`, so the body cannot name the field. It is reconstructed
// in full and parked at
// scratchpad/waveL/parked/BrnServerInterfaceX360_03_Prepare.cpp with the exact
// header lines that would unblock it.
//
// ---------------------------------------------------------------------------
// Release() -- the four-stage resumable teardown machine (measured, 0x8258B2E0)
//
// It is the exact MIRROR of Prepare's machine: Prepare runs Base THEN Games and
// clears meReleaseStage when it reaches DONE; Release runs Games THEN Base
// (reverse-order teardown) and clears mePrepareStage when it reaches DONE. Both
// cross-resets are measured, not inferred -- keep both.
//
// The X360 codegen is a 4-entry jump table on meReleaseStage (console +0x14C0)
// with the classic resume-machine FALL-THROUGH shape: each case first re-stores
// its own stage value (so a re-entry after a `false` return resumes at exactly
// that step), then does its one piece of work, then falls into the next case.
// A stage that returns false leaves meReleaseStage parked on itself.
//
//   0x8258B2F8  lwz   r11, 0x14C0(r31)          ; switch (meReleaseStage)
//   0x8258B2FC  cmplwi cr6, r11, 3              ; 4 cases; >3 -> default
//   0x8258B300  bgt   cr6, def_8258B31C
//   0x8258B304  li    r30, 0                    ; the constant 0 reused below
//   -- case 0 (E_RELEASESTAGE_START) --
//   0x8258B330  stw   r30, 0x14C0(r31)          ; meReleaseStage = START (re-store)
//   -- case 1 (E_RELEASESTAGE_GAMES_COMPONENT) --  falls in from case 0
//   0x8258B334  lwz   r11, 0xC2C(r31)           ; mGames' vptr
//   0x8258B338  li    r10, 1
//   0x8258B33C  addi  r3,  r31, 0xC2C           ; this-> &mGames
//   0x8258B340  lwz   r11, 0x1C(r11)            ; vtable slot +0x1C == Games::Release
//   0x8258B344  stw   r10, 0x14C0(r31)          ; meReleaseStage = GAMES_COMPONENT
//   0x8258B348  mtctr r11                       ;   (stored BEFORE the call)
//   0x8258B34C  bctrl                           ; mGames.Release()
//   0x8258B350  clrlwi r11, r3, 24              ; bool return (byte-truncated)
//   0x8258B358  beq   cr6, loc_8258B3AC         ; !ok -> return false
//   -- case 2 (E_RELEASESTAGE_BASECLASS) --      falls in from case 1
//   0x8258B35C  li    r11, 2
//   0x8258B360  mr    r3,  r31                  ; `this`
//   0x8258B364  stw   r11, 0x14C0(r31)          ; meReleaseStage = BASECLASS
//   0x8258B368  bl    BrnServerInterfaceBase::Release
//   0x8258B36C  clrlwi r11, r3, 24              ; bool return
//   0x8258B374  beq   cr6, loc_8258B3AC         ; !ok -> return false
//   -- case 3 (E_RELEASESTAGE_DONE) --           falls in from case 2
//   0x8258B378  li    r11, 3
//   0x8258B37C  stw   r30, 0x14BC(r31)          ; mePrepareStage = E_PREPARESTAGE_START
//   0x8258B380  li    r3,  1                    ; return true
//   0x8258B384  stw   r11, 0x14C0(r31)          ; meReleaseStage = DONE
//   -- default --
//   0x8258B38C  bl    CgsDev::Assert::BeginAssert
//   0x8258B394  li    r5,  0xB5                 ; __LINE__ == 181
//   0x8258B398  addi  r4,  ..., "d:\p4\b5_main\burnout\main\code\gamesource\..."
//   0x8258B3A0  addi  r3,  ..., "0"             ; the assert EXPRESSION string
//   0x8258B3A4  bl    CgsDev::Assert::FireAssert
//   0x8258B3A8  bl    CgsDev::Assert::EndAssert
//   0x8258B3AC  li    r3, 0                     ; ...and fall out returning false
//
// The default arm falls into the shared `r3 = 0` epilogue, so the assert does NOT
// return early -- it fires and then returns false, which is what CGS_ASSERT +
// `return false` reproduces.
//
// Console-vs-host note (measured offsets are COMMENTS only): 0xC2C / 0x14BC /
// 0x14C0 are 32-bit-pointer X360 layout facts. Nothing here reproduces them --
// every access is a by-name member access, so the host's (wider) layout is used.
//
// Dispatch note: the asm reaches Games::Release through mGames' own vtable slot
// +0x1C. That slot holds CgsNetwork::ServerInterfaceGamesX360::Release
// (0x8288C0C8), which is a single-instruction `b ServerInterfaceGames::Release`
// thunk -- so the direct by-name call on the by-value member is semantically
// identical (see scratchpad/waveL/ServerInterfaceX360.spec.md for the full
// 18-slot vtable dump).
// ---------------------------------------------------------------------------

namespace BrnNetwork
{
    bool BrnServerInterfaceX360::Release()
    {
        switch ( meReleaseStage )
        {
        case E_RELEASESTAGE_START:
            // Re-store of the stage the switch just matched (asm case 0). It is
            // redundant on the value but it IS emitted, and it is what makes each
            // arm a self-contained resume point.
            meReleaseStage = E_RELEASESTAGE_START;
            // fall through

        case E_RELEASESTAGE_GAMES_COMPONENT:
            meReleaseStage = E_RELEASESTAGE_GAMES_COMPONENT;
            if ( !mGames.Release() )
            {
                return false;
            }
            // fall through

        case E_RELEASESTAGE_BASECLASS:
            meReleaseStage = E_RELEASESTAGE_BASECLASS;
            if ( !BrnServerInterfaceBase::Release() )
            {
                return false;
            }
            // fall through

        case E_RELEASESTAGE_DONE:
            // Arm the MIRROR machine for the next Prepare (measured: stw of the
            // same zero register to +0x14BC, before the meReleaseStage store).
            mePrepareStage = E_PREPARESTAGE_START;
            meReleaseStage = E_RELEASESTAGE_DONE;
            return true;

        default:
            // BrnServerInterfaceX360.cpp:181 -- expression string "0" verbatim.
            CGS_ASSERT_AT( false,
                "0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
                    "../Network/X360/BrnServerInterfaceX360.cpp", 181 /* ARTIST 0x8258B394 */);

            return false;
        }
    }
}

// FOLDED FROM BrnServerInterfaceX360_wL_04.cpp

#include "GameSource/Network/X360/BrnServerInterfaceX360.h"                  // BrnNetwork::BrnServerInterfaceX360
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                           // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"    // CgsDev::PerfMonCpu

// Asserts pass __FILE__; ARTIST's reads "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Network/X360/BrnServerInterfaceX360.cpp".

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
//     argument here. Settled off VehicleManager::Construct's thirty
//     identically shaped calls. There is no parent-handle
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
    CGS_ASSERT_AT( miNetworkServerInterfaceX360PM1 >= 0,
        "miNetworkServerInterfaceX360PM1 >= 0", __FILE__, 56 /* ARTIST 0x8258B1A4 */);

    CGS_ASSERT_AT( miNetworkServerInterfaceX360PM2 >= 0,
        "miNetworkServerInterfaceX360PM2 >= 0", __FILE__, 57 /* ARTIST 0x8258B1CC */);

}

}

// FOLDED FROM BrnServerInterfaceX360_wL_05.cpp

#include "GameSource/Network/X360/BrnServerInterfaceX360.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePrepareParams.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"  // CgsNetwork::E_COMPONENTS_GAMES

namespace BrnNetwork
{
    bool BrnServerInterfaceX360::Prepare( CgsNetwork::ServerInterfacePrepareParams * lpPrepareParams )
    {
        // Unconditional, and BEFORE the switch: hand the Prepare chain the one
        // component this platform layer adds. The X360 is
        // `addi r30, r31, 0xC2C ; stw r30, 4(r4)` -- the +4 destination is slot 1 of the
        // params' component array, i.e. mapComponents[E_COMPONENTS_GAMES]. An earlier
        // draft named that storage as a scalar mpComponentToPrepare; the DWARF has no such
        // member and types it as ServerInterfaceComponent*[12], so the slot is addressed
        // by its enumerator here rather than by the console's byte offset.
        lpPrepareParams->mapComponents[CgsNetwork::E_COMPONENTS_GAMES] = &mGames;

        switch ( mePrepareStage )
        {
        case E_PREPARESTAGE_START:
            // Re-store of the stage the switch just matched (asm case 0). Redundant
            // on the value, but emitted -- it is what makes each arm a self-contained
            // resume point.
            mePrepareStage = E_PREPARESTAGE_START;
            // fall through

        case E_PREPARESTAGE_BASECLASS:
            mePrepareStage = E_PREPARESTAGE_BASECLASS;
            if ( !BrnServerInterfaceBase::Prepare( lpPrepareParams ) )
            {
                return false;
            }
            // fall through

        case E_PREPARESTAGE_GAMES_COMPONENT:
            mePrepareStage = E_PREPARESTAGE_GAMES_COMPONENT;
            if ( !mGames.Prepare( this ) )
            {
                return false;
            }
            // fall through

        case E_PREPARESTAGE_DONE:
            // Arm the MIRROR machine for the next Release (measured: stw of the same
            // zero register to +0x14C0, before the mePrepareStage store).
            meReleaseStage = E_RELEASESTAGE_START;
            mePrepareStage = E_PREPARESTAGE_DONE;
            return true;

        default:
            // BrnServerInterfaceX360.cpp:120 -- expression string "0" verbatim.
            CGS_ASSERT_AT( false,
                "0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
                    "../Network/X360/BrnServerInterfaceX360.cpp", 120 /* ARTIST 0x8258B2BC */);

            return false;
        }
    }
}

#undef CGS_ARTIST_FILE
