// ===================================================================================
// CgsNetwork::SyncTimeClient -- implementation
//   b5-decomp/src/GameShared/GameClasses/Network/Time/CgsSyncTimeClient.cpp
//
// Reconstructed from the X360 ARTIST binary (Construct @0x8287D1C0,
// Prepare @0x8288ADA0, Update @0x82892378, Release @0x8288AF48,
// TimeIsSynchronised @0x8287D240), shaped by the DecFIGS DWARF and the Feb-2007 idiom.
//
// The client side of the network clock sync. Once Prepare()d against a host, each
// Update() pulls a received SyncTimeMessage, measures its round-trip delay and the
// host/client time difference, keeps a 4-deep rolling window of both, derives the
// estimated host clock into lpNetworkTime, and -- every KU16_SYNC_SEND_FRAME_TIMEOUT
// frames while connected -- sends a fresh sync-time message back to the host.
// ===================================================================================

#include "GameShared/GameClasses/Network/Time/CgsSyncTimeClient.h"

#include "GameShared/GameClasses/Network/Time/CgsSyncTimeBase.h"      // SyncTimeMessageManager
#include "GameShared/GameClasses/Network/Time/CgsSyncTimeMessage.h"   // SyncTimeMessage
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"  // PlayerManager, EConnectionStatus
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h" // GetFrameDiffWrapped16
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT

#include <cmath>

namespace CgsNetwork
{
    // --- file-scope constants (DWARF CgsSyncTimeClient.cpp:47/48) --------------------

    // How many network frames may elapse before the client re-sends a sync request to
    // the host (asm: GetFrameDiffWrapped16(...) >= 6u).
    static const u16 KU16_SYNC_SEND_FRAME_TIMEOUT = 6;

    // How long (frames) the host may stay unavailable before the client gives up.
    static const s32 KI_HOST_UNAVAILABLE_TIMEOUT = 600;

    // ---- Construct @0x8287D1C0 ------------------------------------------------------
    void
    SyncTimeClient::Construct(PlayerManager* lpPlayerManager, SyncTimeMessageManager* lpMessageManager)
    {
        CGS_ASSERT(lpPlayerManager != 0, "lpPlayerManager");
        CGS_ASSERT(lpMessageManager != 0, "lpMessageManager");

        mpSyncTimeMessageManager = lpMessageManager;   // *v4    = a3
        mpPlayerManager          = lpPlayerManager;     // v4[1]  = a2
        mHostPlayerID            = KI_INVALID_PLAYER_ID; // v4[2] = -1
    }

    // ---- Destruct @ (DWARF CgsSyncTimeClient.cpp:303) -------------------------------
    void
    SyncTimeClient::Destruct()
    {
        // No teardown beyond the implicit member lifetimes (the X360 body is empty save
        // for the gpr save/restore frame).
    }

    // ---- Prepare @0x8288ADA0 --------------------------------------------------------
    // Latch the host we will synchronise to and reset the rolling measurement windows.
    bool
    SyncTimeClient::Prepare(NetworkPlayerID lHostPlayerID)
    {
        mHostPlayerID = lHostPlayerID;

        CGS_ASSERT(mHostPlayerID != KI_INVALID_PLAYER_ID,
                   "mHostPlayerID != K_INVALID_PLAYER_ID");

        // We must not be the host ourselves: the host has no registered SyncTime player
        // slot for itself. (Original streamed the message through a StrStream.)
        NetworkPlayer* lpHostPlayer = mpPlayerManager->GetPlayerByID(mHostPlayerID);
        CGS_ASSERT(lpHostPlayer != 0,
                   "Trying to prepare the sync time client when we are host.\n");

        mpSyncTimeMessageManager->SwitchMode(SyncTimeMessageManager::E_MESSAGE_MODE_CLIENT);

        mu8DelayHead              = 0;
        miNumTimePackets          = 0;
        mu8NumTimePacketsStored   = 0;
        mu8TimeDifferenceHead     = 0;
        mu16LastSyncSentFrame     = 0xFFFF;
        mu8NumTimeDifferenceStored = 0;

        for (s32 liIndex = 0; liIndex < KI_NUM_DELAYS_STORE; ++liIndex)
        {
            mafDelays[liIndex] = 0.0f;
        }
        // mafTimeDifference is seeded with FLT_MAX (0x7F7FFFFF) so an un-measured slot
        // never passes the synchronisation tolerance test.
        for (s32 liIndex = 0; liIndex < KI_NUM_TIME_DIFFS_STORE; ++liIndex)
        {
            mafTimeDifference[liIndex] = 3.4028234663852886e+38f; // FLT_MAX, asm 2139095039
        }

        return true;
    }

