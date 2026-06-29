#pragma once

// ===================================================================================
// CgsNetwork::SyncTimeClient -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Time/CgsSyncTimeClient.h
//
// The client-side half of the host/client clock-synchronisation handshake. A non-host
// peer drives one of these to estimate the host clock: it pulls each freshly received
// SyncTimeMessage out of its SyncTimeMessageManager, measures the message's round-trip
// delay and the host/client time difference, keeps a small rolling window of both, and
// (every few frames) prepares a fresh sync-time message to send back to the host.
//
// SHAPE authoritative from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../CgsSyncTimeClient.h:38), gated against the X360
// ARTIST binary (Construct @0x8287D1C0, Prepare @0x8288ADA0, Update @0x82892378,
// Release @0x8288AF48, TimeIsSynchronised @0x8287D240):
//
//   mpSyncTimeMessageManager  (+0x00)  -- the message pool this client pumps
//   mpPlayerManager           (+0x04)  -- session player registry (host/local lookup)
//   mHostPlayerID             (+0x08)  -- the host we are synchronising to (-1 == none)
//   mafDelays[4]              (+0x0C)  -- rolling window of measured round-trip delays
//   mafTimeDifference[4]      (+0x1C)  -- rolling window of host/client time differences
//   miNumTimePackets          (+0x2C)  -- total sync packets processed since Prepare
//   mu8NumTimePacketsStored   (+0x30)  -- valid entries currently in mafDelays
//   mu8NumTimeDifferenceStored(+0x31)  -- valid entries currently in mafTimeDifference
//   mu16LastSyncSentFrame     (+0x32)  -- frame the last outgoing sync was sent (0xFFFF)
//   mu8DelayHead              (+0x34)  -- next write slot in mafDelays
//   mu8TimeDifferenceHead     (+0x35)  -- next write slot in mafTimeDifference
//
// WIDTH MODELLING: every member is accessed BY NAME; the C++ struct is laid out natively
// (semantic parity, not byte-exact X360 layout -- the same rule the rest of the project
// follows, cf. CgsSyncTimeBase.h / CgsNetworkPlayer.h). The DWARF/X360 byte offsets above
// are documentation only. The X360 build has no C++ `virtual` on these methods.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessageWithPlayerIDs.h"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"

namespace CgsNetwork
{
    // Pointer-only dependencies -- forward-declare to avoid pulling the whole player /
    // message-manager hierarchy into every includer.
    struct PlayerManager;
    struct SyncTimeMessageManager;

    struct SyncTimeClient
    {
        // The rolling-window depths (DWARF CgsSyncTimeClient.h:41/42).
        static const s32 KI_NUM_DELAYS_STORE     = 4;
        static const s32 KI_NUM_TIME_DIFFS_STORE = 4;

        // The shared signed player-index typedef (mirrors MessageWithPlayerIDs).
        typedef MessageWithPlayerIDs::NetworkPlayerID NetworkPlayerID;

        // The "no player" sentinel mHostPlayerID carries when unprepared.
        static const NetworkPlayerID KI_INVALID_PLAYER_ID = -1;

        void Construct(PlayerManager* lpPlayerManager, SyncTimeMessageManager* lpMessageManager);
        void Destruct();

        bool Prepare(NetworkPlayerID lHostPlayerID);
        bool Release();

        void Update(u16 lu16FrameNum, CgsSystem::Time* lpNetworkTime, const CgsSystem::Time* lpGlobalTime);

        bool TimeIsSynchronised(CgsSystem::Time lTimeDiffTolerance, s32 liMinNumPacketsRecvd) const;

    private:
        SyncTimeMessageManager* mpSyncTimeMessageManager;   // +0x00
        PlayerManager*          mpPlayerManager;            // +0x04
        NetworkPlayerID         mHostPlayerID;              // +0x08

        f32 mafDelays[KI_NUM_DELAYS_STORE];                 // +0x0C
        f32 mafTimeDifference[KI_NUM_TIME_DIFFS_STORE];     // +0x1C

        s32 miNumTimePackets;                               // +0x2C
        u8  mu8NumTimePacketsStored;                        // +0x30
        u8  mu8NumTimeDifferenceStored;                     // +0x31
        u16 mu16LastSyncSentFrame;                          // +0x32
        u8  mu8DelayHead;                                   // +0x34
        u8  mu8TimeDifferenceHead;                          // +0x35
    };
} // namespace CgsNetwork
