// ===================================================================================
// CgsNetwork::TimeManager -- implementation
//   b5-decomp/src/GameShared/GameClasses/Network/Time/CgsTimeManager.cpp
//
// Reconstructed from the X360 ARTIST binary, shaped by the DecFIGS DWARF and the
// Feb-2007 idiom. Recovered functions in this TU:
//   Construct                     @0x82898EE0
//   Prepare                       @0x82898FA0
//   Update                        @0x82892B90
//   Release                       @0x82899090
//   AddPlayer                     @0x82892E18
//   RemovePlayer                  @0x82892F10
//   PrepareSyncTimeManagers       @0x8288B420
//   GetU16FrameCountSinceStart    @0x8288B2D0
//   GetFrameWrapCountSinceStart   @0x82893310
//   GetFramesSinceStart           @0x82893388
//   NextFrame                     @0x825813F0
//
// The remaining DWARF-declared methods (SetStartFrame / the simple
// getters / ResetNetworkTime / Start/StopSyncingTime / Disconnected / Destruct /
// OnHostMigration / ReleaseSyncTimeManagers / the start-frame helpers) were not in
// this TU's binary export; they are declared in the header and bodied in their own
// waves. Only the ten functions above are defined here.
//
// The original streamed a handful of dev-log lines ("Cleared Start frame", "...called",
// "SyncTimeManager invalid status ...") through a CgsDev::StrStream debug stream; per
// the project convention those dev-log streams are dropped, and the asserts that built
// a message through gpcMessageBuffer lower to the project CGS_ASSERT macro.
// ===================================================================================

#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"

#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"   // PlayerManager
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"   // NetworkPlayer
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT

namespace CgsNetwork
{
    // ---- Construct @0x82898EE0 ------------------------------------------------------
    // Build the three sync sub-objects, latch the player registry, and clear the clocks,
    // frame counter and start-frame bookmark.
    void
    TimeManager::Construct(PlayerManager* lpPlayerManager)
    {
        mSyncTimeMessageManager.Construct();
        mSyncTimeClient.Construct(lpPlayerManager, &mSyncTimeMessageManager);
        mSyncTimeHost.Construct(lpPlayerManager, &mSyncTimeMessageManager);

        mpPlayerManager         = lpPlayerManager;
        muFrameCount            = 0;
        miPlayersAdded          = 0;
        meSyncTimePrepareStatus = E_SYNC_TIME_PREPARE_STATUS_NONE;

        mGlobalTime.SetFloatVal(0.f);
        mbWeAreSyncingTime      = false;
        mNetworkTime.SetFloatVal(0.f);

        // Clear the start-frame bookmark (original dev-logged "Cleared Start frame").
        mu16StartFrame          = KU16_INVALID_FRAME;   // asm: sth -1
        miStartFrameWrapCount   = 0;
    }

    // ---- Prepare @0x82898FA0 --------------------------------------------------------
    // Per-session reset before any players join. Asserts no players are still added,
    // clears the sync state and clock, and re-arms the start-frame bookmark.
    bool
    TimeManager::Prepare()
    {
        // Original dev-logged "CgsNetwork::TimeManager::Prepare called".
        CGS_ASSERT(miPlayersAdded == 0, "miPlayersAdded == 0");

        meSyncTimePrepareStatus = E_SYNC_TIME_PREPARE_STATUS_NONE;
        mbWeAreSyncingTime      = false;
        mNetworkTime.SetFloatVal(0.f);

        // Clear the start-frame bookmark.
        mu16StartFrame          = KU16_INVALID_FRAME;
        miStartFrameWrapCount   = 0;

        return true;
    }