    // ---- Release @0x8288AF48 --------------------------------------------------------
    bool
    SyncTimeClient::Release()
    {
        CGS_ASSERT(mHostPlayerID != KI_INVALID_PLAYER_ID,
                   "mHostPlayerID != K_INVALID_PLAYER_ID");

        NetworkPlayer* lpHostPlayer = mpPlayerManager->GetPlayerByID(mHostPlayerID);
        CGS_ASSERT(lpHostPlayer != 0,
                   "We are trying to release the sync time client when the local player is host\n");

        mpSyncTimeMessageManager->SwitchMode(SyncTimeMessageManager::E_MESSAGE_MODE_NONE);
        mHostPlayerID = KI_INVALID_PLAYER_ID;

        return true;
    }

    // ---- TimeIsSynchronised @0x8287D240 ---------------------------------------------
    // True once we have processed at least liMinNumPacketsRecvd sync packets and the
    // average measured host/client time difference is within lTimeDiffTolerance.
    bool
    SyncTimeClient::TimeIsSynchronised(CgsSystem::Time lTimeDiffTolerance, s32 liMinNumPacketsRecvd) const
    {
        CGS_ASSERT(lTimeDiffTolerance.GetFloatVal() > 0.0f,
                   "lTimeDiffTolerance.GetFloatVal() > 0.0f");
        CGS_ASSERT(liMinNumPacketsRecvd >= 0,
                   "liMinNumPacketsRecvd >= 0");

        if (miNumTimePackets < liMinNumPacketsRecvd)
        {
            return false;
        }

        f32 lfAverageTimeDiff = 0.0f;
        for (u8 lu8Index = 0; lu8Index < mu8NumTimeDifferenceStored; ++lu8Index)
        {
            lfAverageTimeDiff += mafTimeDifference[lu8Index];
        }
        lfAverageTimeDiff /= static_cast<f32>(mu8NumTimeDifferenceStored);

        return (std::fabs(lfAverageTimeDiff) <= lTimeDiffTolerance.GetFloatVal());
    }

