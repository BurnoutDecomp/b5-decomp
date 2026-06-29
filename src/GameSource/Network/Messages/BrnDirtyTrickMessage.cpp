#include "types.hpp"

#include "GameSource/Network/Messages/BrnDirtyTrickMessage.h"
#include "GameSource/Network/BrnNetworkManager.h"        // BrnNetworkManager::PackOrUnpack (player-id field helper)
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::DirtyTrickMessage::Construct            @ 0x8257AB68
//   BrnNetwork::DirtyTrickMessage::PrepareForSend       @ 0x8257ABC0
//   BrnNetwork::DirtyTrickMessage::GetPackedMessageSize @ 0x8257AC58
//   BrnNetwork::DirtyTrickMessage::PackOrUnpack         @ 0x8257AC78
//   BrnNetwork::DirtyTrickMessage::Retrieve             @ 0x8257E4E8
//   (GetName is bodied inline in the header @ 0x827DFD10; Destruct lives in its own TU,
//    like the sibling CameraRequestMessage/MarkedManMessage release paths.)
//
// A RELIABLE message announcing a dirty-trick event: the aggressor and victim network
// player ids (two s32 fields after the 0x28-byte ReliableMessage base) plus a one-byte
// trick type and a one-byte trick status:
//   +0x28  mAggressorNetworkPlayerID  (NetworkPlayerID == s32)
//   +0x2C  mVictimNetworkPlayerID     (NetworkPlayerID == s32)
//   +0x30  mu8DirtyTrickType          (u8)
//   +0x31  mu8DirtyTrickStatus        (u8)
//
// Construct seeds the two inherited player-id fields to -1, chains to the Message base,
// then seeds the four payload fields (the two leaf player ids to -1, the type/status to
// their max sentinels 3/5). PrepareForSend re-arms the slot only when it is not already
// VALID: it stores all four payload fields, stamps the reliable send (type id 0x12 == 18),
// then asserts IsReliable(). Retrieve asserts the four out-pointers are non-null, copies
// the fields out only when the VALID flag is set (clearing it), and returns whether
// anything was retrieved. PackOrUnpack ORs the four field statuses with the base reliable
// status (0 == success).
//
// The X360 GetPackedMessageSize tail-calls a COMDAT-folded copy of the reliable base size
// (the binary resolved the shared stub to TestConnectionMessage::GetPackedMessageSize);
// semantically it is CgsNetwork::ReliableMessage::GetPackedMessageSize().

namespace BrnNetwork
{
    // DecFIGS DWARF (BrnDirtyTrickMessage.cpp:27-30). The min/max bounds the two u8 fields
    // construct to / quantise against. KU8_MAX_DIRTY_TRICK_TYPE (3) and
    // KU8_MAX_DIRTY_TRICK_STATUS (5) are the Construct-time "max" sentinels stored at
    // +0x30 / +0x31. The PackOrUnpack WIRE range max for the type field is 2 (asm
    // `li r6, 2`), i.e. one below the type sentinel -- the four payback trick types
    // [0..2] that travel on the wire -- whereas the status field quantises over the full
    // [0, 5] range (asm `li r6, 5`).
    const u8 KU8_MIN_DIRTY_TRICK_TYPE   = 0;
    const u8 KU8_MAX_DIRTY_TRICK_TYPE   = 3;
    const u8 KU8_MIN_DIRTY_TRICK_STATUS = 0;
    const u8 KU8_MAX_DIRTY_TRICK_STATUS = 5;

    // The wire-quantise upper bound for the trick-type field (asm `li r6, 2`), distinct
    // from the Construct-time max sentinel above.
    static const s32 KI_DIRTY_TRICK_TYPE_WIRE_MAX = 2;

    // EMessageType id stamped onto the reliable send (asm `li r4, 0x12`).
    static const s32 KI_DIRTY_TRICK_MESSAGE_TYPE = 18;

    void DirtyTrickMessage::Construct()
    {
        // Inlined MessageWithPlayerIDs base init (the two player ids -> -1, stored before
        // the base Construct in the X360 body).
        mSendingPlayerID = KI_INVALID_PLAYER_ID;   // stw -1, +0x20
        mRecvingPlayerID = KI_INVALID_PLAYER_ID;   // stw -1, +0x24
        CgsNetwork::Message::Construct();
        mAggressorNetworkPlayerID = KI_INVALID_PLAYER_ID;     // stw -1, +0x28
        mVictimNetworkPlayerID    = KI_INVALID_PLAYER_ID;     // stw -1, +0x2C
        mu8DirtyTrickType         = KU8_MAX_DIRTY_TRICK_TYPE;  // stb  3, +0x30
        mu8DirtyTrickStatus       = KU8_MAX_DIRTY_TRICK_STATUS;// stb  5, +0x31
    }

