#include "GameShared/GameClasses/Network/Time/CgsSyncTimeBase.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (CgsNetwork::SyncTimeMessageManager -- the host/client clock-sync message pool).
//
//   Construct                    @ 0x8288A6A0
//   GetPlayerMessageIndex        @ 0x8287D138
//   RegisterMessages             @ 0x8288A830
//   UnregisterMessages           @ 0x8288A9F0
//   PrepareSyncTimeMessageToSend @ 0x8288AAE8
//   SwitchMode                   @ 0x8288AC18
//
// Owning header: CgsSyncTimeBase.h. Every member is touched by name; the X360 byte
// offsets in the asm (e.g. the 0x38-byte SyncTimeMessage stride, meMessageMode @ +0x32C)
// are the platform layout the natively-laid-out C++ struct stands in for.

namespace CgsNetwork
{
    // -------------------------------------------------------------------------------
    // Reset every slot to "free" and put the manager in the idle mode. The X360 build
    // inlines each SyncTimeMessage's field init inside the per-slot loop; reversed back
    // to a plain Construct() call on each message plus the explicit sentinels the loop
    // writes (free player id, -1 message player ids, zeroed timestamps).
    void SyncTimeMessageManager::Construct()
    {
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_SYNC_PLAYERS; ++liPlayerIndex)
        {
            mNetworkPlayerID[liPlayerIndex] = KI_INVALID_PLAYER_ID;

            SyncTimeMessage& lSendMessage = mSyncMessageToSend[liPlayerIndex];
            lSendMessage.Construct();
            lSendMessage.mClientSendTime.SetFloatVal(0.0f);
            lSendMessage.mHostTime.SetFloatVal(0.0f);
            lSendMessage.mHostPlayerID   = KI_INVALID_PLAYER_ID;
            lSendMessage.mClientPlayerID = KI_INVALID_PLAYER_ID;

            SyncTimeMessage& lReceiveMessage = mSyncMessageToReceive[liPlayerIndex];
            lReceiveMessage.Construct();
            lReceiveMessage.mClientSendTime.SetFloatVal(0.0f);
            lReceiveMessage.mHostTime.SetFloatVal(0.0f);
            lReceiveMessage.mHostPlayerID   = KI_INVALID_PLAYER_ID;
            lReceiveMessage.mClientPlayerID = KI_INVALID_PLAYER_ID;
        }

