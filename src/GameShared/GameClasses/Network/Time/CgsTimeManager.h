#pragma once

// ===================================================================================
// CgsNetwork::TimeManager -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Time/CgsTimeManager.h
//
// The session-wide network time/frame manager. It owns the three clock-sync sub-objects
// (the client estimator, the host responder and the shared sync-time message pool), the
// network/global Time clocks, the running network frame counter, and the per-session
// "start frame" bookmark the gameplay code converts frame numbers against.
//
// SHAPE authoritative from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../CgsTimeManager.h:42), gated against the X360 ARTIST
// binary. Member ORDER and the X360 byte offsets (documentation only) are:
//
//   mSyncTimeClient          (+0x000)  -- client-side clock estimator
//   mSyncTimeHost            (+0x038)  -- host-side clock responder
//   mSyncTimeMessageManager  (+0x040)  -- shared per-player sync-time message pool
//   mNetworkTime             (+0x370)  -- estimated network (host) clock
//   mGlobalTime              (+0x378)  -- local global clock the sync is measured against
//   meSyncTimePrepareStatus  (+0x380)  -- NONE / HOST / CLIENT
//   mbWeAreSyncingTime       (+0x384)  -- gate: only Update the sync managers when set
//   muFrameCount             (+0x388)  -- running network frame counter
//   mu16StartFrame           (+0x38C)  -- the 16-bit start-frame bookmark (0xFFFF == none)
//   miStartFrameWrapCount    (+0x390)  -- wrap count at the start-frame bookmark
//   mpPlayerManager          (+0x394)  -- the session player registry
//   miPlayersAdded           (+0x398)  -- number of AddPlayer()/RemovePlayer() balance
//
// WIDTH MODELLING: every member is accessed BY NAME; the C++ struct is laid out natively
// (semantic parity, not byte-exact X360 layout -- the same rule the rest of the project
// follows, cf. CgsSyncTimeBase.h / CgsSyncTimeClient.h). The X360 byte offsets above are
// documentation only. The X360 build has no C++ `virtual` on these methods.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Time/CgsSyncTimeClient.h"
#include "GameShared/GameClasses/Network/Time/CgsSyncTimeHost.h"
#include "GameShared/GameClasses/Network/Time/CgsSyncTimeBase.h"      // SyncTimeMessageManager
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessageWithPlayerIDs.h"  // NetworkPlayerID
#include "GameShared/GameClasses/System/Timer/CgsTime.h"             // CgsSystem::Time

namespace CgsNetwork
{
    // Pointer-only collaborator; an incomplete type suffices and avoids pulling the whole
    // player registry into every includer.
    struct PlayerManager;
    struct TimerStatus;

    struct TimeManager
    {
        // The shared signed player-index typedef (mirrors MessageWithPlayerIDs).
        typedef MessageWithPlayerIDs::NetworkPlayerID NetworkPlayerID;

        // The maximum number of players this manager balances (AddPlayer asserts
        // `miPlayersAdded <= KI_MAX_PLAYERS`; the X360 compares the post-increment count
        // against 8, i.e. it must stay <= 8).
        static const s32 KI_MAX_PLAYERS = 8;

        // The "no player" / "no start frame" 16-bit sentinel mu16StartFrame carries when
        // the start frame has not been set (asm stores -1 sign-extended into the u16 ==
        // 0xFFFF, and GetU16FrameCountSinceStart asserts its result is not this value).
        static const u16 KU16_INVALID_FRAME = 0xFFFF;

        // The 16-bit network frame-wrap modulus. The wire protocol counts frames modulo
        // 0xFFFF (not 0x10000), so a "u16 frame" is always `frame % 0xFFFF`.
        static const u32 KU_FRAME_WRAP_MODULUS = 0xFFFF;

        // Where in the sync handshake this manager currently is. DWARF CgsTimeManager.h:140.
        enum ESyncTimePrepareStatus
        {
            E_SYNC_TIME_PREPARE_STATUS_NONE   = 0,
            E_SYNC_TIME_PREPARE_STATUS_HOST   = 1,
            E_SYNC_TIME_PREPARE_STATUS_CLIENT = 2,
        };

        // How a freshly-set start frame relates to the current frame. DWARF CgsTimeManager.h:102.
        enum EStartFrame
        {
            E_START_FRAME_CURRENT = 0,
            E_START_FRAME_PAST    = 1,
            E_START_FRAME_INVALID = 2,
        };

        // --- lifecycle ---
        void Construct(PlayerManager* lpPlayerManager);
        bool Prepare();
        void Update();
        bool Release();
        void Destruct();

        // --- clock / sync control ---
        void ResetNetworkTime();
        void StartSyncingTime();
        void StopSyncingTime();
        CgsSystem::Time GetNetworkTime() const;
        bool IsTimeSynchronised() const;

        // --- session membership ---
        void OnHostMigration(NetworkPlayerID lOldHostID, NetworkPlayerID lNewHostID, bool lbIAmHost);
        void AddPlayer(NetworkPlayerID lPlayerID, bool lbIAmHost);
        void RemovePlayer(NetworkPlayerID lPlayerID);
        void Disconnected();

        // --- start-frame bookmark / frame queries ---
        void SetStartFrame(EStartFrame leStartFrame, const CgsSystem::Time* lpStartTime, f32 lfTimeStep);
        bool HasStartFrameBeenSetValid();
        void NextFrame(const TimerStatus* lpTimerStatus);
        u32  GetFrameCount() const;
        u16  GetU16FrameCount() const;
        u16  GetU16FrameCountSinceStart() const;
        s32  GetFrameWrapCountSinceStart() const;
        s32  GetFramesSinceStart() const;

        // Consumer-facing alias kept for the reconstructed callers that read the running
        // network frame number wrapped into the 16-bit wire window
        // (BrnNetworkSelectedRoutesManager / BrnNetworkAggressiveDrivingManager). Same
        // result as GetU16FrameCount(); retained so those TUs keep compiling unchanged.
        u16 GetCurrentFrameNumber() const { return GetU16FrameCount(); }

    private:
        void ReleaseSyncTimeManagers();
        void PrepareSyncTimeManagers(bool lbIAmHost, NetworkPlayerID lNewHostID);
        u16  GetU16StartFrame() const;
        s32  GetStartFrameWrapCount() const;
        s32  CalculateFrameWrapCount() const;
        void CalculateFrameCountAndWrapCount(u32 luFrameCount, u16* lpu16FrameCount, s32* lpiWrapCount) const;

        // --- layout (shape + order from the DWARF member list) -----------------------
        SyncTimeClient          mSyncTimeClient;          // +0x000
        SyncTimeHost            mSyncTimeHost;            // +0x038
        SyncTimeMessageManager  mSyncTimeMessageManager;  // +0x040
        CgsSystem::Time         mNetworkTime;             // +0x370
        CgsSystem::Time         mGlobalTime;              // +0x378
        ESyncTimePrepareStatus  meSyncTimePrepareStatus;  // +0x380
        bool                    mbWeAreSyncingTime;       // +0x384
        u32                     muFrameCount;             // +0x388
        u16                     mu16StartFrame;           // +0x38C
        s32                     miStartFrameWrapCount;    // +0x390
        PlayerManager*          mpPlayerManager;          // +0x394
        s32                     miPlayersAdded;           // +0x398
    };
} // namespace CgsNetwork
