#pragma once

// ===================================================================================
// BrnNetwork::TeamSelectMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnTeamSelectMessage.h
//
// A CgsNetwork::ReliableMessage subclass that broadcasts the final team-selection
// table: per-player {network player id, team id} pairs plus the player/team counts
// and the autobalance / final-selection flags. There is no DecFIGS DWARF for this TU
// (dwarfdump none found), so the class shape is recovered directly from the X360
// asm (BURNOUT_X360_ARTIST.XEX) byte offsets, modelled exactly like the committed
// sibling BrnNetwork::SelectedRoutesMessage / BrnNetwork::CarSelectMessage. The base
// CgsNetwork::ReliableMessage is the committed home in CgsReliableMessage.h and is
// reused BY NAME here -- not forked.
//
// LAYOUT (recovered from the asm; ReliableMessage adds no data of its own so the first
// leaf member lands at +0x28):
//   +0x00  (CgsNetwork::ReliableMessage base, 0x28 bytes)
//   +0x28  PlayerTeamInfo maPlayerTeamInfo[8]   (8 x 8 bytes == 64 bytes -> +0x68)
//            PlayerTeamInfo { NetworkPlayerID mPlayerID (+0x00); s32 miTeam (+0x04) }
//   +0x68  s32  miNumPlayers      (packed [1, 8])
//   +0x6C  s32  miNumTeams        (packed [1, 7])
//   +0x70  bool mbAutobalance
//   +0x71  bool mbFinalSelection
//
//   Construct XMemSets +0x28..+0x68 (64 bytes) to 0 then zeroes the four trailing
//   scalars; PrepareForSend XMemCpys a caller-supplied 64-byte table in. The per-player
//   table is the worst-case 8 entries: 8 * sizeof(PlayerTeamInfo) == 0x40, the bound the
//   X360 asserts (slwi r11, numPlayers, 3 ; cmplwi r11, 0x40).
//
// LEDGER FUNCTION bodied in this (header) TU (X360 @ 0x827DFD70):
//   BrnNetwork::TeamSelectMessage::GetName
//     -> returns the literal "Team Select Message" (lis/addi a rodata string, blr).
//        No member or base access.
//
// The remaining declared methods (Construct/PrepareForSend/Retrieve/
// GetPackedMessageSize/PackOrUnpack) live in the sibling .cpp TU
// (BrnTeamSelectMessage.cpp) and are declared here for the class shape but NOT bodied
// in this TU.
// ===================================================================================

#include "types.hpp"                                                                  // s32, u16, bool
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"     // CgsNetwork::ReliableMessage (committed base)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                           // BrnNetwork::NetworkPlayerID (committed typedef)

namespace BrnNetwork
{
    // The fixed payload cap recovered from the X360 asserts: the 64-byte payload holds
    // at most 8 PlayerTeamInfo entries (8 * 8 == 0x40, the `slwi 3 ; cmplwi 0x40` bound).
    const s32 KI_TEAM_SELECT_MAX_PLAYERS = 8;

    // The TeamSelectMessage broadcasts which team each player landed on. Class shape is
    // recovered from the X360 asm (no DWARF); base reused by name from CgsReliableMessage.h.
    // Like the committed base hierarchy, the X360 build models the vtable as the explicit
    // Message::mpVTable member (no C++ `virtual`), so these are plain methods -- adding a C++
    // `virtual` here would inject a second vptr and shift every member off its recovered
    // byte offset (see the offsetof pins in the .cpp).
    struct TeamSelectMessage : public CgsNetwork::ReliableMessage
    {
        // One per-player team assignment in the broadcast table (8 bytes; +0x00 player id,
        // +0x04 team id). The player id field is (de)serialised by the shared
        // BrnNetworkManager::PackOrUnpack NetworkPlayerID primitive; the team id is a
        // quantised int in [1, 8].
        struct PlayerTeamInfo
        {
            NetworkPlayerID mPlayerID;   // +0x00
            s32             miTeam;      // +0x04
        };

    public:
        // Sibling-.cpp methods (declared for class shape; NOT bodied in this TU).
        void          Construct();
        void          PrepareForSend(u16 lu16Frame, const PlayerTeamInfo* lpaPlayerTeamInfo,
                                     s32 liNumPlayers, s32 liNumTeams,
                                     bool lbAutobalance, bool lbFinalSelection);
        bool          Retrieve(PlayerTeamInfo* lpaPlayerTeamInfo, s32* lpiNumPlayers,
                               s32* lpiNumTeams, bool* lpbAutobalance, bool* lpbFinalSelection);
        s32           GetPackedMessageSize();

        // LEDGER func @ 0x827DFD70 -- bodied inline below.
        const char* GetName() const;

    protected:
        CgsNetwork::PackOrUnpackResult PackOrUnpack();

    private:
        PlayerTeamInfo maPlayerTeamInfo[KI_TEAM_SELECT_MAX_PLAYERS];   // +0x28 (64 bytes)
        s32            miNumPlayers;                                   // +0x68
        s32            miNumTeams;                                     // +0x6C
        bool           mbAutobalance;                                  // +0x70
        bool           mbFinalSelection;                               // +0x71

        // Uncalled layout pin in BrnTeamSelectMessage.cpp needs offsetof on the private
        // members; befriend it so the static_asserts can name them.
        friend void TeamSelectMessage_AssertLayout();
    };

    // BrnNetwork::TeamSelectMessage::GetName  @ 0x827DFD70
    //   lis/addi a rodata string literal, blr -- no member or base access.
    inline const char* TeamSelectMessage::GetName() const
    {
        return "Team Select Message";
    }
} // namespace BrnNetwork
