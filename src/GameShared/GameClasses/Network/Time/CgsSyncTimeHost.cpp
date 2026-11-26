#include "GameShared/GameClasses/Network/Time/CgsSyncTimeHost.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameShared\\GameClasses\\Network\\Time\\CgsSyncTimeHost.cpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Network/Time/CgsSyncTimeBase.h"
#include "GameShared/GameClasses/Network/Time/CgsSyncTimeMessage.h"
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (CgsNetwork::SyncTimeHost -- host-side driver of the clock-sync round trip).
//
//   Construct @ 0x8287D380   (executed in the boot-trace goal milestone)
//   Prepare   @ 0x8288B078
//   Release   @ 0x8288B0E0
//   Update    @ 0x82892910
//
// Owning header: CgsSyncTimeHost.h. Every member/collaborator is touched by name; the
// X360 byte offsets in the asm (mpSyncTimeMessageManager @ +0x0, mpPlayerManager @ +0x4,
// the inlined GetNextLocalPlayerID read @ +0x23FC of the player manager, and the inlined
// receive-message scan over the manager's 0x38-byte slots) are the platform layout the
// natively-laid-out C++ structs stand in for.

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameShared\\GameClasses\\Network\\Time\\CgsSyncTimeHost.cpp".

namespace CgsNetwork
{
    // -------------------------------------------------------------------------------
    // Latch the borrowed player manager and sync-time message pool. The asm stores the
    // message manager at +0x0 and the player manager at +0x4 (`*this = a3; this[1] = a2`).
    void SyncTimeHost::Construct(PlayerManager* lpPlayerManager, SyncTimeMessageManager* lpMessageManager)
    {
        CGS_ASSERT_AT(lpPlayerManager != 0,
            "lpPlayerManager",
            CGS_ARTIST_FILE, 49 /* ARTIST 0x8287D3BC */);
        CGS_ASSERT_AT(lpMessageManager != 0,
            "lpMessageManager",
            CGS_ARTIST_FILE, 50 /* ARTIST 0x8287D3E0 */);

        // ARTIST 0x8287D3E8 stores the player manager (+4) before the message manager (+0).
        mpPlayerManager          = lpPlayerManager;
        mpSyncTimeMessageManager = lpMessageManager;
    }

    // -------------------------------------------------------------------------------
    void SyncTimeHost::Destruct()
    {
        // No owned resources to release: both members are borrowed pointers.
    }

    // -------------------------------------------------------------------------------
    // Enter the session as host: flip the shared message manager into host mode so its
    // slots are keyed by the client id.
    bool SyncTimeHost::Prepare()
    {
        CGS_ASSERT_AT(mpSyncTimeMessageManager != 0,
            "mpSyncTimeMessageManager", CGS_ARTIST_FILE, 70 /* ARTIST 0x8288B0A0 */);


        mpSyncTimeMessageManager->SwitchMode(SyncTimeMessageManager::E_MESSAGE_MODE_HOST);
        return true;
    }

    // -------------------------------------------------------------------------------
    // Leave the session: park the shared message manager back in the idle mode.
    bool SyncTimeHost::Release()
    {
        CGS_ASSERT_AT(mpSyncTimeMessageManager != 0,
            "mpSyncTimeMessageManager", CGS_ARTIST_FILE, 137 /* ARTIST 0x8288B108 */);


        mpSyncTimeMessageManager->SwitchMode(SyncTimeMessageManager::E_MESSAGE_MODE_NONE);
        return true;
    }

    // -------------------------------------------------------------------------------
    // Drain the received sync-time messages. For every received message whose recorded
    // host id matches our local player id, stamp the current host time onto the reply and
    // queue it back to the originating client. The X360 build inlines both the player
    // manager's GetNextLocalPlayerID() (read of mLocalPlayerID) and the message manager's
    // GetNextRecievedMessage() (the scan over the seven receive slots) into this body;
    // reversed back into the named calls the DWARF source hints record.
    void SyncTimeHost::Update(u16 lu16FrameNumber, CgsSystem::Time& lTime)
    {
        CGS_ASSERT_AT(mpPlayerManager != 0,
            "mpPlayerManager",
            CGS_ARTIST_FILE, 95 /* ARTIST 0x82892954 */);
        CGS_ASSERT_AT(mpSyncTimeMessageManager != 0,
            "mpSyncTimeMessageManager",
            CGS_ARTIST_FILE, 96 /* ARTIST 0x8289297C */);

        NetworkPlayerID lLocalPlayerID = K_INVALID_PLAYER_ID;
        mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID);

        SyncTimeMessage* lpMessage;
        while (mpSyncTimeMessageManager->GetNextRecievedMessage(&lpMessage))
        {
            // Pure out-params filled by Retrieve before any read (asm emits no init stores).
            NetworkPlayerID lMessageClientPlayerID;
            NetworkPlayerID lMessageHostPlayerID;
            CgsSystem::Time lMessageClientTime;
            CgsSystem::Time lMessageHostTime;

            CGS_ASSERT_AT(lpMessage != 0,
                "lpMessage",
                CGS_ARTIST_FILE, 108 /* ARTIST 0x82892A20 */);

            const bool lbRetrieved = lpMessage->Retrieve(&lMessageClientPlayerID,
                                                         &lMessageClientTime,
                                                         &lMessageHostPlayerID,
                                                         &lMessageHostTime);
            CGS_ASSERT_AT(lbRetrieved,
                "lpMessage->Retrieve( &lMessageClientPlayerID, &lMessageClientTime, " "&lMessageHostPlayerID, &lMessageHostTime )",
                CGS_ARTIST_FILE, 109 /* ARTIST 0x82892A5C */);

            if (lMessageHostPlayerID == lLocalPlayerID)
            {
                CgsSystem::Time lHostTime(lTime.GetFloatVal());

                mpSyncTimeMessageManager->PrepareSyncTimeMessageToSend(lu16FrameNumber,
                                                                       lMessageClientTime,
                                                                       lHostTime,
                                                                       lMessageHostPlayerID,
                                                                       lMessageClientPlayerID);
            }
        }
    }
} // namespace CgsNetwork

#undef CGS_ARTIST_FILE
