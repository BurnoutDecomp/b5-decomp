#include "types.hpp"

#include "GameSource/Network/Messages/BrnActiveFburnChallengeMessage.h"
#include "GameSource/Network/BrnNetworkManager.h"        // BrnNetwork::BrnNetworkManager::PackOrUnpack (player-id field primitive)
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::ActiveFburnChallengeMessage::Construct             @ 0x8257CBD8
//   BrnNetwork::ActiveFburnChallengeMessage::GetPackedMessageSize  @ 0x8257CC30
//   BrnNetwork::ActiveFburnChallengeMessage::PackOrUnpack          @ 0x8257CC80
//   BrnNetwork::ActiveFburnChallengeMessage::GetName               @ 0x8257CD20
//   BrnNetwork::ActiveFburnChallengeMessage::PrepareForSend        @ 0x8257CD30
//   BrnNetwork::ActiveFburnChallengeMessage::Retrieve              @ 0x8257FCC0
//
// The message announces the roster of an active freeburn challenge: a fixed table of up
// to KI_MAX_NETWORK_PLAYERS network-player ids, the challenge id, and the count of
// players in it.
//
// Notes on the asm vs. the DWARF declaration:
//  * PrepareForSend's CgsID parameter is passed whole in one 64-bit GPR (r5) and stored
//    via a single `std` at +0x48; Hex-Rays mis-modelled it as a 32-bit register PAIR
//    (`__PAIR64__(a4, a3)`) and then mistook one half for the source array pointer. The
//    DWARF signature (uint16_t, CgsID, NetworkPlayerID*, int32_t) is authoritative and
//    matches the register order frame=r4, challengeID=r5, playerArray=r6, count=r7.
//  * GetPackedMessageSize chains through the symbol the linker folded the
//    ReliableMessage::GetPackedMessageSize body onto (ICF labelled it
//    TestConnectionMessage::GetPackedMessageSize -- identical body). The semantic base
//    call is ReliableMessage::GetPackedMessageSize(), matching the class hierarchy.

static const char KAC_ARTIST_ASSERT_FILE[] =
    "..\\..\\..\\GameSource\\Network/Messages/BrnActiveFburnChallengeMessage.cpp";

namespace BrnNetwork
{
    // EMessageType id for the active-freeburn-challenge message (asm `li r4, 0x26`).
    static const s32 KI_ACTIVE_FBURN_CHALLENGE_MESSAGE_TYPE = 38;

    void ActiveFburnChallengeMessage::Construct()
    {
        // Inlined MessageWithPlayerIDs base init (the two inherited player ids -> -1),
        // emitted before the Message::Construct chain.
        mSendingPlayerID = KI_INVALID_PLAYER_ID;
        mRecvingPlayerID = KI_INVALID_PLAYER_ID;
        CgsNetwork::Message::Construct();

        mChallengeID            = 0;
        miNumPlayersInChallenge = 0;
        for (s32 liPlayer = 0; liPlayer < KI_MAX_NETWORK_PLAYERS; ++liPlayer)
            maPlayersInChallengeIDs[liPlayer] = 0;
    }

    const char* ActiveFburnChallengeMessage::GetName() const
    {
        return "Active Freeburn Challenge Message";
    }

