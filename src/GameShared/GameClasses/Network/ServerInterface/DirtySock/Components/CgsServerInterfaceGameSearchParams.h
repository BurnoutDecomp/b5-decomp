#ifndef CGS_SERVER_INTERFACE_GAME_SEARCH_PARAMS_H
#define CGS_SERVER_INTERFACE_GAME_SEARCH_PARAMS_H

#include "types.hpp"
#include "../CgsServerInterfaceStructureInterface.h"

// ===========================================================================
// CgsNetwork::ServerInterfaceGameSearchParamsBase
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceGameSearchParams.{h,cpp}
//
// Parameter block describing a "search for games" request. Derives from
// ServerInterfaceStructureInterface (vptr-only polymorphic base) and serialises
// itself into a tagfield record (START/COUNT/ROOM/ASYNC/SYS*/PLAYERS/CUSTOM*).
//
// LAYOUT (dwarfdump + X360 asm @ 0x82878898 SerialiseToString):
//   +0x00  vptr
//   +0x04  miNumGames        (s32)
//   +0x08  miRoomID          (s32)   (-1 == none)
//   +0x0C  muGameFlagsMask   (u32)
//   +0x10  muGameFlagsValue  (u32)
//   +0x14  mbReturnPlayers   (bool)
//
// VTABLE order after the StructureInterface six entries:
//   [+0x18] Prepare, [+0x1C] GetCustomFlagsMask, [+0x20] GetCustomFlagsValue.
// The asm calls them at exactly those vtable offsets, so they follow the base's
// GetData()-const slot (+0x14).
//
// On X360 the concrete type is ServerInterfaceGameSearchParamsX360 (the platform
// leaf `ServerInterfaceGameSearchParams` resolves to). Its Prepare
// (X360 @ 0x82878B20) seeds the base fields (miNumGames=10, miRoomID=-1, both
// flag words 0, mbReturnPlayers=0) and zeroes an 80-byte X360-only payload at
// +0x18 plus a trailing word at +0x68 -- modelled on the leaf below as
// maX360Payload[80] + muX360Field_68 so every byte the asm writes is named.
// ===========================================================================

namespace CgsNetwork
{
    struct ServerInterfaceGameSearchParamsBase : public ServerInterfaceStructureInterface
    {
    public:
        ServerInterfaceGameSearchParamsBase();
        virtual ~ServerInterfaceGameSearchParamsBase();

        // its own TU
        virtual bool Prepare();

        // its own TU
        void SerialiseToString(char* lpcRecord, s32 liRecLen) const;

        // CgsServerInterfaceGameSearchParams.h:120
        void SetReturnPlayers(bool lbReturnPlayers) { mbReturnPlayers = lbReturnPlayers; }

        // CgsServerInterfaceGameSearchParams.h:126
        bool ReturnPlayers() const { return mbReturnPlayers; }

        // ADDITIVE GROW (flagged by the ServerInterfaceGamesX360 group): read accessor
        // for the game-flags value word (+0x10). The X360 games leaf SearchForGames
        // (X360 0x8288C460) reads it to derive the XNRANK matchmaking tagfield (bit 10).
        // Exposed by name so the read goes through the named member rather than a
        // raw-offset hack; layout unchanged.
        u32 GetGameFlagsValue() const { return muGameFlagsValue; }

    protected:
        // CgsServerInterfaceGameSearchParams.h:83
        virtual u32 GetCustomFlagsMask() const = 0;
        // CgsServerInterfaceGameSearchParams.h:88
        virtual u32 GetCustomFlagsValue() const = 0;

        // CgsServerInterfaceGameSearchParams.h:73
        s32 miNumGames;
        // CgsServerInterfaceGameSearchParams.h:74
        s32 miRoomID;
        // CgsServerInterfaceGameSearchParams.h:75
        u32 muGameFlagsMask;
        // CgsServerInterfaceGameSearchParams.h:76
        u32 muGameFlagsValue;
        // CgsServerInterfaceGameSearchParams.h:78
        bool mbReturnPlayers;   // +0x14
    };

    // X360 platform leaf. `ServerInterfaceGameSearchParams` aliases to this on the
    // X360 build. Its Prepare (X360 @ 0x82878B20) seeds the base fields and zeroes an
    // 80-byte X360-only payload at +0x18 plus a trailing word at +0x68 (modelled by
    // name below). The custom-flags getters (GetCustomFlagsMask/Value) are NOT in this
    // TU -- they remain pure virtual here (their concrete bodies live in a separate,
    // un-assigned custom-flags TU); this leaf is therefore still abstract w.r.t. those
    // slots and is exercised via a flagged-placeholder test subclass in the embed check.
    struct ServerInterfaceGameSearchParamsX360 : public ServerInterfaceGameSearchParamsBase
    {
    public:
        ServerInterfaceGameSearchParamsX360();

        // X360 @ 0x82878B20 -- seed base fields + zero the X360 payload, return true.
        virtual bool Prepare();

        // Alignment padding: the base ends at +0x15 (bool mbReturnPlayers at +0x14);
        // the X360 payload below is word-aligned at +0x18 (the asm's addi r3,r11,0x18).
        u8  maAlignPad_15[3];    // +0x15
        // The X360-only payload the leaf Prepare zeroes (this+0x18 .. this+0x67).
        u8  maX360Payload[80];   // +0x18  (memset(this+0x18, 0, 80))
        u32 muX360Field_68;      // +0x68  (single stw 0)
    };

    typedef ServerInterfaceGameSearchParamsX360 ServerInterfaceGameSearchParams;
}

#endif // CGS_SERVER_INTERFACE_GAME_SEARCH_PARAMS_H
