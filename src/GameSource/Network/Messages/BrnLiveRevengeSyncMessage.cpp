#include "GameSource/Network/Messages/BrnLiveRevengeSyncMessage.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                         // CGS_ASSERT
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"  // PackOrUnpackInt / PackOrUnpackDateAndTime

// BrnNetwork::LiveRevengeSyncMessage -- carries one LiveRevengeRelationship (at +0x28, straight
// after the 0x28-byte reliable-message base) to the rival it describes. The sender stages its
// relationship; the receiver retrieves it flipped to its own point of view and merges it.
namespace BrnNetwork
{
    void LiveRevengeSyncMessage::Construct()
    {
        // Inlined MessageWithPlayerIDs base init (the two inherited player ids -> -1), then the
        // Message::Construct chain.
        mSendingPlayerID = CgsNetwork::MessageWithPlayerIDs::KI_INVALID_PLAYER_ID;
        mRecvingPlayerID = CgsNetwork::MessageWithPlayerIDs::KI_INVALID_PLAYER_ID;
        CgsNetwork::Message::Construct();

        mLiveRevengeRelationship.Destruct();
    }

    void LiveRevengeSyncMessage::Release()
    {
        mLiveRevengeRelationship.Release();
        SetMessageInvalid();
    }

    void LiveRevengeSyncMessage::Destruct()
    {
        mLiveRevengeRelationship.Destruct();
    }

    // Stage the relationship for sending, unless a staged copy is still waiting to go out.
    void LiveRevengeSyncMessage::PrepareForSend(const LiveRevengeRelationship* lpLiveRevengeRelationship,
                                                u16 lu16Frame)
    {
        if (!IsMessageValid())
        {
            mLiveRevengeRelationship = *lpLiveRevengeRelationship;
            ReliableMessage::PrepareForSend(KI_LIVE_REVENGE_SYNC_MESSAGE_TYPE, lu16Frame);
            CGS_ASSERT(IsReliable(), "IsReliable()");
        }
    }

    // Hand a received relationship over from the rival's point of view to ours, then release the
    // slot. False when nothing has arrived.
    bool LiveRevengeSyncMessage::Retrieve(LiveRevengeRelationship* lpLiveRevengeRelationship)
    {
        if (!IsMessageValid())
        {
            return false;
        }

        *lpLiveRevengeRelationship = mLiveRevengeRelationship;
        lpLiveRevengeRelationship->FlipPointOfView();
        mLiveRevengeRelationship.Release();
        SetMessageInvalid();
        return true;
    }

    // Clear the payload so the size is the same every call, then let the base pack and measure.
    s32 LiveRevengeSyncMessage::GetPackedMessageSize()
    {
        mLiveRevengeRelationship.Clear();
        return ReliableMessage::GetPackedMessageSize();
    }

    // The payload travels as the change time, the event count, the running score and both stat
    // blocks. Each field is copied out, (de)serialised, and written back.
    CgsNetwork::PackOrUnpackResult LiveRevengeSyncMessage::PackOrUnpack()
    {
        CgsNetwork::PackOrUnpackResult lxResult = ReliableMessage::PackOrUnpack();

        CgsSystem::DateAndTime lLastTimeChanged = mLiveRevengeRelationship.GetLastChangedTime();
        s32                    liTotalEvents    = mLiveRevengeRelationship.GetTotalEvents();
        s32                    liCurrentScore   = mLiveRevengeRelationship.GetCurrentScoreForLocalPlayer();
        CommonRelationship     lOverallStats    = *mLiveRevengeRelationship.GetOverallStats();

        lxResult |= CgsNetwork::PackOrUnpackDateAndTime(this, &lLastTimeChanged);
        lxResult |= CgsNetwork::PackOrUnpackInt(this, &liTotalEvents, 0, 0x7FFFFF);
        lxResult |= CgsNetwork::PackOrUnpackInt(this, &liCurrentScore, -0x7FFF, 0x7FFF);
        lxResult |= PackOrUnpack(&lOverallStats);

        mLiveRevengeRelationship.SetLastTimeChanged(lLastTimeChanged);
        mLiveRevengeRelationship.SetTotalNumberOfEvents(liTotalEvents);
        mLiveRevengeRelationship.SetCurrentScoreForPlayer(liCurrentScore);
        mLiveRevengeRelationship.SetOverallStats(&lOverallStats);

        return lxResult;
    }

    CgsNetwork::PackOrUnpackResult LiveRevengeSyncMessage::PackOrUnpack(CommonRelationship* lpCommonRelationship)
    {
        CgsNetwork::PackOrUnpackResult lxResult = PackOrUnpack(&lpCommonRelationship->mPlayerStats);
        lxResult |= PackOrUnpack(&lpCommonRelationship->mRivalStats);
        return lxResult;
    }

    // Six of the nine stats travel, each as a non-negative int, in this wire order. The event
    // gap and the two payback counts stay local.
    CgsNetwork::PackOrUnpackResult LiveRevengeSyncMessage::PackOrUnpack(CommonRelationshipStats* lpCommonRelationshipStats)
    {
        CgsNetwork::PackOrUnpackResult lxResult =
            CgsNetwork::PackOrUnpackInt(this, &lpCommonRelationshipStats->miWins, 0, 0x7FFFFFFF);
        lxResult |= CgsNetwork::PackOrUnpackInt(this, &lpCommonRelationshipStats->miLongestStreak, 0, 0x7FFFFFFF);
        lxResult |= CgsNetwork::PackOrUnpackInt(this, &lpCommonRelationshipStats->miMarks, 0, 0x7FFFFFFF);
        lxResult |= CgsNetwork::PackOrUnpackInt(this, &lpCommonRelationshipStats->miScalps, 0, 0x7FFFFFFF);
        lxResult |= CgsNetwork::PackOrUnpackInt(this, &lpCommonRelationshipStats->miScoresSettled, 0, 0x7FFFFFFF);
        lxResult |= CgsNetwork::PackOrUnpackInt(this, &lpCommonRelationshipStats->miTakedowns, 0, 0x7FFFFFFF);
        return lxResult;
    }
}
