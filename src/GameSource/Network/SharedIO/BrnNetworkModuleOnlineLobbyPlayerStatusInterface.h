#pragma once

// ===================================================================================
// BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData (+ OnlineLobbyPlayerStatusInterface)
//   -- owning header (DWARF home BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h)
//   b5-decomp/src/GameSource/Network/SharedIO/BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h
//
// One player's LOBBY status record (car/wheel selection, ready state, team, the
// game/voip connection classifications and the selection flags) plus the 8-slot
// interface that owns the array. Created for the OnlineGameRoomPlayerInfo keystone
// (wave H): the game-room screen walks these records both in the
// GuiEventNetworkLobbyPlayerList payload (event 244: LobbyPlayerStatusData[8] then the
// player count) and in the GuiCache mirror (cache +0xB640).
//
// CLASS SHAPE + MEMBER ORDER: DecFIGS DWARF
//   references/DecFIGS/dwarfdump/GameSource/Network/SharedIO/
//     BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h  (:47-:76, :109-:127)
// MEMBER PLACEMENT: X360 ARTIST asm -- the game-room screen's row accesses pin the
// 56-byte stride and the fields it reads (OGRPI HandlePlayerLobbyListEvent /
// ShowPlayerList / HighlightNewPlayer / HandleControllerInputMainSubState):
//   +0  mSelectedCarID   +8  mSelectedWheelID   +16 mPlayerID   +20 meReadyStatus
//   +24 mePlayerTeam     +28 meGameConnectionType   +32 meVoipConnectionType
//   +36 miPlayerColourIndex  +40 muCarColourIndex   +44 muCarPaintFinishIndex
//   +48 mbLocalPlayer    +49 mbIsHost   +50 mbFinalSelection   +51 mbIsCriterion
//   (pad to the 56-byte stride)
//
// The three enum-typed rows whose enum homes are not yet committed shared headers
// (EPlayerTeam / the PS3 ConnectionData connection type) are modelled as their
// underlying s32, per the boundary-header convention (same as BrnGuiCache.h's
// GetCurrentGameModeType note).
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID

#include <cstddef>   // offsetof (stride pin in _AssertLayout)

namespace BrnNetwork
{
    namespace BrnNetworkModuleIO
    {
        struct LobbyPlayerStatusData
        {
            // DWARF :50 -- the ready column the game-room ready icon indexes.
            enum EReadyStatus
            {
                E_READY_STATUS_HOST      = 0,
                E_READY_STATUS_READY     = 1,
                E_READY_STATUS_NOT_READY = 2,
                E_READY_STATUS_PLAYING   = 3,
                E_READY_STATUS_COUNT     = 4,
            };

            CgsID        mSelectedCarID;          // :60  X360 row+0
            CgsID        mSelectedWheelID;        // :61  X360 row+8
            s32          mPlayerID;               // :62  X360 row+16 (GuiEventNetworkLaunching::NetworkPlayerID, typedef s32; -1 == empty slot)
            EReadyStatus meReadyStatus;           // :63  X360 row+20
            s32          mePlayerTeam;            // :64  X360 row+24 (GsmIO::EPlayerTeam; underlying s32)
            s32          meGameConnectionType;    // :65  X360 row+28 (CgsNetwork ConnectionData EConnectionType; underlying s32)
            s32          meVoipConnectionType;    // :66  X360 row+32 (as above)
            s32          miPlayerColourIndex;     // :67  X360 row+36
            u32          muCarColourIndex;        // :68  X360 row+40
            u32          muCarPaintFinishIndex;   // :69  X360 row+44
            bool         mbLocalPlayer;           // :70  X360 row+48
            bool         mbIsHost;                // :71  X360 row+49
            bool         mbFinalSelection;        // :72  X360 row+50
            bool         mbIsCriterion;           // :73  X360 row+51
            // Natural alignment ends the record at +52; the X360 array stride is 56
            // (the game-room screen advances rows by 56, and the event payload puts the
            // count at 8*56 == +448). The trailing reserved pad pins that stride.
            u8           maReservedPadTo56[4];    // X360 row+52..55

            void Clear();   // DWARF :76 (body is the network module's own TU)
        };

        // DWARF :109 -- the 8-slot owner of the lobby records.
        struct OnlineLobbyPlayerStatusInterface
        {
            LobbyPlayerStatusData*       GetPlayerLobbyData(s32 liIndex);        // :115
            const LobbyPlayerStatusData* GetPlayerLobbyData(s32 liIndex) const;  // :120
            void Clear();                                                        // :124

        private:
            LobbyPlayerStatusData maPlayerData[8];   // :127

            // Never called -- complete-class context; pins the 56-byte row stride the
            // X360 walks (both scalar runs are pointer-free, so the host stride equals
            // the console stride exactly).
            static void _AssertLayout()
            {
                static_assert(sizeof(LobbyPlayerStatusData) == 56, "X360 row stride");
                static_assert(offsetof(LobbyPlayerStatusData, mPlayerID)     == 16, "mPlayerID @row+16");
                static_assert(offsetof(LobbyPlayerStatusData, meReadyStatus) == 20, "meReadyStatus @row+20");
                static_assert(offsetof(LobbyPlayerStatusData, mbLocalPlayer) == 48, "mbLocalPlayer @row+48");
            }
        };
    }
}