    // ---- Update @0x82892B90 ---------------------------------------------------------
    // Drive whichever half of the clock-sync handshake is active this session. Does
    // nothing unless we are actively syncing time.
    void
    TimeManager::Update()
    {
        if (!mbWeAreSyncingTime)
        {
            return;
        }

        switch (meSyncTimePrepareStatus)
        {
        case E_SYNC_TIME_PREPARE_STATUS_NONE:
            // Not preparing either side -- nothing to do (asm: status < 1 falls through).
            break;

        case E_SYNC_TIME_PREPARE_STATUS_HOST:
        {
            CgsSystem::Time lNetworkTime = mNetworkTime;
            mSyncTimeHost.Update(GetU16FrameCount(), lNetworkTime);
            break;
        }

        case E_SYNC_TIME_PREPARE_STATUS_CLIENT:
            mSyncTimeClient.Update(GetU16FrameCount(), &mNetworkTime, &mGlobalTime);
            break;

        default:
            CGS_ASSERT(false, "SyncTimeManager invalid status in CgsNetwork::TimeManager::Update");
            break;
        }
    }

    // ---- Release @0x82899090 --------------------------------------------------------
    // Tear down the sync managers and clear the clock, start-frame bookmark and counts.
    bool
    TimeManager::Release()
    {
        ReleaseSyncTimeManagers();

        mNetworkTime.SetFloatVal(0.f);

        // Clear the start-frame bookmark.
        mu16StartFrame        = KU16_INVALID_FRAME;
        miStartFrameWrapCount = 0;

        miPlayersAdded        = 0;
        mbWeAreSyncingTime    = false;

        return true;
    }

    // ---- PrepareSyncTimeManagers @0x8288B420 ----------------------------------------
    // Switch the manager into host or client mode and prepare the matching sub-object.
    void
    TimeManager::PrepareSyncTimeManagers(bool lbIAmHost, NetworkPlayerID lNewHostID)
    {
        meSyncTimePrepareStatus = lbIAmHost ? E_SYNC_TIME_PREPARE_STATUS_HOST
                                            : E_SYNC_TIME_PREPARE_STATUS_CLIENT;

        switch (meSyncTimePrepareStatus)
        {
        case E_SYNC_TIME_PREPARE_STATUS_HOST:
            CGS_ASSERT(mSyncTimeHost.Prepare(), "mSyncTimeHost.Prepare()");
            break;

        case E_SYNC_TIME_PREPARE_STATUS_CLIENT:
            CGS_ASSERT(mSyncTimeClient.Prepare(lNewHostID), "mSyncTimeClient.Prepare(lNewHostID)");
            break;

        default:
            CGS_ASSERT(false, "Can't prepare state in CgsNetwork::TimeManager::PrepareSyncTimeManagers");
            break;
        }
    }

    // ---- AddPlayer @0x82892E18 ------------------------------------------------------
    // A player joined: re-prepare the sync managers against the current host and, if the
    // joining player is one of ours, register its sync-time messages.
    void
    TimeManager::AddPlayer(NetworkPlayerID lPlayerID, bool lbIAmHost)
    {
        ReleaseSyncTimeManagers();
        PrepareSyncTimeManagers(lbIAmHost, mpPlayerManager->GetHostPlayerID());

        CGS_ASSERT(mpPlayerManager != 0, "mpPlayerManager");

        if (mpPlayerManager->IsLocalPlayer(lPlayerID))
        {
            mSyncTimeMessageManager.RegisterMessages(mpPlayerManager->GetPlayerByID(lPlayerID));
        }

        ++miPlayersAdded;
        CGS_ASSERT(miPlayersAdded <= KI_MAX_PLAYERS, "miPlayersAdded <= KI_MAX_PLAYERS");
    }

    // ---- RemovePlayer @0x82892F10 ---------------------------------------------------
    // A player left: re-prepare the sync managers against the (possibly new) host, and if
    // the leaving player was one of ours, unregister its sync-time messages.
    void
    TimeManager::RemovePlayer(NetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(mpPlayerManager != 0, "mpPlayerManager");
        CGS_ASSERT(miPlayersAdded > 0, "miPlayersAdded > 0");

        --miPlayersAdded;
        ReleaseSyncTimeManagers();

        const NetworkPlayerID lHostID = mpPlayerManager->GetHostPlayerID();
        if (lHostID != lPlayerID && lHostID != MessageWithPlayerIDs::KI_INVALID_PLAYER_ID)
        {
            const bool lbIAmHost = (lHostID == mpPlayerManager->GetLocalPlayerID());
            PrepareSyncTimeManagers(lbIAmHost, lHostID);
        }

        if (mpPlayerManager->IsLocalPlayer(lPlayerID))
        {
            mSyncTimeMessageManager.UnregisterMessages(mpPlayerManager->GetPlayerByID(lPlayerID));
        }
    }