        meMessageMode = E_MESSAGE_MODE_NONE;
    }

    // -------------------------------------------------------------------------------
    void SyncTimeMessageManager::Destruct()
    {
        // No owned resources to release: the slots are value-type members.
    }

    // -------------------------------------------------------------------------------
    // Find the slot a given player id occupies, or -1 if the player is not registered.
    s32 SyncTimeMessageManager::GetPlayerMessageIndex(NetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(lPlayerID != KI_INVALID_PLAYER_ID, "lPlayerID != K_INVALID_PLAYER_ID");

        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_SYNC_PLAYERS; ++liPlayerIndex)
        {
            if (mNetworkPlayerID[liPlayerIndex] == lPlayerID)
            {
                return liPlayerIndex;
            }
        }

        return -1;
    }

    // -------------------------------------------------------------------------------
    // Claim a free slot for a newly-connecting player. Returns the slot index, or -1 if
    // the pool is exhausted.
    s32 SyncTimeMessageManager::AddNewPlayer(NetworkPlayerID lPlayerID)
    {
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_SYNC_PLAYERS; ++liPlayerIndex)
        {
            if (mNetworkPlayerID[liPlayerIndex] == KI_INVALID_PLAYER_ID)
            {
                mNetworkPlayerID[liPlayerIndex] = lPlayerID;
                return liPlayerIndex;
            }
        }

        CGS_ASSERT(false, "Ran out of sync time messages to use");
        return -1;
    }

    // -------------------------------------------------------------------------------
    // Register this manager's sync-time send/receive messages for a player and claim a
    // slot for it.
    bool SyncTimeMessageManager::RegisterMessages(NetworkPlayer* lpPlayer)
    {
        if (lpPlayer != nullptr)
        {
            CGS_ASSERT(!lpPlayer->IsMessageTypeRegistered(KI_E_MESSAGE_TYPE_SYNC_TIME),
                       "!lpPlayer->IsMessageTypeRegistered( E_MESSAGE_TYPE_SYNC_TIME )");
            CGS_ASSERT(GetPlayerMessageIndex(lpPlayer->GetPlayerID()) == -1,
                       "GetPlayerMessageIndex( lpPlayer->GetPlayerID() ) == -1");

            const s32 liPlayerIndex = AddNewPlayer(lpPlayer->GetPlayerID());
            CGS_ASSERT(liPlayerIndex != -1, "Got too many messages?");

            lpPlayer->RegisterMessageType(KI_E_MESSAGE_TYPE_SYNC_TIME,
                                          sizeof(SyncTimeMessage),
                                          &mSyncMessageToSend[liPlayerIndex],
                                          &mSyncMessageToReceive[liPlayerIndex],
                                          nullptr,
                                          nullptr,
                                          nullptr);
        }

        return true;
    }

    // -------------------------------------------------------------------------------
    // Tear down a player's sync-time registration and free its slot.
    bool SyncTimeMessageManager::UnregisterMessages(NetworkPlayer* lpPlayer)
    {
        if (lpPlayer != nullptr)
        {
            CGS_ASSERT(lpPlayer->IsMessageTypeRegistered(KI_E_MESSAGE_TYPE_SYNC_TIME),
                       "Trying to unregister sync time message that's not registered");

            lpPlayer->UnRegisterMessageType(KI_E_MESSAGE_TYPE_SYNC_TIME);

            const s32 liPlayerIndex = GetPlayerMessageIndex(lpPlayer->GetPlayerID());

            mSyncMessageToSend[liPlayerIndex].SetMessageInvalid();
            mSyncMessageToReceive[liPlayerIndex].SetMessageInvalid();
            mNetworkPlayerID[liPlayerIndex] = KI_INVALID_PLAYER_ID;
        }

        return true;
    }

    // -------------------------------------------------------------------------------
    // Fill in and queue the outgoing sync-time message for the appropriate slot. When
    // acting as host the slot is keyed by the client id; as a client it is keyed by the
    // host id.
    void SyncTimeMessageManager::PrepareSyncTimeMessageToSend(u16 lu16FrameNum,
                                                              CgsSystem::Time& lClientSendTime,
                                                              CgsSystem::Time& lHostTime,
                                                              NetworkPlayerID  lHostPlayerID,
                                                              NetworkPlayerID  lClientPlayerID)
    {
        NetworkPlayerID lLookupPlayerID;
        switch (meMessageMode)
        {
        case E_MESSAGE_MODE_HOST:
            lLookupPlayerID = lClientPlayerID;
            break;
        case E_MESSAGE_MODE_CLIENT:
            lLookupPlayerID = lHostPlayerID;
            break;
        default:
            CGS_ASSERT(false, "Sync time Message Manager in state none");
            return;
        }

        const s32        liMessageIndex = GetPlayerMessageIndex(lLookupPlayerID);
        SyncTimeMessage& lMessage       = mSyncMessageToSend[liMessageIndex];

        if (!lMessage.IsMessageValid())
        {
            lMessage.Prepare(lClientPlayerID, lClientSendTime, lHostPlayerID, lHostTime);
            lMessage.PrepareForSend(KI_E_MESSAGE_TYPE_SYNC_TIME, lu16FrameNum);
        }
    }

    // -------------------------------------------------------------------------------
    // Move this manager into a new host/client/none mode and clear the queued/valid
    // state on every live slot so nothing stale is sent under the new role.
    void SyncTimeMessageManager::SwitchMode(EMessageMode leMessageMode)
    {
        CGS_ASSERT(leMessageMode >= 0, "leMessageMode >= 0");
        CGS_ASSERT(leMessageMode <= E_MESSAGE_MODE_NONE, "leMessageMode <= E_MESSAGE_MODE_NONE");

        meMessageMode = leMessageMode;

        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_SYNC_PLAYERS; ++liPlayerIndex)
        {
            if (mNetworkPlayerID[liPlayerIndex] != KI_INVALID_PLAYER_ID)
            {
                mSyncMessageToSend[liPlayerIndex].SetMessageInvalid();
                mSyncMessageToReceive[liPlayerIndex].SetMessageInvalid();
            }
        }
    }
} // namespace CgsNetwork
