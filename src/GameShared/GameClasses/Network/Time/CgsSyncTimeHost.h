#pragma once

// ===================================================================================
// CgsNetwork::SyncTimeHost -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Time/CgsSyncTimeHost.h
//
// The host-side driver of the clock-synchronisation round trip. It owns no slots of its
// own; it borrows the session's SyncTimeMessageManager (the shared per-player sync-time
// message pool) and the PlayerManager, and each Update() it drains every received
// sync-time message addressed to the local host id, stamps the current host time, and
// queues the reply back to the originating client.
//
// SHAPE authoritative from the DecFIGS DWARF (CgsSyncTimeHost.h:40), gated against the
// X360 ARTIST binary:
//
//   mpSyncTimeMessageManager  -- the shared host/client sync-time message pool (+0x0)
//   mpPlayerManager           -- the session player registry              (+0x4)
//
// Construct (@0x8287D380) stores the message manager at +0x0 and the player manager at
// +0x4 (asm: `*this = lpMessageManager; this[1] = lpPlayerManager`), matching the DWARF
// member order. The Update/Prepare/Release methods read those two members back by name.
//
// WIDTH MODELLING: every member is accessed BY NAME; the C++ struct is laid out natively
// (semantic parity, not byte-exact X360 layout -- the rule the rest of the project
// follows, cf. CgsSyncTimeBase.h). All methods are plain (no C++ `virtual`).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessageWithPlayerIDs.h"

namespace CgsNetwork
{
    // Pointer-only collaborators on this class's API; incomplete types suffice and avoid
    // dragging the player/message hierarchies into every includer.
    struct PlayerManager;
    struct SyncTimeMessageManager;

    struct SyncTimeHost
    {
        // The shared signed player-index typedef (mirrors MessageWithPlayerIDs).
        typedef MessageWithPlayerIDs::NetworkPlayerID NetworkPlayerID;

        void Construct(PlayerManager* lpPlayerManager, SyncTimeMessageManager* lpMessageManager);
        void Destruct();

        bool Prepare();
        bool Release();

        void Update(u16 lu16FrameNumber, CgsSystem::Time& lTime);

    private:
        // --- layout (shape + order from the DWARF member list) -----------------------
        SyncTimeMessageManager* mpSyncTimeMessageManager;  // +0x0
        PlayerManager*          mpPlayerManager;           // +0x4
    };
} // namespace CgsNetwork