    // ---- GetU16FrameCountSinceStart @0x8288B2D0 -------------------------------------
    // The current frame number relative to the start-frame bookmark, in the 16-bit wire
    // window. Wraps modulo KU_FRAME_WRAP_MODULUS (0xFFFF, not 0x10000), so a borrow is
    // applied when the current frame is below the bookmark.
    u16
    TimeManager::GetU16FrameCountSinceStart() const
    {
        const u16 lu16Now   = GetU16FrameCount();
        const u16 lu16Start = mu16StartFrame;

        u16 lu16Result = static_cast<u16>(lu16Now - lu16Start);
        if (lu16Now < lu16Start)
        {
            --lu16Result;   // borrow into the 0xFFFF-wide window
        }

        CGS_ASSERT(lu16Result != KU16_INVALID_FRAME, "lu16Result != KU16_INVALID_FRAME");
        return lu16Result;
    }

    // ---- GetFrameWrapCountSinceStart @0x82893310 ------------------------------------
    // How many full 16-bit frame wraps have elapsed since the start-frame bookmark.
    s32
    TimeManager::GetFrameWrapCountSinceStart() const
    {
        // SIGNED divide: the asm @0x82893310 emits the sign-test + (v-1)/0xFFFF correction
        // codegen for signed division by 65535 (diverges from unsigned for muFrameCount>=0x80000000).
        const s32 liWrapsNow = static_cast<s32>(muFrameCount) / static_cast<s32>(KU_FRAME_WRAP_MODULUS);

        s32 liResult = liWrapsNow - miStartFrameWrapCount;
        if (GetU16FrameCount() < mu16StartFrame)
        {
            --liResult;
        }

        return liResult;
    }

    // ---- GetFramesSinceStart @0x82893388 --------------------------------------------
    // The total (non-wrapped) frame count elapsed since the start-frame bookmark.
    s32
    TimeManager::GetFramesSinceStart() const
    {
        const u16 lu16FramesSinceStart = GetU16FrameCountSinceStart();
        const s32 liWrapsSinceStart    = GetFrameWrapCountSinceStart();

        return static_cast<s32>(lu16FramesSinceStart)
             + static_cast<s32>(KU_FRAME_WRAP_MODULUS) * liWrapsSinceStart;
    }

    // ---- GetU16FrameCount -----------------------------------------------------------
    // The running network frame counter reduced into the 16-bit wire window. The X360
    // call sites inline this `muFrameCount % 0xFFFF` reduction (the 0x80008001
    // reciprocal-multiply idiom); it is provided here as the named helper the other
    // frame queries and the GetCurrentFrameNumber() consumer alias share.
    u16
    TimeManager::GetU16FrameCount() const
    {
        return static_cast<u16>(muFrameCount % KU_FRAME_WRAP_MODULUS);
    }

    // ---- NextFrame @0x825813F0 ------------------------------------------------------
    // Advance both clocks by this frame's timer step and bump the running frame counter.
    // The step is the timer status' current time step (mfBaseTimeStep * mfTimeStepMultiplier,
    // asm: lfs 8(r4) * lfs 4(r4) into fp31, computed once and reused for both += Time temps).
    void
    TimeManager::NextFrame(const CgsSystem::TimerStatus* lpTimerStatus)
    {
        const f32 lfTimeStep = lpTimerStatus->GetCurrentTimeStep();

        mNetworkTime += CgsSystem::Time(lfTimeStep);   // this+0x370 += Time(step)
        mGlobalTime  += CgsSystem::Time(lfTimeStep);   // this+0x378 += Time(step)

        ++muFrameCount;                                // this+0x388
    }
} // namespace CgsNetwork
