#include "types.hpp"

#include "GameSource/Network/Messages/BrnTeamSelectMessage.h"
#include "GameSource/Network/BrnNetworkManager.h"             // BrnNetwork::BrnNetworkManager::PackOrUnpack (NetworkPlayerID field primitive)
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT

#include <cstddef>                                            // offsetof (uncalled layout assert)
#include <cstring>                                            // memcpy / memset (the X360 64-byte payload moves)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::TeamSelectMessage::Construct             @ 0x8257D438
//   BrnNetwork::TeamSelectMessage::GetPackedMessageSize  @ 0x8257D498
//   BrnNetwork::TeamSelectMessage::PackOrUnpack          @ 0x8257D4F0
//   BrnNetwork::TeamSelectMessage::PrepareForSend        @ 0x82580070
//   BrnNetwork::TeamSelectMessage::Retrieve              @ 0x82580178
//   (GetName is bodied inline in the header @ 0x827DFD70.)
//
// A RELIABLE message broadcasting the final team-selection table. Payload after the
// 0x28-byte ReliableMessage base:
//   +0x28  PlayerTeamInfo maPlayerTeamInfo[8]  (64 bytes -> +0x68)
//   +0x68  s32  miNumPlayers
//   +0x6C  s32  miNumTeams
//   +0x70  bool mbAutobalance
//   +0x71  bool mbFinalSelection

namespace BrnNetwork
{
    // Quantiser bounds recovered from the asm immediates.
    //   miNumPlayers : sub_82881370(.., 1, 8)   -- li r5,1 ; li r6,8 @ 0x8257D55C/58
    //   miNumTeams   : sub_82881370(.., 1, 7)   -- li r5,1 ; li r6,7 @ 0x8257D538/34
    //   per-player team id : sub_82881370(.., 1, 8) -- li r5,1 ; li r6,8 @ 0x8257D5A8/A4
    static const s32 KI_MIN_NUM_PLAYERS = 1;
    static const s32 KI_MAX_NUM_PLAYERS = 8;
    static const s32 KI_MIN_NUM_TEAMS   = 1;
    static const s32 KI_MAX_NUM_TEAMS   = 7;
    static const s32 KI_MIN_TEAM_ID     = 1;
    static const s32 KI_MAX_TEAM_ID     = 8;

    // Wire type id stamped onto the reliable send (CgsNetwork::ReliableMessage::
    // PrepareForSend leType arg; li r4, 0x2B in the X360 PrepareForSend body @ 0x82580118).
    static const s32 KI_TEAM_SELECT_MESSAGE_TYPE = 43;

    // ---------------------------------------------------------------------------------
    // Construct @ 0x8257D438
    //   Seeds the two inherited player ids to -1 (the inlined MessageWithPlayerIDs init),
    //   chains to the Message base ctor, then zeroes the 64-byte player-team table and the
    //   four trailing scalars. (The X360 stores the two -1s, calls Message::Construct,
    //   XMemSets +0x28..+0x68, then stores 0 to +0x68/+0x6C and 0 bytes to +0x70/+0x71.)
    // ---------------------------------------------------------------------------------
    void TeamSelectMessage::Construct()
    {
        // Inlined MessageWithPlayerIDs base init (stw -1 @ +0x20 / +0x24).
        mSendingPlayerID = CgsNetwork::MessageWithPlayerIDs::KI_INVALID_PLAYER_ID;
        mRecvingPlayerID = CgsNetwork::MessageWithPlayerIDs::KI_INVALID_PLAYER_ID;

        CgsNetwork::Message::Construct();

        std::memset(maPlayerTeamInfo, 0, sizeof(maPlayerTeamInfo));   // XMemSet(+0x28, 0, 0x40)
        miNumPlayers     = 0;                                         // stw 0, +0x68
        miNumTeams       = 0;                                         // stw 0, +0x6C
        mbAutobalance    = false;                                     // stb 0, +0x70
        mbFinalSelection = false;                                     // stb 0, +0x71
    }

