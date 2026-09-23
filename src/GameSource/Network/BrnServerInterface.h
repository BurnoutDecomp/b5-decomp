#ifndef BRN_SERVER_INTERFACE_H
#define BRN_SERVER_INTERFACE_H

#include "types.hpp"
#include "GameSource/Network/X360/BrnServerInterfaceX360.h"

namespace CgsNetwork
{
    class ServerInterfaceConnection;   // GetConnectionComponent() return type (pointer only)
    class ServerInterfaceGames;        // GetGameComponent() return type (pointer only)
    class ServerInterfacePlayerInfo;   // GetPlayerInfoComponent() return type (pointer only)
    class ServerInterfaceServerInfo;   // GetServerInfoComponent() return type (pointer only)
    class ServerInterfaceHttp;         // GetHttpComponent() return type (pointer only)
    class ServerInterfacePingRegions;  // GetPingRegionsComponent() return type (pointer only)
}

// ===========================================================================
// BrnNetwork::BrnServerInterface
//   Home: GameSource/Network/BrnServerInterface.{h,cpp}
//
// The most-derived Burnout server-interface aggregate the game actually embeds
// (BrnNetworkManager::mServerInterface, living at manager+0x38E8). It adds NO data
// of its own: it is the X360 platform layer BrnServerInterfaceX360 plus this
// class's out-of-line virtual destructor, which anchors the single most-derived
// deleting-destructor thunk to this TU.
//
// HIERARCHY (measured on X360 -- CORRECTED 2026-08-04, see the correction below):
//   BrnServerInterfaceBase  <-  BrnServerInterfaceX360  <-  BrnServerInterface
//
// * BrnNetworkManager's ctor @ 0x827E5530 builds the whole object in place, the
//   intermediate's ctor fully inlined into it (the X360 emits no standalone
//   BrnServerInterfaceX360::BrnServerInterfaceX360):
//     0x827E5548  addi r29, r31, 0x38E8         ; the interface at manager+0x38E8
//     0x827E5550  bl   BrnServerInterfaceBase::BrnServerInterfaceBase
//     0x827E557C  stw  off_820CFBF8, 0xC2C(r29) ; ServerInterfaceGamesX360 vtable
//     0x827E5580  stw  off_820D0F40, 0(r29)     ; the most-derived vtable
//     0x827E5584  stw  off_820CEC0C, 0x4DB4(r31); the manager's NEXT member
//                                               ;  => sizeof == 0x4DB4-0x38E8 == 0x14CC
// * The most-derived vtable off_820D0F40 carries THIS class's deleting destructor
//   at slot 0 and BrnServerInterfaceX360's seven overrides, in the
//   CgsNetwork::ServerInterfaceDirtySock declaration order:
//     +0x00 BrnServerInterface::`scalar deleting destructor'           0x827E4090
//     +0x04 BrnServerInterfaceX360::Construct                          0x8258B100
//     +0x08 BrnServerInterfaceX360::Prepare                            0x8258B200
//     +0x0C BrnServerInterfaceX360::Release                            0x8258B2E0
//     +0x10 BrnServerInterfaceX360::Destruct                           0x8258B3C8
//     +0x14 CgsNetwork::ServerInterfaceDirtySockX360::Update           (inherited)
//     +0x18 BrnServerInterfaceX360::Suspend                            0x82586618
//     +0x1C BrnServerInterfaceX360::Resume                             0x82586670
//     +0x20 BrnServerInterfaceBase::OnEvent                            0x8259A358
//     +0x24 CgsNetwork::ServerInterfaceDirtySock::ConvertError         0x828759D8
//     +0x28 BrnServerInterfaceX360::Update(PostSimulationInputBuffer*) 0x82593AB0
//   It is the ONLY vtable referencing those seven functions, so they ARE the live
//   object's active lifecycle behaviour; BrnServerInterface overrides none of them.
// * DecFIGS DWARF twin (near-ancestor; gated on the X360 ledger, which names all
//   seven BrnNetwork::BrnServerInterfaceX360::* functions):
//   dwarfdump/GameSource/Network/BrnServerInterface.h:49 declares
//   "BrnServerInterface : public BrnServerInterfacePS3" with NO members of its own,
//   and dwarfdump/GameSource/Network/PS3/BrnServerInterfacePS3.h:44 declares the
//   platform intermediate carrying "CgsNetwork::ServerInterfaceGamesPS3 mGames;
//   EPrepareStage mePrepareStage; EReleaseStage meReleaseStage;" plus exactly those
//   seven virtuals.
//
// The scalar deleting destructor @ 0x827E4090 belongs to this class; its walk is
// the Base teardown with ONE extra component-vtable restore prepended at +0xC2C:
//
//   result[779] = &off_820CDBF8;   // 0xC2C  BrnServerInterfaceX360::mGames
//   result[746] = &off_820CDBF8;   // 0xBA8 ] these eleven restores are
//   result[738] = &off_820CDBF8;   // 0xB88 ]   IDENTICAL to the Base dtor
//   ... (the eight further Base restores) ...
//   result[ 48] = &off_820CDBF8;   // 0x0C0 ]
//   *result     =  off_820CDBD0;   // the (shared) Base primary vtable slot
//   if ( flag & 1 ) operator delete(result);
//
// i.e. the derived teardown is exactly the Base teardown (@ 0x827E1710) with one
// extra component-vtable restore prepended at +0xC2C -- the by-value destructor of
// the intermediate's mGames, which (being the last-declared member of the last base)
// is torn down first and so reinstalls its shared
// CgsNetwork::ServerInterfaceComponent base vtable (off_820CDBF8) before the
// inlined Base destructor runs. The compiler synthesises that whole walk + the
// conditional free from the trivial out-of-line virtual destructor; only the
// (empty) body is hand-written, matching the established deleting-destructor
// convention (see BrnServerInterfaceBase.cpp and
// BrnServerInterfaceDownloadableConfig.cpp). Defining the destructor out-of-line
// here anchors this class's deleting-destructor thunk to this TU.
//
// CORRECTION (wave L, 2026-08-04) -- THE NOTE THAT STOOD HERE WAS WRONG. It
// modelled the +0xC2C restore as an unnamed `mExtraComponent` of type
// CgsNetwork::ServerInterfaceComponent declared on THIS class, FLAGGED "the
// concrete leaf type is not named by any available dossier or DWARF". It is named:
// the member is BrnServerInterfaceX360::mGames, a by-value
// CgsNetwork::ServerInterfaceGamesX360, whose leaf vtable off_820CFBF8 the manager
// ctor installs at exactly +0xC2C (0x827E557C) -- all 18 of that vtable's slots are
// CgsNetwork::ServerInterfaceGamesX360 entries (Construct @+0x00 ..
// ReceivedGameEvent @+0x44). The stand-in member is therefore deleted here:
// carrying it as well would double-count the component and displace the
// intermediate's mePrepareStage / meReleaseStage /
// miNetworkServerInterfaceX360PM1 / PM2 (console +0x14BC..+0x14CB), which are what
// close sizeof at 0x14CC.
//
// The X360 +0xC2C / +0xBA8.. member offsets are 32-bit-pointer layout facts, quoted
// only to identify which member each access touches; they are NOT reproduced or
// static_asserted on a 64-bit host (vptr + pointers widen there, so the embedded
// slots land at different byte offsets while the by-name member walk the compiler
// emits is identical).
//
// The component accessors below are inline on the console (every caller reads the
// component slot straight out of the facade's component table); the status / error
// queries callers make on this class are the inherited ServerInterfaceDirtySock ones.
// Suspend/Resume are inherited virtuals -- overridden one level up, by
// BrnServerInterfaceX360.
// ===========================================================================