    void DirtyTrickMessage::PrepareForSend(u16 lu16Frame, NetworkPlayerID lAggressorPlayerID,
                                           NetworkPlayerID lVictimPlayerID, u8 lu8DirtyTrickType,
                                           u8 lu8DirtyTrickStatus)
    {
        // Re-arm only an idle slot: the X360 body skips the whole store/send when the
        // inherited VALID flag is already set.
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
            return;

        mAggressorNetworkPlayerID = lAggressorPlayerID;    // stw, +0x28
        mVictimNetworkPlayerID    = lVictimPlayerID;        // stw, +0x2C
        mu8DirtyTrickType         = lu8DirtyTrickType;      // stb, +0x30
        mu8DirtyTrickStatus       = lu8DirtyTrickStatus;    // stb, +0x31
        CgsNetwork::ReliableMessage::PrepareForSend(KI_DIRTY_TRICK_MESSAGE_TYPE, lu16Frame);
        CGS_ASSERT(IsReliable(), "IsReliable()");
    }

    s32 DirtyTrickMessage::GetPackedMessageSize()
    {
        // The X360 build seeds the worst-case payload before reporting the base reliable
        // size: player ids 0, type 0, status 5 (asm stores 0/0/0/5 at +0x28..+0x31).
        mAggressorNetworkPlayerID = 0;                          // stw 0, +0x28
        mVictimNetworkPlayerID    = 0;                          // stw 0, +0x2C
        mu8DirtyTrickType         = KU8_MIN_DIRTY_TRICK_TYPE;   // stb 0, +0x30
        mu8DirtyTrickStatus       = KU8_MAX_DIRTY_TRICK_STATUS; // stb 5, +0x31
        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    CgsNetwork::PackOrUnpackResult DirtyTrickMessage::PackOrUnpack()
    {
        // Base reliable (de)serialise (the wrapped 16-bit reliable id), then each payload
        // field. Every per-field status is OR-ed into the running result (0 == success).
        const CgsNetwork::PackOrUnpackResult lxBase = CgsNetwork::ReliableMessage::PackOrUnpack();

        const CgsNetwork::PackOrUnpackResult lxAggressor =
            BrnNetworkManager::PackOrUnpack(this, &mAggressorNetworkPlayerID) | lxBase;
        const CgsNetwork::PackOrUnpackResult lxVictim =
            BrnNetworkManager::PackOrUnpack(this, &mVictimNetworkPlayerID) | lxAggressor;
        const CgsNetwork::PackOrUnpackResult lxType =
            CgsNetwork::PackOrUnpackU8(this, &mu8DirtyTrickType,
                                       KU8_MIN_DIRTY_TRICK_TYPE, KI_DIRTY_TRICK_TYPE_WIRE_MAX) | lxVictim;
        return CgsNetwork::PackOrUnpackU8(this, &mu8DirtyTrickStatus,
                                          KU8_MIN_DIRTY_TRICK_STATUS, KU8_MAX_DIRTY_TRICK_STATUS) | lxType;
    }

    bool DirtyTrickMessage::Retrieve(NetworkPlayerID* lpAggressorPlayerID, NetworkPlayerID* lpVictimPlayerID,
                                     u8* lpu8DirtyTrickType, u8* lpu8DirtyTrickStatus)
    {
        CGS_ASSERT(lpAggressorPlayerID != 0, "lpAggressorNetworkPlayerID");
        CGS_ASSERT(lpVictimPlayerID    != 0, "lpVictimNetworkPlayerID");
        CGS_ASSERT(lpu8DirtyTrickType  != 0, "lpu8DirtyTrickType");
        CGS_ASSERT(lpu8DirtyTrickStatus != 0, "lpu8DirtyTrickStatus");

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
            return false;

        *lpAggressorPlayerID  = mAggressorNetworkPlayerID;
        *lpVictimPlayerID     = mVictimNetworkPlayerID;
        *lpu8DirtyTrickType   = mu8DirtyTrickType;
        *lpu8DirtyTrickStatus = mu8DirtyTrickStatus;
        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
        CGS_ASSERT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0,
                   "!CgsNetwork::ReliableMessage::IsMessageValid()");
        return true;
    }
} // namespace BrnNetwork
