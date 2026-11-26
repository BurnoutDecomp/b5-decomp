#include "GameShared/GameClasses/Network/Time/CgsSyncTimeMessage.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameShared\\GameClasses\\Network\\Time\\CgsSyncTimeMessage.cpp"
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (CgsNetwork::SyncTimeMessage)
//
//   GetName @ 0x827DBC68:  return "Sync Time Message";
//
// GetName is the inline header-homed accessor (CgsSyncTimeMessage.h); this .cpp is its
// definition home and pulls in the owning header so the class is the real
// Message-derived type rather than a throwaway local shape.

namespace CgsNetwork
{
    // Both times travel as whole seconds in [KI_MIN_SYNC_TIME, KI_MAX_SYNC_TIME) (three days)
    // plus a fraction at KF_MAX_SYNC_TIME_ERROR resolution (1/600 s); the two player ids as
    // ints in [-1, INT_MAX]. The minimum second is a zero-initialised data word that nothing
    // writes.
    const s32 KI_MIN_SYNC_TIME           = 0;
    const s32 KI_MAX_SYNC_TIME           = 259200;
    const f32 KF_MAX_SYNC_TIME_ERROR     = 0.0016666667f;
    const s32 KI_MIN_SYNC_TIME_PLAYER_ID = -1;
    const s32 KI_MAX_SYNC_TIME_PLAYER_ID = 0x7FFFFFFF;

    // Size the message with both times at zero and both player ids invalid.
    s32 SyncTimeMessage::GetPackedMessageSize()
    {
        mClientSendTime.SetFloatVal(0.0f);
        mHostTime.SetFloatVal(0.0f);
        mHostPlayerID   = -1;
        mClientPlayerID = -1;
        return Message::GetPackedMessageSize();
    }

    // The client send time, the host time, the host id and the client id, each status
    // OR-ed into the result.
    PackOrUnpackResult SyncTimeMessage::PackOrUnpack()
    {
        PackOrUnpackResult lxResult = PackOrUnpackTime(this, &mClientSendTime,
                                                       KI_MIN_SYNC_TIME, KI_MAX_SYNC_TIME, KF_MAX_SYNC_TIME_ERROR);
        lxResult |= PackOrUnpackTime(this, &mHostTime,
                                     KI_MIN_SYNC_TIME, KI_MAX_SYNC_TIME, KF_MAX_SYNC_TIME_ERROR);
        lxResult |= CgsNetwork::Message::PackOrUnpack(&mHostPlayerID,
                                    KI_MIN_SYNC_TIME_PLAYER_ID, KI_MAX_SYNC_TIME_PLAYER_ID);
        lxResult |= CgsNetwork::Message::PackOrUnpack(&mClientPlayerID,
                                    KI_MIN_SYNC_TIME_PLAYER_ID, KI_MAX_SYNC_TIME_PLAYER_ID);
        return lxResult;
    }

    // Fill an idle message with both player ids and both times.
    void SyncTimeMessage::Prepare(MessageWithPlayerIDs::NetworkPlayerID liClientPlayerID,
                                  CgsSystem::Time lClientSendTime,
                                  MessageWithPlayerIDs::NetworkPlayerID liHostPlayerID,
                                  CgsSystem::Time lHostTime)
    {
        CGS_ASSERT_AT(!IsMessageValid(),
            "!IsMessageValid()",
                "..\\..\\..\\GameShared\\GameClasses\\Network\\Time\\CgsSyncTimeMessage.cpp", 79 /* ARTIST 0x8287D430 */);


        mClientPlayerID = liClientPlayerID;
        mClientSendTime = lClientSendTime;
        mHostPlayerID   = liHostPlayerID;
        mHostTime       = lHostTime;
    }

    // Copy a received message out and consume it. False when nothing is pending.
    bool SyncTimeMessage::Retrieve(MessageWithPlayerIDs::NetworkPlayerID* lpClientID,
                                   CgsSystem::Time* lpClientTime,
                                   MessageWithPlayerIDs::NetworkPlayerID* lpHostID,
                                   CgsSystem::Time* lpHostTime)
    {
        CGS_ASSERT_AT(lpClientID,
            "lpClientID",
            CGS_ARTIST_FILE, 105 /* ARTIST 0x8288B18C */);
        CGS_ASSERT_AT(lpClientTime,
            "lpClientTime",
            CGS_ARTIST_FILE, 106 /* ARTIST 0x8288B1B0 */);
        CGS_ASSERT_AT(lpHostID,
            "lpHostID",
            CGS_ARTIST_FILE, 107 /* ARTIST 0x8288B1D4 */);
        CGS_ASSERT_AT(lpHostTime,
            "lpHostTime",
            CGS_ARTIST_FILE, 108 /* ARTIST 0x8288B1F8 */);

        // ARTIST 0x8288B20C branches to a trailing `return false` (0x8288B258): the copy-out is the body
        // of the valid test.
        if (IsMessageValid())
        {
            *lpClientID   = mClientPlayerID;
            *lpHostID     = mHostPlayerID;
            *lpClientTime = mClientSendTime;
            *lpHostTime   = mHostTime;
            SetMessageInvalid();
            return true;
        }
        return false;
    }
}
