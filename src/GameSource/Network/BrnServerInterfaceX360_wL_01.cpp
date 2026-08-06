#include "GameSource/Network/X360/BrnServerInterfaceX360.h"

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
//          (a) Feb-2007 source -- highest authority, and it DOES cover this type:
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
//         grounded anywhere. Feb-2007 CgsServerInterfaceGames.h:117 and DWARF
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
    // liUpdateFlags: `mr r4, r30` @0x82586648 materialises the stashed flags for the
    // `bl BrnServerInterfaceBase::Suspend` @0x82586650, and NOTHING materialises r4 for
    // the component bctrl @0x82586644 (r4 is live there only because it is this
    // function's own untouched incoming parameter -- a non-signal). The argument on the
    // mGames call below is therefore NOT measured: it is carried solely to satisfy the
    // committed leaf header's `virtual void Suspend(s32)`, which the Feb-2007 source,
    // the DWARF and the callee body all contradict. See the +0x24 note in the banner;
    // the correct shape is `mGames.Suspend();` and lands with the header fix.
    void BrnServerInterfaceX360::Suspend( int32_t liUpdateFlags )
    {
        mGames.Suspend( liUpdateFlags );   // FIXME(header): argument is spurious -- see banner +0x24
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
