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
// Also here: ReleaseSyncTimeManagers, SetStartFrame, IsTimeSynchronised,
// OnHostMigration, StartSyncingTime, Disconnected and the two frame/wrap helpers. The
// trivial accessors are inline in the header; ResetNetworkTime and Destruct have no
// caller in the tree and stay declared only.
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
        // A count that is negative as a signed value takes the second reduction, which
        // rounds the wrap count down rather than toward zero.
        s32 liWrapsNow;
        if (static_cast<s32>(muFrameCount) >= 0)
        {
            liWrapsNow = static_cast<s32>(muFrameCount / KU_FRAME_WRAP_MODULUS);
        }
        else
        {
            liWrapsNow = static_cast<s32>((muFrameCount - 1) / KU_FRAME_WRAP_MODULUS) - 0x10001;
        }

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

    // ---- ReleaseSyncTimeManagers ----------------------------------------------------
    // Release whichever half of the clock sync is prepared, then drop back to "none".
    void
    TimeManager::ReleaseSyncTimeManagers()
    {
        switch (meSyncTimePrepareStatus)
        {
        case E_SYNC_TIME_PREPARE_STATUS_HOST:
            CGS_ASSERT(mSyncTimeHost.Release(), "mSyncTimeHost.Release()");
            break;

        case E_SYNC_TIME_PREPARE_STATUS_CLIENT:
            CGS_ASSERT(mSyncTimeClient.Release(), "mSyncTimeClient.Release()");
            break;

        default:
            break;
        }

        meSyncTimePrepareStatus = E_SYNC_TIME_PREPARE_STATUS_NONE;
    }

    // ---- StartSyncingTime -----------------------------------------------------------
    // Arm the per-frame sync and prepare the half that matches our role: the host
    // answers requests, everyone else estimates the host clock. The Prepare results
    // are not checked.
    void
    TimeManager::StartSyncingTime()
    {
        mbWeAreSyncingTime = true;

        const NetworkPlayerID lHostID = mpPlayerManager->GetHostPlayerID();
        if (mpPlayerManager->IsLocalPlayer(lHostID))
        {
            mSyncTimeHost.Prepare();
        }
        else
        {
            mSyncTimeClient.Prepare(lHostID);
        }
    }

    // ---- Disconnected ---------------------------------------------------------------
    // Lost the session: release everything and reset the sync message pool.
    void
    TimeManager::Disconnected()
    {
        CGS_ASSERT(Release(), "Release()");
        mSyncTimeMessageManager.Destruct();
    }

    // ---- IsTimeSynchronised ---------------------------------------------------------
    // The host's clock is the reference, so it is always in sync. A client is in sync
    // once its averaged clock difference is within 0.05 s over at least 8 replies.
    bool
    TimeManager::IsTimeSynchronised() const
    {
        switch (meSyncTimePrepareStatus)
        {
        case E_SYNC_TIME_PREPARE_STATUS_NONE:
            return false;

        case E_SYNC_TIME_PREPARE_STATUS_HOST:
            return true;

        case E_SYNC_TIME_PREPARE_STATUS_CLIENT:
            return mSyncTimeClient.TimeIsSynchronised(CgsSystem::Time(0.05f), 8);

        default:
            // The original streams the function name after this prefix.
            CGS_ASSERT(false, "SyncTimeManager not prepared in ");
            return false;
        }
    }

    // ---- OnHostMigration ------------------------------------------------------------
    // Re-prepare the sync halves under the new host. The old host id only reaches a
    // dropped dev-log line.
    void
    TimeManager::OnHostMigration(NetworkPlayerID /*lOldHostID*/, NetworkPlayerID lNewHostID, bool lbIAmHost)
    {
        CGS_ASSERT(miPlayersAdded > 0, "miPlayersAdded > 0");

        ReleaseSyncTimeManagers();
        PrepareSyncTimeManagers(lbIAmHost, lNewHostID);
    }

    // ---- SetStartFrame --------------------------------------------------------------
    // Bookmark the frame gameplay counts from: the current frame, the frame at which
    // the network clock read *lpStartTime (stepping back by the elapsed time over the
    // frame step), or no frame at all.
    void
    TimeManager::SetStartFrame(EStartFrame leStartFrame, const CgsSystem::Time* lpStartTime, f32 lfTimeStep)
    {
        if (leStartFrame == E_START_FRAME_CURRENT)
        {
            mu16StartFrame        = GetU16FrameCount();
            miStartFrameWrapCount = CalculateFrameWrapCount();
        }
        else if (leStartFrame == E_START_FRAME_PAST)
        {
            CGS_ASSERT(lpStartTime, "lpStartTime");
            CGS_ASSERT(lfTimeStep > 0.0f, "lfTimeStep > 0.0f");

            const CgsSystem::Time lElapsedTime    = mNetworkTime - *lpStartTime;
            const u32             luElapsedFrames = static_cast<u32>(static_cast<s64>(lElapsedTime.GetFloatVal() / lfTimeStep));
            const u32             luStartFrame    = muFrameCount - luElapsedFrames;

            CalculateFrameCountAndWrapCount(luStartFrame, &mu16StartFrame, &miStartFrameWrapCount);
        }
        else
        {
            CGS_ASSERT(leStartFrame == E_START_FRAME_INVALID, "leStartFrame == E_START_FRAME_INVALID");

            mu16StartFrame        = KU16_INVALID_FRAME;
            miStartFrameWrapCount = 0;
        }
    }

    // ---- CalculateFrameWrapCount ----------------------------------------------------
    s32
    TimeManager::CalculateFrameWrapCount() const
    {
        u16 lu16FrameCount;
        s32 liWrapCount;
        CalculateFrameCountAndWrapCount(muFrameCount, &lu16FrameCount, &liWrapCount);
        return liWrapCount;
    }

    // ---- CalculateFrameCountAndWrapCount --------------------------------------------
    // Split a running frame count into its 16-bit wire frame and the number of whole
    // 0xFFFF-frame wraps. A count that is negative as a signed value (a start frame
    // stepped back past frame zero) takes the second reduction, which rounds the wrap
    // count down rather than toward zero; GetFrameWrapCountSinceStart reduces the same way.
    void
    TimeManager::CalculateFrameCountAndWrapCount(u32 luFrameCount, u16* lpu16FrameCount, s32* lpiWrapCount) const
    {
        s32 liNumWraps;
        if (static_cast<s32>(luFrameCount) >= 0)
        {
            liNumWraps = static_cast<s32>(luFrameCount / KU_FRAME_WRAP_MODULUS);
        }
        else
        {
            liNumWraps = static_cast<s32>((luFrameCount - 1) / KU_FRAME_WRAP_MODULUS) - 0x10001;
        }

        *lpiWrapCount    = liNumWraps;
        *lpu16FrameCount = static_cast<u16>(luFrameCount % KU_FRAME_WRAP_MODULUS);
    }
} // namespace CgsNetwork