    // ---- Update @0x82892378 ---------------------------------------------------------
    // Process at most one received sync-time message, fold its delay / time-difference
    // measurements into the rolling windows, publish the estimated host clock into
    // lpNetworkTime, and periodically send a fresh sync request back to the host.
    void
    SyncTimeClient::Update(u16 lu16FrameNum, CgsSystem::Time* lpNetworkTime, const CgsSystem::Time* lpGlobalTime)
    {
        // Identify our own (local client) player id; if there is no local player there is
        // nothing for the client sync to do this frame.
        NetworkPlayerID lClientPlayerID = KI_INVALID_PLAYER_ID;
        if (!mpPlayerManager->GetNextLocalPlayerID(&lClientPlayerID))
        {
            return;
        }

        SyncTimeMessage* lpMessage = 0;
        if (mpSyncTimeMessageManager->GetNextRecievedMessage(&lpMessage))
        {
            CGS_ASSERT(lpMessage != 0, "lpMessage");

            NetworkPlayerID lMessageClietPlayerID = KI_INVALID_PLAYER_ID;
            NetworkPlayerID lMessageHostPlayerID  = KI_INVALID_PLAYER_ID;
            CgsSystem::Time lMessageClientTime;
            CgsSystem::Time lMessageHostTime;

            bool lbRetrieved = lpMessage->Retrieve(&lMessageClietPlayerID,
                                                   &lMessageClientTime,
                                                   &lMessageHostPlayerID,
                                                   &lMessageHostTime);
            CGS_ASSERT(lbRetrieved,
                       "lpMessage->Retrieve( &lMessageClietPlayerID, &lMessageClientTime, "
                       "&lMessageHostPlayerID, &lMessageHostTime )");

            // Only fold in a message that is the round-trip of our own request to our host.
            if (lMessageClietPlayerID == lClientPlayerID && lMessageHostPlayerID == mHostPlayerID)
            {
                // Round-trip delay = (now) - (the time we originally stamped on the request).
                CGS_ASSERT(*lpGlobalTime >= lMessageClientTime,
                           "Current Time is later than message send time");

                CgsSystem::Time lDelay = *lpGlobalTime - lMessageClientTime;

                mafDelays[mu8DelayHead] = lDelay.GetFloatVal();
                mu8DelayHead = static_cast<u8>((mu8DelayHead + 1) % KI_NUM_DELAYS_STORE);
                if (mu8NumTimePacketsStored < KI_NUM_DELAYS_STORE)
                {
                    ++mu8NumTimePacketsStored;
                }
                ++miNumTimePackets;

                // One-way delay estimate = half the averaged round trip.
                f32 lfDelaySum = 0.0f;
                for (u8 lu8Index = 0; lu8Index < mu8NumTimePacketsStored; ++lu8Index)
                {
                    CGS_ASSERT(mafDelays[lu8Index] >= 0.0f, "mafDelays[lu8Index] >= 0.0f");
                    lfDelaySum += mafDelays[lu8Index];
                }
                f32 lfAverageDelay = (lfDelaySum / static_cast<f32>(mu8NumTimePacketsStored)) * 0.5f;
                CgsSystem::Time lAverageDelay(lfAverageDelay);

                // Time difference = incoming network time - (host stamp + one-way delay).
                // asm 0x828926F4: operator-(this=lpNetworkTime, rhs=lEstimatedHostNow), read
                // BEFORE the SetFloatVal publish below overwrites *lpNetworkTime.
                CgsSystem::Time lEstimatedHostNow = lMessageHostTime + lAverageDelay;
                CgsSystem::Time lTimeDiff         = *lpNetworkTime - lEstimatedHostNow;

                mafTimeDifference[mu8TimeDifferenceHead] = lTimeDiff.GetFloatVal();
                mu8TimeDifferenceHead = static_cast<u8>((mu8TimeDifferenceHead + 1) % KI_NUM_TIME_DIFFS_STORE);
                if (mu8NumTimeDifferenceStored < KI_NUM_TIME_DIFFS_STORE)
                {
                    ++mu8NumTimeDifferenceStored;
                }

                CGS_ASSERT(lMessageHostTime >= CgsSystem::Time(0.0f), "lMessageHostTime >= 0.0f");

                // Publish the estimated host clock = host stamp + one-way delay.
                lpNetworkTime->SetFloatVal(lMessageHostTime.GetFloatVal() + lfAverageDelay);

                CGS_ASSERT(lpNetworkTime->GetFloatVal() >= 0.0f,
                           "lpNetworkTime->GetFloatVal() >= 0.0f");
            }

            // Exactly one sync-time message is expected to be pending per frame.
            SyncTimeMessage* lpExtraMessage = 0;
            CGS_ASSERT(!mpSyncTimeMessageManager->GetNextRecievedMessage(&lpExtraMessage),
                       "!mpSyncTimeMessageManager->GetNextRecievedMessage( &lpMessage )");
        }

        // While the host link is up, re-send our sync request periodically.
        if (mpPlayerManager->GetConnectionStatus(mHostPlayerID) == E_CONNECTION_SUCCESS)
        {
            if (mu16LastSyncSentFrame == 0xFFFF ||
                GetFrameDiffWrapped16(lu16FrameNum, mu16LastSyncSentFrame) >= KU16_SYNC_SEND_FRAME_TIMEOUT)
            {
                mu16LastSyncSentFrame = lu16FrameNum;

                CgsSystem::Time lClientSendTime(lpGlobalTime->GetFloatVal());
                CgsSystem::Time lHostTime(0.0f);

                mpSyncTimeMessageManager->PrepareSyncTimeMessageToSend(lu16FrameNum,
                                                                       lClientSendTime,
                                                                       lHostTime,
                                                                       mHostPlayerID,
                                                                       lClientPlayerID);
            }
        }
    }
} // namespace CgsNetwork