    s32 ActiveFburnChallengeMessage::GetPackedMessageSize()
    {
        // Reset the payload to its empty/default state before the base reports the size.
        mChallengeID            = 0;
        miNumPlayersInChallenge = 0;
        for (s32 liPlayer = 0; liPlayer < KI_MAX_NETWORK_PLAYERS; ++liPlayer)
            maPlayersInChallengeIDs[liPlayer] = 0;
        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    CgsNetwork::PackOrUnpackResult ActiveFburnChallengeMessage::PackOrUnpack()
    {
        CgsNetwork::PackOrUnpackResult lxResult = CgsNetwork::ReliableMessage::PackOrUnpack();
        lxResult |= CgsNetwork::Message::PackOrUnpack(&mChallengeID);
        lxResult |= CgsNetwork::Message::PackOrUnpack(&miNumPlayersInChallenge, 0, KI_MAX_NETWORK_PLAYERS);

        for (s32 liPlayer = 0; liPlayer < miNumPlayersInChallenge; ++liPlayer)
            lxResult |= BrnNetworkManager::PackOrUnpack(this,&maPlayersInChallengeIDs[liPlayer]);

        return lxResult;
    }

    void ActiveFburnChallengeMessage::PrepareForSend(u16 lu16FrameCount,
                                                     CgsID lChallengeID,
                                                     NetworkPlayerID* lpaPlayersInChallengeIDs,
                                                     s32 liNumPlayersInChallenge)
    {
        CGS_ASSERT(lu16FrameCount != CgsNetwork::KU16_INVALID_FRAME,
            CgsDev::Assert::MessageAt(
                "lu16FrameCount != KU16_INVALID_FRAME", KAC_ARTIST_ASSERT_FILE, 145 /* ARTIST 0x8257CD6C */));
        CGS_ASSERT(!IsMessageValid(),
            CgsDev::Assert::MessageAt(
                "!CgsNetwork::ReliableMessage::IsMessageValid()", KAC_ARTIST_ASSERT_FILE, 146 /* ARTIST 0x8257CD98 */));

        mChallengeID            = lChallengeID;
        miNumPlayersInChallenge = liNumPlayersInChallenge;

        CGS_ASSERT(liNumPlayersInChallenge > 0,
            CgsDev::Assert::MessageAt(
                "liNumPlayersInChallenge > 0", KAC_ARTIST_ASSERT_FILE, 151 /* ARTIST 0x8257CDC4 */));
        CGS_ASSERT(liNumPlayersInChallenge <= KI_MAX_NETWORK_PLAYERS,
            CgsDev::Assert::MessageAt(
                "liNumPlayersInChallenge <= KI_MAX_NETWORK_PLAYERS", KAC_ARTIST_ASSERT_FILE, 152 /* ARTIST 0x8257CDE8 */));

        for (s32 liPlayer = 0; liPlayer < miNumPlayersInChallenge; ++liPlayer)
            maPlayersInChallengeIDs[liPlayer] = lpaPlayersInChallengeIDs[liPlayer];

        CgsNetwork::ReliableMessage::PrepareForSend(KI_ACTIVE_FBURN_CHALLENGE_MESSAGE_TYPE, lu16FrameCount);
    }

    bool ActiveFburnChallengeMessage::Retrieve(CgsID* lpChallengeID,
                                               NetworkPlayerID* lpaNetworkPlayerIDs,
                                               s32* lpiNumPlayersInChallenge)
    {
        CGS_ASSERT(lpChallengeID, CgsDev::Assert::MessageAt(
            "lpChallengeID",
            "..\\..\\..\\GameSource\\Network/Messages/BrnActiveFburnChallengeMessage.cpp", 176 /* ARTIST 0x8257FD00 */));
        CGS_ASSERT(lpaNetworkPlayerIDs, CgsDev::Assert::MessageAt(
            "lpaNetworkPlayerIDs",
            "..\\..\\..\\GameSource\\Network/Messages/BrnActiveFburnChallengeMessage.cpp", 177 /* ARTIST 0x8257FD24 */));
        CGS_ASSERT(lpiNumPlayersInChallenge, CgsDev::Assert::MessageAt(
            "lpiNumPlayersInChallenge",
            "..\\..\\..\\GameSource\\Network/Messages/BrnActiveFburnChallengeMessage.cpp", 178 /* ARTIST 0x8257FD48 */));

        // ARTIST 0x8257FD5C branches to a trailing `return false` (0x8257FE10): the copy-out
        // is the body of the valid test.
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
        {
            *lpChallengeID            = mChallengeID;
            *lpiNumPlayersInChallenge = miNumPlayersInChallenge;

            CGS_ASSERT(miNumPlayersInChallenge > 0, CgsDev::Assert::MessageAt(
                "miNumPlayersInChallenge > 0",
                "..\\..\\..\\GameSource\\Network/Messages/BrnActiveFburnChallengeMessage.cpp", 187 /* ARTIST 0x8257FD90 */));
            CGS_ASSERT(miNumPlayersInChallenge <= KI_MAX_NETWORK_PLAYERS, CgsDev::Assert::MessageAt(
                "miNumPlayersInChallenge <= KI_MAX_NETWORK_PLAYERS",
                "..\\..\\..\\GameSource\\Network/Messages/BrnActiveFburnChallengeMessage.cpp", 188 /* ARTIST 0x8257FDB8 */));

            for (s32 liPlayer = 0; liPlayer < miNumPlayersInChallenge; ++liPlayer)
                lpaNetworkPlayerIDs[liPlayer] = maPlayersInChallengeIDs[liPlayer];

            mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
            return true;
        }
        return false;
    }
}