namespace BrnNetwork
{
    class BrnServerInterface : public BrnServerInterfaceX360
    {
    public:
        BrnServerInterface();

        // Scalar deleting destructor (bodied in BrnServerInterface.cpp).
        virtual ~BrnServerInterface();

        // Typed views of the facade's component table (slot = CgsNetwork::EComponents).
        // Inline on the console: each caller loads the slot's component pointer directly.
        CgsNetwork::ServerInterfaceConnection* GetConnectionComponent()
        {
            return reinterpret_cast<CgsNetwork::ServerInterfaceConnection*>(
                CgsNetwork::ServerInterfaceDirtySock::GetConnectionComponent() );
        }
        CgsNetwork::ServerInterfaceGames* GetGameComponent()
        {
            return reinterpret_cast<CgsNetwork::ServerInterfaceGames*>(
                CgsNetwork::ServerInterfaceDirtySock::GetGameComponent() );
        }
        CgsNetwork::ServerInterfacePlayerInfo* GetPlayerInfoComponent()
        {
            return reinterpret_cast<CgsNetwork::ServerInterfacePlayerInfo*>(
                GetComponent( CgsNetwork::E_COMPONENTS_PLAYER_INFO ) );
        }
        CgsNetwork::ServerInterfaceHttp* GetHttpComponent()
        {
            return reinterpret_cast<CgsNetwork::ServerInterfaceHttp*>(
                GetComponent( CgsNetwork::E_COMPONENTS_HTTP ) );
        }
        CgsNetwork::ServerInterfaceServerInfo* GetServerInfoComponent()
        {
            return reinterpret_cast<CgsNetwork::ServerInterfaceServerInfo*>(
                GetComponent( CgsNetwork::E_COMPONENTS_SERVERINFO ) );
        }
        CgsNetwork::ServerInterfacePingRegions* GetPingRegionsComponent()
        {
            return reinterpret_cast<CgsNetwork::ServerInterfacePingRegions*>(
                CgsNetwork::ServerInterfaceDirtySock::GetPingRegionsComponent() );
        }

        // NO DATA MEMBERS: this class adds none. The +0xC2C component is
        // BrnServerInterfaceX360::mGames (see the correction in the header note), and
        // the DWARF twin declares BrnServerInterface with a ctor/dtor only.
    };
}

#endif // BRN_SERVER_INTERFACE_H
