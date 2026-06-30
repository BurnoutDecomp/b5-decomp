#ifndef BRN_NETWORK_PLAYER_INFO_DATA_H
#define BRN_NETWORK_PLAYER_INFO_DATA_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfoData.h"

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
// derivative of CgsNetwork::ServerInterfacePlayerInfoDataBase (the lobby player-info
// record -- single vptr at +0x00, miID at +0xC0) plus its own Prepare() step. The base
// supplies the layout / GetID(); the empty virtual-dtor body is unchanged (the compiler
// synthesises the base-dtor chain). Prepare() is declared-only here; its body lands with
// this class's own (login player-info) TU.
// ===========================================================================

namespace BrnNetwork
{
    class PlayerInfoData : public CgsNetwork::ServerInterfacePlayerInfoDataBase
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
