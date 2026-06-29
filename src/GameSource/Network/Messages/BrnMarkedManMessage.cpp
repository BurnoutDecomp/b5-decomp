#include "types.hpp"

#include "GameSource/Network/Messages/BrnMarkedManMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::MarkedManMessage::Construct            @ 0x8257B558
//   BrnNetwork::MarkedManMessage::PackOrUnpack         @ 0x8257B5A8
//   BrnNetwork::MarkedManMessage::GetPackedMessageSize @ 0x8257B620
//   BrnNetwork::MarkedManMessage::PrepareForSend       @ 0x8257E900
//   BrnNetwork::MarkedManMessage::Retrieve             @ 0x8257E930
//   (GetName is bodied inline in the header @ 0x827DFD40; Destruct lives in its
//    own TU, like the sibling HullSyncMessage::Release.)
//
// Reliable "marked man" notification: who the current marked man is (a NetworkPlayerID
// at +0x28) plus a final-answer flag (bool at +0x2C), serialised after the reliable
// base's reliable id. The player-id sentinel is -1 (KI_INVALID_PLAYER_ID), so the int
// field (de)serialises over [-1, 0x7FFFFFFF].
//
// PrepareForSend re-arms the slot: it clears the inherited VALID flag if it was set,
// stores both fields unconditionally, then stamps the reliable message with type 25
// (0x19). Retrieve asserts both out-pointers are non-null, copies the fields out only
// when the VALID flag is set (clearing it), and returns whether anything was retrieved.
// PackOrUnpack ORs the two field statuses with the base reliable status (0 == success).
//
// The X360 GetPackedMessageSize tail-calls a COMDAT-folded copy of the reliable base
// size (the binary resolved the shared stub to TestConnectionMessage::GetPackedMessageSize);
// semantically it is CgsNetwork::ReliableMessage::GetPackedMessageSize().

namespace BrnNetwork
{
    // EMessageType id for the marked-man message (asm `li r4, 0x19`).
    static const s32 KI_MARKED_MAN_MESSAGE_TYPE = 25;

    void MarkedManMessage::Construct()
    {
        // Inlined MessageWithPlayerIDs base init (the two player ids -> -1).
        mSendingPlayerID = KI_INVALID_PLAYER_ID;
        mRecvingPlayerID = KI_INVALID_PLAYER_ID;
        CgsNetwork::Message::Construct();
        mMarkedMan     = KI_INVALID_PLAYER_ID;   // stw -1, +0x28
        mbFinalAnswer  = false;                  // stb  0, +0x2C
    }

    s32 MarkedManMessage::GetPackedMessageSize()
    {
        // The X360 build re-inits the fields before reporting the base reliable size.
        mMarkedMan    = KI_INVALID_PLAYER_ID;
        mbFinalAnswer = false;
        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    CgsNetwork::PackOrUnpackResult MarkedManMessage::PackOrUnpack()
    {
        const CgsNetwork::PackOrUnpackResult lxMarkedMan =
            CgsNetwork::PackOrUnpackInt(this, &mMarkedMan, -1, 0x7FFFFFFF);
        const CgsNetwork::PackOrUnpackResult lxFinalAnswer =
            CgsNetwork::PackOrUnpackBool(this, &mbFinalAnswer);
        return CgsNetwork::ReliableMessage::PackOrUnpack() | (lxFinalAnswer | lxMarkedMan);
    }

    void MarkedManMessage::PrepareForSend(u16 lu16Frame, NetworkPlayerID lMarkedMan,
                                          bool lbFinalAnswer)
    {
        // Re-arm: drop a previously-pending VALID flag before re-stamping the slot.
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
            mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;

        mMarkedMan    = lMarkedMan;     // stw, +0x28
        mbFinalAnswer = lbFinalAnswer;  // stb, +0x2C
        CgsNetwork::ReliableMessage::PrepareForSend(KI_MARKED_MAN_MESSAGE_TYPE, lu16Frame);
    }

    bool MarkedManMessage::Retrieve(NetworkPlayerID* lpMarkedMan, bool* lpbFinalAnswer)
    {
        CGS_ASSERT(lpMarkedMan != 0, "lpMarkedMan");
        CGS_ASSERT(lpbFinalAnswer != 0, "lpbIsFinalSelection");

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
            return false;

        *lpMarkedMan    = mMarkedMan;
        *lpbFinalAnswer = mbFinalAnswer;
        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
        CGS_ASSERT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0,
                   "!CgsNetwork::ReliableMessage::IsMessageValid()");
        return true;
    }
} // namespace BrnNetwork
