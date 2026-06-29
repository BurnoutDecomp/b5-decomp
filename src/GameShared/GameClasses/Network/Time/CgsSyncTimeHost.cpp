#include "GameShared/GameClasses/Network/Time/CgsSyncTimeHost.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
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

namespace CgsNetwork
{
    // -------------------------------------------------------------------------------
    // Latch the borrowed player manager and sync-time message pool. The asm stores the
    // message manager at +0x0 and the player manager at +0x4 (`*this = a3; this[1] = a2`).
    void SyncTimeHost::Construct(PlayerManager* lpPlayerManager, SyncTimeMessageManager* lpMessageManager)
    {
        CGS_ASSERT(lpPlayerManager != nullptr, "lpPlayerManager");
        CGS_ASSERT(lpMessageManager != nullptr, "lpMessageManager");

        mpSyncTimeMessageManager = lpMessageManager;
        mpPlayerManager          = lpPlayerManager;
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
        CGS_ASSERT(mpSyncTimeMessageManager != nullptr, "mpSyncTimeMessageManager");

        mpSyncTimeMessageManager->SwitchMode(SyncTimeMessageManager::E_MESSAGE_MODE_HOST);
        return true;
    }

    // -------------------------------------------------------------------------------
    // Leave the session: park the shared message manager back in the idle mode.
    bool SyncTimeHost::Release()
    {
        CGS_ASSERT(mpSyncTimeMessageManager != nullptr, "mpSyncTimeMessageManager");

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
        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");
        CGS_ASSERT(mpSyncTimeMessageManager != nullptr, "mpSyncTimeMessageManager");

        NetworkPlayerID lLocalPlayerID;
        mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID);

        SyncTimeMessage* lpMessage;
        while (mpSyncTimeMessageManager->GetNextRecievedMessage(&lpMessage))
        {
            // Pure out-params filled by Retrieve before any read (asm emits no init stores).
            NetworkPlayerID lMessageClientPlayerID;
            NetworkPlayerID lMessageHostPlayerID;
            CgsSystem::Time lMessageClientTime;
            CgsSystem::Time lMessageHostTime;

            CGS_ASSERT(lpMessage != nullptr, "lpMessage");

            const bool lbRetrieved = lpMessage->Retrieve(&lMessageClientPlayerID,
                                                         &lMessageClientTime,
                                                         &lMessageHostPlayerID,
                                                         &lMessageHostTime);
            CGS_ASSERT(lbRetrieved,
                       "lpMessage->Retrieve( &lMessageClientPlayerID, &lMessageClientTime, "
                       "&lMessageHostPlayerID, &lMessageHostTime )");

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
