#ifndef BRN_NETWORK_PLAYER_INFO_DATA_H
#define BRN_NETWORK_PLAYER_INFO_DATA_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfacePlayerInfoDataX360.h"

// ===========================================================================
// BrnNetwork::PlayerInfoData
//   Home: GameSource/Network/Parameters/BrnNetworkPlayerInfoData.{h,cpp}
//
// Game-side per-player info object. The recovered function is its
// `scalar deleting destructor' (X360 @ 0x8255EA38): store the class vptr
// (off_8207C88C) at this+0, then conditionally call operator delete -- the
// codegen of a virtual destructor.
//
// ADDITIVE GROW (BrnNetworkLoginManagerBase TU): the X360 login flow
// (LoginManagerBase::UpdateWaitingForPlayerID @ 0x82561A10) constructs a stack
// PlayerInfoData (256-byte record, vtable off_820821EC), calls Prepare() on it, hands
// it to ServerInterfacePlayerInfo::GetLocalPlayerInfo (whose parameter is a
// CgsNetwork::ServerInterfacePlayerInfoDataBase*), then reads its player id at +0xC0
// (GetID()) and proceeds once it is no longer -1. That pins PlayerInfoData as a
// derivative of the lobby player-info record (single vptr at +0x00, miID at +0xC0)
// plus its own Prepare() step. The base supplies the layout / GetID(); the empty
// virtual-dtor body is unchanged (the compiler synthesises the base-dtor chain).
// Prepare() is declared-only here; its body lands with this class's own (login
// player-info) TU.
//
// BASE REFINED Base -> X360 leaf (GamerPictureManagerX360 TU): the X360
// GamerPictureManagerX360::AddPlayer @ 0x825616B8 builds this same stack record
// (vtable off_820821EC, ctor stw @ 0x82561808), has GetLocalPlayerInfo fill it, and
// then reads its 8-byte XUID WHOLE at record +0xF8 (`ld r10, var_48(r1)` @
// 0x8256184C, record base sp+0x60 -> +0xF8) -- the exact muXUID slot the committed
// CgsNetwork::ServerInterfacePlayerInfoDataX360 leaf adds after the +0xF4 base tail
// (and the record's 256-byte size == that leaf's 0x100). So on the X360 build the
// game-side record derives from the X360 PLATFORM leaf, not the raw base. The
// DecFIGS DWARF agrees in shape: BrnNetwork::PlayerInfoData derives from the
// per-platform `CgsNetwork::ServerInterfacePlayerInfoData` wrapper declared at
// CgsServerInterface.h:85 (PS3 flavour there); resolving the platform selection
// straight to the X360 leaf mirrors the committed
// `ServerInterface : ServerInterfaceDirtySockX360` pattern (CgsServerInterface.h).
// Upcasts to ServerInterfacePlayerInfoDataBase* (GetLocalPlayerInfo's parameter)
// still hold through the leaf.
// ===========================================================================

namespace BrnNetwork
{
    class PlayerInfoData : public CgsNetwork::ServerInterfacePlayerInfoDataX360
    {
    public:
        virtual ~PlayerInfoData();

        // X360 BrnNetwork::PlayerInfoData::Prepare -- bring the record to its prepared (cleared)
        // state before GetLocalPlayerInfo fills it. Returns true on success (the X360 asserts the
        // result). Declared-only; the body lands with this class's own TU.
        bool Prepare();

        // The CgsNetwork::ServerInterfaceStructureInterface pure virtuals this concrete leaf
        // implements (its vtable off_820821EC overrides them). The X360 login flow stack-constructs
        // a PlayerInfoData, so the class must be concrete; the override bodies belong to this class's
        // own TU and are declared-only here. (SerialiseFromUser is overridden from the player-info
        // base, which already provides a body slot.)
        virtual const char* GetPattern() const;
        virtual s32         GetPatternLength() const;
        virtual u32         GetDataSize() const;
        virtual void*       GetData();
        virtual const void* GetData() const;
    };
}

#endif // BRN_NETWORK_PLAYER_INFO_DATA_H
