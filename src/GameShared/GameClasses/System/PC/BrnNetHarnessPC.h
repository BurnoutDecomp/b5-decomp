#pragma once

#include "types.hpp"

// ============================================================================
// BrnNetHarnessPC.h -- the two-instance LAN test harness and its [net] witness lines.
//
// [PC HARNESS, NOT X360] Nothing here exists on the console. It lets the unattended test harness
// (tools\tests\run_pair.ps1) drive two game instances into one online game without a pad:
//
//   BRN_NET_HOST=1   this instance hosts: sign in (GUI event 272), then create an unranked free
//                    burn lobby game (GUI event 256: game mode 15, security 2, ranked off).
//   BRN_NET_JOIN=1   this instance joins: sign in (272), then quick match (GUI event 251: unranked,
//                    free burn), retried until it lands in a game.
//   BRN_NET_DELAY=s  seconds after the first network update before the sign-in is posted
//                    (default 30, so the boot reaches free roam first).
//
// The events are the ones the front end posts: they go into the network module's input GUI queue
// on channel 40, exactly as the GUI's own OutputGuiEvent records arrive, and the network
// StateManager handles them unchanged. Neither variable set: Update() returns at once.
//
// Witness(): bounded "[net] <site> ..." log lines at the network chokepoints (sign-in, create,
// join, player list). They print only when BP_LAN=1 or a harness role is set, so the default
// offline run logs nothing new. Each site prints at most KI_WITNESS_LINES_PER_SITE lines.
// ============================================================================

namespace CgsModule
{
    template <s32 BUFSIZE, s32 ALIGN> class VariableEventQueue;
}
namespace BrnNetwork
{
    class BrnNetworkManager;
}

namespace BrnNetHarnessPC
{
    const s32 KI_WITNESS_LINES_PER_SITE = 24;

    // Hook 1, in BrnGameModule::DoUpdate_NetworkPostSim: under the post-simulation input
    // buffer's write lock, right after the GUI out-events are appended to its GUI queue. Posts
    // the next harness event (if any) into that queue; the network StateManager reads it in the
    // same update.
    void Update(CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue);

    // Hook 2, in BrnNetworkModule::ProcessAfterSimulation: right after the network manager ran
    // (the module is updating, so its manager may be read). Records sign-in / idle / in-game
    // state for the next Update().
    void Observe(BrnNetwork::BrnNetworkManager* lpNetworkManager);

    // True when witness lines print (BP_LAN=1 or a harness role).
    bool WitnessEnabled();

    // "[net] <lpcSite> <printf text>\n", bounded per site.
    void Witness(const char* lpcSite, const char* lpcFormat, ...);

    // The GUI player-list record (up to 8 entries of { s32 player id, char name[16] }, 20-byte
    // stride): one line each time the id/name set changes, bounded like Witness().
    void WitnessPlayerList(const void* lpRecord, s32 liNumPlayers, s32 liTotalPlayers);
}