    // ---------------------------------------------------------------------------------
    // PrepareForSend @ 0x82580070
    //   Clears the VALID flag if it is set, asserts the table pointer is non-null and the
    //   player count fits the fixed payload (8 * liNumPlayers <= 0x40), copies the 64-byte
    //   table in, stores the counts and flags, stamps the reliable send (type 43 + frame)
    //   through the base, then asserts the message came out reliable.
    // ---------------------------------------------------------------------------------
    void TeamSelectMessage::PrepareForSend(u16 lu16Frame, const PlayerTeamInfo* lpaPlayerTeamInfo,
                                           s32 liNumPlayers, s32 liNumTeams,
                                           bool lbAutobalance, bool lbFinalSelection)
    {
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
            mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;

        CGS_ASSERT(lpaPlayerTeamInfo != 0, "lpaPlayerTeamInfo");
        CGS_ASSERT(static_cast<s32>(8 * liNumPlayers) <= static_cast<s32>(sizeof(maPlayerTeamInfo)),
                   "sizeof( maPlayerTeamInfo) >= liNumPlayers * sizeof( PlayerTeamInfo )");

        std::memcpy(maPlayerTeamInfo, lpaPlayerTeamInfo, sizeof(maPlayerTeamInfo));   // XMemCpy(+0x28, src, 0x40)
        miNumPlayers     = liNumPlayers;        // stw, +0x68
        miNumTeams       = liNumTeams;          // stw, +0x6C
        mbAutobalance    = lbAutobalance;       // stb, +0x70
        mbFinalSelection = lbFinalSelection;    // stb, +0x71

        CgsNetwork::ReliableMessage::PrepareForSend(KI_TEAM_SELECT_MESSAGE_TYPE, lu16Frame);
        CGS_ASSERT(IsReliable(), "IsReliable()");
    }

