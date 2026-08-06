#ifndef BRN_SERVER_INTERFACE_X360_H
#define BRN_SERVER_INTERFACE_X360_H

#include "types.hpp"
#include "GameSource/Network/BrnServerInterfaceBase.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfaceGamesX360.h"

// ===========================================================================
// BrnNetwork::BrnServerInterfaceX360
//   Home: GameSource/Network/X360/BrnServerInterfaceX360.{h,cpp}
//   (X360 assert path: "...\gamesource\unity\../Network/X360/BrnServerInterfaceX360.cpp")
//
// The X360 platform layer of the Burnout server interface: BrnServerInterfaceBase
// plus the ONE extra embedded component -- the Xbox-LIVE games interface
// CgsNetwork::ServerInterfaceGamesX360 -- and the four-stage Prepare/Release
// resume machines + the two CPU perfmon brackets around Update.
//
// HIERARCHY (measured, X360):
//   BrnServerInterfaceBase  <-  BrnServerInterfaceX360 (this)  <-  BrnServerInterface
// * The DecFIGS DWARF twin attests the same chain on PS3:
//   dwarfdump/GameSource/Network/BrnServerInterface.h:49
//     "BrnServerInterface : public BrnServerInterfacePS3" and
//   dwarfdump/GameSource/Network/PS3/BrnServerInterfacePS3.h:44
//     "BrnServerInterfacePS3 : public BrnServerInterfaceBase" -- with members
//     mGames / mePrepareStage / meReleaseStage and the same seven virtuals.
// * The single most-derived X360 vtable @ 0x820D0F40 holds
//   BrnServerInterface::`scalar deleting destructor' (0x827E4090) at slot 0 and
//   THIS class's seven overrides in the ServerInterfaceDirtySock slot order
//   (Construct +0x04, Prepare +0x08, Release +0x0C, Destruct +0x10,
//    Suspend +0x18, Resume +0x1C) plus the Brn-introduced
//   Update(PostSimulationInputBuffer*) overload slot at +0x28; the inherited
//   +0x14 Update() slot stays CgsNetwork::ServerInterfaceDirtySockX360::Update.
//
// THE +0xC2C COMPONENT IS THE GAMES INTERFACE (measured; de-forks the wave-11
// draft's invented "GamesServerInterfaceX360" type):
//   BrnNetworkManager's ctor @ 0x827E5530 constructs the interface at
//   manager+0x38E8, calls BrnServerInterfaceBase::BrnServerInterfaceBase, then
//     0x827E557C  stw off_820CFBF8, 0xC2C(r29)   ; games-leaf vtable -> +0xC2C
//     0x827E5580  stw off_820D0F40, 0(r29)       ; most-derived vtable -> +0
//   off_820CFBF8 IS CgsNetwork::ServerInterfaceGamesX360's vtable (18 slots,
//   Construct@+0x00 .. ReceivedGameEvent@+0x44; dumped 2026-08-04, see
//   scratchpad/waveL/ServerInterfaceX360.spec.md). Every games-interface
//   dispatch in this TU's asm resolves through it:
//     +0x00 Construct  +0x14 Destruct  +0x18 Prepare  +0x1C Release
//     +0x20 Update     +0x24 Suspend   +0x28 Resume
//   The +0x14/+0x18/+0x1C/+0x00 entries are the leaf's single-instruction
//   forwarding thunks (`b ServerInterfaceGames::X` @ 0x8287F738/0x8287F740/
//   0x828997B0/0x8288C0C8), so direct by-name member calls on the by-value
//   mGames reach the identical bodies.
//
// LAYOUT (X360 console offsets, 32-bit; host layout differs -- by-name only):
//   +0x000  BrnServerInterfaceBase sub-object            (0xC2C bytes)
//   +0xC2C  mGames (ServerInterfaceGamesX360, 0x890)     ends exactly at +0x14BC
//   +0x14BC mePrepareStage                               (Construct zeroes it)
//   +0x14C0 meReleaseStage
//   +0x14C4 miNetworkServerInterfaceX360PM1              (assert names it, cpp:56)
//   +0x14C8 miNetworkServerInterfaceX360PM2              (assert names it, cpp:57)
//   sizeof == 0x14CC, proven end-to-end by the manager ctor: the interface lives
//   at manager+0x38E8 and the NEXT member's vtable is installed at
//   manager+0x4DB4 (0x827E5584), 0x4DB4 - 0x38E8 == 0x14CC. BrnServerInterface
//   adds no data of its own.
//   The PM handle pair is X360-only (absent from the PS3 DWARF twin) --
//   attested here by the "miNetworkServerInterfaceX360PM1 >= 0" /
//   "...PM2 >= 0" assert-expression strings in Construct.
//
// The four-stage machines (values from the Prepare/Release jumptable immediates;
// names from the PS3 DWARF twin, verbatim): Prepare runs Base then Games and
// resets meReleaseStage on DONE; Release runs Games then Base (reverse teardown)
// and resets mePrepareStage on DONE.
// ===========================================================================

namespace BrnNetwork
{
    class BrnServerInterfaceX360 : public BrnServerInterfaceBase
    {
    public:
        // BrnServerInterfacePS3.h:47 (DWARF twin; values == the X360 jumptable cases)
        enum EPrepareStage
        {
            E_PREPARESTAGE_START           = 0,
            E_PREPARESTAGE_BASECLASS       = 1,
            E_PREPARESTAGE_GAMES_COMPONENT = 2,
            E_PREPARESTAGE_DONE            = 3
        };

        // BrnServerInterfacePS3.h:55 (DWARF twin; values == the X360 jumptable cases)
        enum EReleaseStage
        {
            E_RELEASESTAGE_START           = 0,
            E_RELEASESTAGE_GAMES_COMPONENT = 1,
            E_RELEASESTAGE_BASECLASS       = 2,
            E_RELEASESTAGE_DONE            = 3
        };

        // The X360 build emits no standalone ctor/dtor for this intermediate class
        // (both fold into BrnNetworkManager's ctor / BrnServerInterface's deleting
        // dtor), so the ctor is inline-empty per the Base's convention and the
        // destructor stays implicit.
        BrnServerInterfaceX360();

        // Lifecycle overrides (ServerInterfaceDirtySock vtable slots; bodies in
        // the sibling BrnServerInterfaceX360.cpp).
        virtual void Construct();                                                      // 0x8258B100
        virtual bool Prepare( CgsNetwork::ServerInterfacePrepareParams * lpPrepareParams ); // 0x8258B200
        virtual bool Release();                                                        // 0x8258B2E0
        virtual void Destruct();                                                       // 0x8258B3C8
        virtual void Update( const BrnNetworkModuleIO::PostSimulationInputBuffer * lpInput ); // 0x82593AB0
        virtual void Suspend( int32_t liUpdateFlags );                                 // 0x82586618
        virtual void Resume();                                                         // 0x82586670

    private:
        // The one extra server-interface component this platform layer adds
        // (console +0xC2C; the manager ctor installs its leaf vtable there).
        CgsNetwork::ServerInterfaceGamesX360 mGames;

        EPrepareStage mePrepareStage;               // +0x14BC (console)
        EReleaseStage meReleaseStage;               // +0x14C0
        s32 miNetworkServerInterfaceX360PM1;        // +0x14C4 "Int - Base Update"  monitor handle
        s32 miNetworkServerInterfaceX360PM2;        // +0x14C8 "Int - Games Update" monitor handle
    };

    inline BrnServerInterfaceX360::BrnServerInterfaceX360()
    {
    }
}

#endif // BRN_SERVER_INTERFACE_X360_H
