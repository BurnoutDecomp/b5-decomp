// ===================================================================================
// CgsNetwork::TimeManager -- implementation
//   b5-decomp/src/GameShared/GameClasses/Network/Time/CgsTimeManager.cpp
//
// Reconstructed from the X360 ARTIST binary, shaped by the DecFIGS DWARF and the
// LegacySrc idiom. Recovered functions in this TU:
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
// trivial accessors and ResetNetworkTime are inline in the header; Destruct has no
// caller in the tree and stays declared only.
//
// The dev-log lines ("Set Start frame", "... called", "OnHostMigration: OldHost=...") go to
// the network log stream ServerInterfaceDirtySock::mNetStreamLogChannelOutput (ARTIST
// 0x82F335C8); the failing defaults stream their message through gpcMessageBuffer.
// ===================================================================================
#define CGS_ARTIST_FILE "..\\..\\..\\GameShared\\GameClasses\\Network\\Time\\CgsTimeManager.cpp"
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations

#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"   // PlayerManager
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"   // NetworkPlayer
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"           // CgsDev::StrStream (streamed asserts)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h" // mNetStreamLogChannelOutput

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
        ResetNetworkTime();
    }

    // ---- Prepare @0x82898FA0 --------------------------------------------------------
    // Per-session reset before any players join. Asserts no players are still added,
    // clears the sync state and clock, and re-arms the start-frame bookmark.
    bool
    TimeManager::Prepare()
    {
        ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "" << __FUNCTION__ << " called\n";
        CGS_ASSERT_AT(miPlayersAdded == 0,
            "miPlayersAdded == 0",
            CGS_ARTIST_FILE, 73 /* ARTIST 0x82899030 */);

        meSyncTimePrepareStatus = E_SYNC_TIME_PREPARE_STATUS_NONE;
        mbWeAreSyncingTime      = false;
        ResetNetworkTime();

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
            CGS_FAIL_STREAM_AT("SyncTimeManager invalid status " << meSyncTimePrepareStatus << " in "
                                   << __FUNCTION__ << "\n", CGS_ARTIST_FILE, 121);
            break;
        }
    }

    // ---- Release @0x82899090 --------------------------------------------------------
    // Tear down the sync managers and clear the clock, start-frame bookmark and counts.
    bool
    TimeManager::Release()
    {
        ReleaseSyncTimeManagers();

        ResetNetworkTime();

        miPlayersAdded        = 0;
        mbWeAreSyncingTime    = false;

        return true;
    }

    // ---- PrepareSyncTimeManagers @0x8288B420 ----------------------------------------
    // Switch the manager into host or client mode and prepare the matching sub-object.
    void
    TimeManager::PrepareSyncTimeManagers(bool lbIAmHost, NetworkPlayerID lNewHostID)
    {
        if (lbIAmHost)
        {
            meSyncTimePrepareStatus = E_SYNC_TIME_PREPARE_STATUS_HOST;
        }
        else
        {
            meSyncTimePrepareStatus = E_SYNC_TIME_PREPARE_STATUS_CLIENT;
        }

        switch (meSyncTimePrepareStatus)
        {
        case E_SYNC_TIME_PREPARE_STATUS_HOST:
            CGS_ASSERT_AT(mSyncTimeHost.Prepare(), "mSyncTimeHost.Prepare()", CGS_ARTIST_FILE, 609);
            break;

        case E_SYNC_TIME_PREPARE_STATUS_CLIENT:
            CGS_ASSERT_AT(mSyncTimeClient.Prepare(lNewHostID), "mSyncTimeClient.Prepare(lNewHostID)", CGS_ARTIST_FILE, 614);
            break;

        default:
            CGS_FAIL_STREAM_AT("Can't prepare state " << meSyncTimePrepareStatus << " in " << __FUNCTION__ << "\n",
                               CGS_ARTIST_FILE, 619);
            break;
        }
    }

    // ---- AddPlayer @0x82892E18 ------------------------------------------------------
    // A player joined: re-prepare the sync managers against the current host and, if the
    // joining player has a network player (a remote one), register its sync-time messages.
    void
    TimeManager::AddPlayer(NetworkPlayerID lPlayerID, bool lbIAmHost)
    {
        ReleaseSyncTimeManagers();
        PrepareSyncTimeManagers(lbIAmHost, mpPlayerManager->GetHostPlayerID());

        CGS_ASSERT_AT(mpPlayerManager != 0,
            "mpPlayerManager",
            CGS_ARTIST_FILE, 259 /* ARTIST 0x82892E70 */);

        NetworkPlayer* lpPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
        if (lpPlayer != 0)
        {
            mSyncTimeMessageManager.RegisterMessages(lpPlayer);
        }

        ++miPlayersAdded;
        CGS_ASSERT_AT(miPlayersAdded <= KI_MAX_PLAYERS,
            "miPlayersAdded <= KI_MAX_PLAYERS",
            CGS_ARTIST_FILE, 268 /* ARTIST 0x82892EFC */);
    }

    // ---- RemovePlayer @0x82892F10 ---------------------------------------------------
    // A player left: re-prepare the sync managers against the (possibly new) host, and if
    // the leaving player has a network player, unregister its sync-time messages.
    void
    TimeManager::RemovePlayer(NetworkPlayerID lPlayerID)
    {
        CGS_ASSERT_AT(mpPlayerManager != 0,
            "mpPlayerManager",
            CGS_ARTIST_FILE, 287 /* ARTIST 0x82892F4C */);
        CGS_ASSERT_AT(miPlayersAdded > 0,
            "miPlayersAdded > 0",
            CGS_ARTIST_FILE, 288 /* ARTIST 0x82892F74 */);

        --miPlayersAdded;
        ReleaseSyncTimeManagers();

        const NetworkPlayerID lHostID = mpPlayerManager->GetHostPlayerID();
        if (lHostID != lPlayerID && lHostID != MessageWithPlayerIDs::KI_INVALID_PLAYER_ID)
        {
            bool lbIAmHost = mpPlayerManager->IsLocalPlayer(lHostID);
            PrepareSyncTimeManagers(lbIAmHost, lHostID);
        }

        NetworkPlayer* lpPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
        if (lpPlayer != 0)
        {
            mSyncTimeMessageManager.UnregisterMessages(lpPlayer);
        }
    }

    // ---- GetU16FrameCountSinceStart @0x8288B2D0 -------------------------------------
    // The current frame number relative to the start-frame bookmark, in the 16-bit wire
    // window. Wraps modulo KU_FRAME_WRAP_MODULUS (0xFFFF, not 0x10000), so a borrow is
    // applied when the current frame is below the bookmark.
    u16
    TimeManager::GetU16FrameCountSinceStart() const
    {
        // DWARF cpp:401 lu16Result; PS3 0xBE7B1C reads the bookmark through GetU16StartFrame. A
        // current frame below the bookmark wraps by the 0xFFFF window (ARTIST 0x8288B324 adds 0xFFFF).
        u16 lu16Result = static_cast<u16>(GetU16FrameCount() - GetU16StartFrame());
        if (GetU16FrameCount() < GetU16StartFrame())
        {
            lu16Result += KU_FRAME_WRAP_MODULUS;
        }

        CGS_ASSERT_AT(lu16Result != KU16_INVALID_FRAME,
            "lu16Result != KU16_INVALID_FRAME",
            CGS_ARTIST_FILE, 410 /* ARTIST 0x8288B354 */);
        return lu16Result;
    }

    // ---- GetFrameWrapCountSinceStart @0x82893310 ------------------------------------
    // How many full 16-bit frame wraps have elapsed since the start-frame bookmark.
    s32
    TimeManager::GetFrameWrapCountSinceStart() const
    {
        // PS3 0xBE7BD4 (cpp:425..449): the wrap count now less the bookmark's, one fewer while the
        // current 16-bit frame is still below the bookmark. DWARF local cpp:439.
        s32 liFrameWrapsSinceInit = CalculateFrameWrapCount() - GetStartFrameWrapCount();
        if (GetU16FrameCount() < GetU16StartFrame())
        {
            --liFrameWrapsSinceInit;
        }

        return liFrameWrapsSinceInit;
    }

    // ---- GetFramesSinceStart @0x82893388 --------------------------------------------
    // The total (non-wrapped) frame count elapsed since the start-frame bookmark.
    s32
    TimeManager::GetFramesSinceStart() const
    {
        // DWARF: no locals; one expression (ARTIST 0x828933A8 widens the 16-bit count before the
        // second call).
        return GetU16FrameCountSinceStart() + GetFrameWrapCountSinceStart() * KU_FRAME_WRAP_MODULUS;
    }

    // GetU16FrameCount is a header inline (CgsTimeManager.h): ARTIST holds no copy.

    // ---- NextFrame @0x825813F0 ------------------------------------------------------
    // Advance both clocks by this frame's timer step and bump the running frame counter.
    // The step is the timer status' current time step (mfBaseTimeStep * mfTimeStepMultiplier,
    // asm: lfs 8(r4) * lfs 4(r4) into fp31, computed once and reused for both += Time temps).
    void
    TimeManager::NextFrame(const CgsSystem::TimerStatus* lpTimerStatus)
    {
        const f32 lfTimeStep = lpTimerStatus->GetCurrentTimeStep();

        // Implicit Time(f32) conversions: ARTIST passes the temporary's stack address, not the
        // constructor's returned this (0x82581420 / 0x82581438).
        mNetworkTime += lfTimeStep;                   // this+0x370
        mGlobalTime  += lfTimeStep;                   // this+0x378

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
            CGS_ASSERT_AT(mSyncTimeHost.Release(), "mSyncTimeHost.Release()", CGS_ARTIST_FILE, 565 /* ARTIST 0x8288B3F8 */);
            break;

        case E_SYNC_TIME_PREPARE_STATUS_CLIENT:
            CGS_ASSERT_AT(mSyncTimeClient.Release(), "mSyncTimeClient.Release()", CGS_ARTIST_FILE, 570 /* ARTIST 0x8288B3B8 */);
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
    // The original body sits in CgsTimeManager.h (its failing default names that file, line 248;
    // ARTIST keeps one out-of-line copy, 0x828803E0, called from StartTimeManager::
    // UpdateClientSyncTime); it stays here until the header move is measured against its caller.
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
            CGS_FAIL_STREAM_AT("SyncTimeManager not prepared in " << __FUNCTION__ << "\n",
                               "..\\..\\..\\GameShared\\GameClasses\\Network/Time/CgsTimeManager.h", 248);
            return false;
        }
    }

    // ---- OnHostMigration ------------------------------------------------------------
    // Log the migration, then re-prepare the sync halves under the new host.
    void
    TimeManager::OnHostMigration(NetworkPlayerID lOldHostID, NetworkPlayerID lNewHostID, bool lbIAmHost)
    {
        ServerInterfaceDirtySock::mNetStreamLogChannelOutput << __FUNCTION__ << ": OldHost=" << lOldHostID
            << " NewHost=" << lNewHostID << " lbIAmHost " << lbIAmHost << "\n";
        CGS_ASSERT_AT(miPlayersAdded > 0,
            "miPlayersAdded > 0",
            CGS_ARTIST_FILE, 230 /* ARTIST 0x82892DF0 */);

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
            ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "Set Start frame " << mu16StartFrame << "\n";
            miStartFrameWrapCount = CalculateFrameWrapCount();
            ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "Set Start frame wrap count " << miStartFrameWrapCount << "\n";
        }
        else if (leStartFrame == E_START_FRAME_PAST)
        {
            CGS_ASSERT_AT(lpStartTime,
                "lpStartTime",
                CGS_ARTIST_FILE, 363 /* ARTIST 0x8289316C */);
            CGS_ASSERT_AT(lfTimeStep > 0.0f,
                "lfTimeStep > 0.0f",
                CGS_ARTIST_FILE, 364 /* ARTIST 0x82893198 */);

            const CgsSystem::Time lElapsedTime    = mNetworkTime - *lpStartTime;
            const u32             luElapsedFrames = static_cast<u32>(lElapsedTime.GetFloatVal() / lfTimeStep);
            const u32             luStartFrame    = muFrameCount - luElapsedFrames;

            CalculateFrameCountAndWrapCount(luStartFrame, &mu16StartFrame, &miStartFrameWrapCount);
            ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "Set Start frame " << mu16StartFrame << "\n";
            ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "Set Start frame wrap count " << miStartFrameWrapCount << "\n";
        }
        else
        {
            CGS_ASSERT_AT(leStartFrame == E_START_FRAME_INVALID,
                "leStartFrame == E_START_FRAME_INVALID",
                CGS_ARTIST_FILE, 381 /* ARTIST 0x828932C0 */);

            mu16StartFrame        = KU16_INVALID_FRAME;
            ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "Cleared Start frame\n";
            miStartFrameWrapCount = 0;
            ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "Cleared Start frame wrap count\n";
        }
    }

    // ---- GetU16StartFrame / GetStartFrameWrapCount (DWARF cpp:463 / :478) -----------
    // The bookmark's two halves; ARTIST holds no copy of either (inlined at every caller).
    u16
    TimeManager::GetU16StartFrame() const
    {
        return mu16StartFrame;
    }

    s32
    TimeManager::GetStartFrameWrapCount() const
    {
        return miStartFrameWrapCount;
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
        // The top-bit test is unsigned: ARTIST compares the count with 0x80000000 (cmplw,
        // GetFrameWrapCountSinceStart 0x82893318) and falls through into the rounding-down arm.
        s32 liNumWraps;
        if (luFrameCount >= 0x80000000u)
        {
            liNumWraps = static_cast<s32>((luFrameCount - 1) / KU_FRAME_WRAP_MODULUS) - 0x10001;
        }
        else
        {
            liNumWraps = static_cast<s32>(luFrameCount / KU_FRAME_WRAP_MODULUS);
        }

        *lpiWrapCount    = liNumWraps;
        *lpu16FrameCount = static_cast<u16>(luFrameCount % KU_FRAME_WRAP_MODULUS);
    }
} // namespace CgsNetwork
