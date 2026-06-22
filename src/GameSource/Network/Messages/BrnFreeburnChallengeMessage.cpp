#include "types.hpp"

#include "GameSource/Network/Messages/BrnFreeburnChallengeMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::FreeburnChallengeMessage::Construct            @ 0x8257C508
//   BrnNetwork::FreeburnChallengeMessage::GetName             @ 0x8257C650
//   BrnNetwork::FreeburnChallengeMessage::GetPackedMessageSize @ 0x8257C560
//   BrnNetwork::FreeburnChallengeMessage::PackOrUnpack         @ 0x8257C580
//   BrnNetwork::FreeburnChallengeMessage::Retrieve             @ 0x8257F920
//
// The enum fields are constructed with a "none" sentinel: meChallengeStatus uses its
// COUNT (7); meEventType is stored as 7, which is one past this enum's COUNT (6) but is
// the value the X360 build writes (it is also the top of the [0,7] wire range the field
// quantises to), so it is reproduced literally as the invalid sentinel.
//
// PackOrUnpack routes each enum through the shared 32-bit int primitive via a scratch
// int (the quantiser works on s32), OR-ing every field's status together exactly as the
// X360 build does.

namespace BrnNetwork
{
    // Value the X360 ctor writes into meEventType as the "no event" sentinel (one past
    // EChallengeEventType's COUNT; top of the field's [0,7] wire range).
    static const BrnNetworkModuleIO::EChallengeEventType KE_CHALLENGE_EVENT_NONE =
        static_cast<BrnNetworkModuleIO::EChallengeEventType>(7);

    void FreeburnChallengeMessage::Construct()
    {
        // Inlined MessageWithPlayerIDs base init (the two player ids -> -1).
        mSendingPlayerID = KI_INVALID_PLAYER_ID;
        mRecvingPlayerID = KI_INVALID_PLAYER_ID;
        CgsNetwork::Message::Construct();
        mChallengeID      = 0;
        meEventType       = KE_CHALLENGE_EVENT_NONE;
        meChallengeStatus = BrnGameState::E_CHALLENGE_STATUS_COUNT;
        miActionIndex     = -1;
    }

    const char* FreeburnChallengeMessage::GetName() const
    {
        return "Freeburn Challenge Message";
    }

    s32 FreeburnChallengeMessage::GetPackedMessageSize()
    {
        mChallengeID      = 0;
        meEventType       = KE_CHALLENGE_EVENT_NONE;
        meChallengeStatus = BrnGameState::E_CHALLENGE_STATUS_COUNT;
        miActionIndex     = 0;
        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    CgsNetwork::PackOrUnpackResult FreeburnChallengeMessage::PackOrUnpack()
    {
        const CgsNetwork::PackOrUnpackResult lxBase = CgsNetwork::ReliableMessage::PackOrUnpack();
        const CgsNetwork::PackOrUnpackResult lxId =
            CgsNetwork::PackOrUnpackCgsID(this, &mChallengeID) | lxBase;
        const CgsNetwork::PackOrUnpackResult lxAction =
            CgsNetwork::PackOrUnpackInt(this, &miActionIndex, 0, 2) | lxId;

        s32 liEventType = meEventType;
        const CgsNetwork::PackOrUnpackResult lxEvent =
            CgsNetwork::PackOrUnpackInt(this, &liEventType, 0, 7) | lxAction;
        meEventType = static_cast<BrnNetworkModuleIO::EChallengeEventType>(liEventType);

        s32 liStatus = meChallengeStatus;
        const CgsNetwork::PackOrUnpackResult lxResult =
            CgsNetwork::PackOrUnpackInt(this, &liStatus, 0, 7) | lxEvent;
        meChallengeStatus = static_cast<BrnGameState::EChallengeStatus>(liStatus);
        return lxResult;
    }

    bool FreeburnChallengeMessage::Retrieve(CgsID* lpChallengeID,
                                            BrnNetworkModuleIO::EChallengeEventType* lpeEventType,
                                            BrnGameState::EChallengeStatus* lpeChallengeStatus,
                                            s32* lpiActionIndex)
    {
        CGS_ASSERT(lpChallengeID, "lpChallengeID");
        CGS_ASSERT(lpeEventType, "lpeEventType");
        CGS_ASSERT(lpeChallengeStatus, "lpeChallengeStatus");

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
            return false;

        *lpChallengeID       = mChallengeID;
        *lpeEventType        = meEventType;
        *lpeChallengeStatus  = meChallengeStatus;
        *lpiActionIndex      = miActionIndex;
        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
        return true;
    }
}