    // ---------------------------------------------------------------------------------
    // Retrieve @ 0x82580178
    //   Asserts all five out-pointers are non-null. If the slot is VALID, copies the table
    //   and scalars out, clears the VALID flag, and returns true; otherwise returns false
    //   without touching the outputs.
    // ---------------------------------------------------------------------------------
    bool TeamSelectMessage::Retrieve(PlayerTeamInfo* lpaPlayerTeamInfo, s32* lpiNumPlayers,
                                     s32* lpiNumTeams, bool* lpbAutobalance, bool* lpbFinalSelection)
    {
        CGS_ASSERT(lpaPlayerTeamInfo != 0, "lpaPlayerTeamInfo");
        CGS_ASSERT(lpiNumPlayers != 0, "lpiNumPlayers");
        CGS_ASSERT(lpiNumTeams != 0, "lpiNumTeams");
        CGS_ASSERT(lpbAutobalance != 0, "lpbAutobalance");
        CGS_ASSERT(lpbFinalSelection != 0, "lpbFinalSelection");

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
            return false;

        std::memcpy(lpaPlayerTeamInfo, maPlayerTeamInfo, sizeof(maPlayerTeamInfo));   // XMemCpy(dst, +0x28, 0x40)
        *lpiNumPlayers     = miNumPlayers;        // lwz +0x68
        *lpiNumTeams       = miNumTeams;          // lwz +0x6C
        *lpbAutobalance    = mbAutobalance;       // lbz +0x70
        *lpbFinalSelection = mbFinalSelection;    // lbz +0x71

        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
        CGS_ASSERT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0,
                   "!CgsNetwork::ReliableMessage::IsMessageValid()");
        return true;
    }

    // ---------------------------------------------------------------------------------
    // PackOrUnpack @ 0x8257D4F0
    //   (De)serialises the payload. Each field is routed through the shared field
    //   primitives whose per-field status is OR-ed into the running result (0 == all
    //   succeeded). Wire fields, in the X360 order:
    //     base reliable id            (ReliableMessage::PackOrUnpack)
    //     mbFinalSelection            PackOrUnpackBool  (+0x71)
    //     mbAutobalance               PackOrUnpackBool  (+0x70)
    //     miNumTeams                  PackOrUnpackInt   [1, 7]  (+0x6C)
    //     miNumPlayers                PackOrUnpackInt   [1, 8]  (+0x68)
    //     for each of miNumPlayers players:
    //       maPlayerTeamInfo[i].mPlayerID  BrnNetworkManager::PackOrUnpack
    //       maPlayerTeamInfo[i].miTeam     PackOrUnpackInt [1, 8]
    // ---------------------------------------------------------------------------------
    CgsNetwork::PackOrUnpackResult TeamSelectMessage::PackOrUnpack()
    {
        CgsNetwork::PackOrUnpackResult lxResult = CgsNetwork::ReliableMessage::PackOrUnpack();
        lxResult = CgsNetwork::PackOrUnpackBool(this, &mbFinalSelection) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackBool(this, &mbAutobalance) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackInt(this, &miNumTeams, KI_MIN_NUM_TEAMS, KI_MAX_NUM_TEAMS) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackInt(this, &miNumPlayers, KI_MIN_NUM_PLAYERS, KI_MAX_NUM_PLAYERS) | lxResult;

        for (s32 liPlayer = 0; liPlayer < miNumPlayers; ++liPlayer)
        {
            PlayerTeamInfo& lInfo = maPlayerTeamInfo[liPlayer];

            lxResult = BrnNetwork::BrnNetworkManager::PackOrUnpack(this, &lInfo.mPlayerID) | lxResult;

            // The X360 stages the team id through a stack temp (lwz +0x04 -> [sp], pack,
            // [sp] -> +0x04); de-staged here to the member by name.
            s32 liTeam = lInfo.miTeam;
            lxResult = CgsNetwork::PackOrUnpackInt(this, &liTeam, KI_MIN_TEAM_ID, KI_MAX_TEAM_ID) | lxResult;
            lInfo.miTeam = liTeam;
        }

        return lxResult;
    }

    // ---------------------------------------------------------------------------------
    // GetPackedMessageSize @ 0x8257D498
    //   Re-zeroes the 64-byte table and the four trailing scalars (so the size query
    //   measures a clean worst case), then tail-calls the (COMDAT-folded) base
    //   TestConnectionMessage::GetPackedMessageSize size query.
    // ---------------------------------------------------------------------------------
    s32 TeamSelectMessage::GetPackedMessageSize()
    {
        std::memset(maPlayerTeamInfo, 0, sizeof(maPlayerTeamInfo));   // XMemSet(+0x28, 0, 0x40)
        miNumPlayers     = 0;                                         // stw 0, +0x68
        miNumTeams       = 0;                                         // stw 0, +0x6C
        mbAutobalance    = false;                                     // stb 0, +0x70
        mbFinalSelection = false;                                     // stb 0, +0x71

        // The X360 tail-calls CgsNetwork::TestConnectionMessage::GetPackedMessageSize (the
        // COMDAT-folded reliable-message size query). The folded body reads only base fields,
        // so the equivalent committed base entry-point ReliableMessage::GetPackedMessageSize()
        // is the same code; calling it here on `this` matches the asm's intent.
        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    // Uncalled layout pin. The committed CgsNetwork::Message base is reconstructed 8 bytes
    // larger than the X360 original (its bitstream view rounds up), so the ABSOLUTE X360 byte
    // offsets (+0x28 table .. +0x71 flags) do NOT survive onto the LLP64 gate host; only the
    // pointer-INVARIANT intra-payload facts the asm relies on are pinned here -- the 8-byte
    // PlayerTeamInfo stride (the asm advances the loop cursor by 8 per entry: `addi r30,r30,8`)
    // and the field run order within the trailing scalars.
    void TeamSelectMessage_AssertLayout()
    {
        static_assert(sizeof(TeamSelectMessage::PlayerTeamInfo) == 8,
                      "PlayerTeamInfo is an 8-byte {NetworkPlayerID, s32 team} pair");
        static_assert(offsetof(TeamSelectMessage::PlayerTeamInfo, mPlayerID) == 0,
                      "PlayerTeamInfo.mPlayerID @ +0x00 (BrnNetworkManager::PackOrUnpack target)");
        static_assert(offsetof(TeamSelectMessage::PlayerTeamInfo, miTeam) == 4,
                      "PlayerTeamInfo.miTeam @ +0x04 (quantised int [1,8])");
        static_assert(sizeof(TeamSelectMessage::maPlayerTeamInfo) == 0x40,
                      "the player-team table is the 64-byte payload XMemSet/XMemCpy'd");
        // The four trailing scalars run miNumPlayers, miNumTeams, mbAutobalance, mbFinalSelection
        // in that storage order (the asm stores +0x68/+0x6C/+0x70/+0x71 in that sequence).
        static_assert(offsetof(TeamSelectMessage, miNumTeams) == offsetof(TeamSelectMessage, miNumPlayers) + 4,
                      "miNumTeams immediately follows miNumPlayers");
        static_assert(offsetof(TeamSelectMessage, mbAutobalance) == offsetof(TeamSelectMessage, miNumTeams) + 4,
                      "mbAutobalance immediately follows miNumTeams");
        static_assert(offsetof(TeamSelectMessage, mbFinalSelection) == offsetof(TeamSelectMessage, mbAutobalance) + 1,
                      "mbFinalSelection immediately follows mbAutobalance");
    }
} // namespace BrnNetwork
