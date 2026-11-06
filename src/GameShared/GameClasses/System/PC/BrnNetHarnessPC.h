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
// Once this instance has been in an online game and leaves it (for any reason), the harness never
// creates or joins again: a leave test must not rejoin behind its own back.
//
// Scheduled injection, on the IN-GAME clock (seconds since this instance was first observed in an
// online game; nothing fires before that). Neither needs BRN_NET_HOST / BRN_NET_JOIN, so a run that
// joined through the real menus can use them too:
//
//   BRN_NET_LEAVE_AT=s   post GUI event 52 (leave the online game) -- the record the game room's
//                        leave overlay posts on Accept -- s seconds in, straight into the network
//                        post-simulation GUI queue. Re-posted (at most 3 times, 10 s apart) while
//                        the instance is still in the game.
//   BRN_NET_SCRIPT=list  post arbitrary GUI records. list = entries separated by ';', each
//                          <seconds>:<gui|net>:<event id>[:<word>/<word>/...]
//                        gui -> the GUI module's out queue, right before BridgeGuiToGameState. Only
//                               the game state sees it: the network side appends the GUI OUTPUT
//                               BUFFER's queue, a different queue (measured: GUI 54 posted here never
//                               reached StateManager::ProcessGuiEvents). Needs InjectGuiEvents.
//                        net -> the network post-simulation GUI queue only (the Update hook). A
//                               record for the network StateManager goes here.
//                        out -> the GUI output buffer's out queue, right before
//                               TranslateGuiEventsToNetworkEvents, which reads only that queue and
//                               turns e.g. GUI 281 into NetworkInLoadingScreenShown.
//                        A word is decimal or 0x-hex (32 bits); a 'q' prefix makes it 64 bits and
//                        puts the payload at record offset 16 (an 8-aligned payload), otherwise
//                        at 12. The record is the GuiEventWrapper form {size, id, offset, payload}
//                        queued on channel 40. Example (GUI 573 challenge select, selector 0,
//                        style 1): "20:gui:573:q0x0123456789ABCDEF/0/1".
//                        No ',' '=' or whitespace inside (flow_run's -DiagEnv splits on them).
//   BRN_NET_TEAM=<seconds>:<team>
//                        put the local player on that team (1 red, 2 blue), through the calls GUI 265
//                        (switch team) makes. That event only swaps red and blue, and nothing on
//                        this build puts a lobby player on a first team, so road rage's launch
//                        check ("exactly two teams") cannot pass unattended without this.
//   BRN_NET_START_MODE=<seconds>:<game mode>
//                        post GUI event 257 (GuiEventNetworkGameParams), the record the online game
//                        options screen posts: one round in that mode, on the first preset online
//                        event that has landmarks. Its 480-byte payload does not fit BRN_NET_SCRIPT.
//                        Waits for the GUI cache's preset list to fill. The launch itself is a
//                        BRN_NET_SCRIPT entry: GUI 54 (launch) or 56 (force launch).
//
// Witness(): bounded "[net] <site> ..." log lines at the network chokepoints (sign-in, create,
// join, player list). WitnessTag(): the same with another tag ("[nettraf] ...", "[netui] ...").
// They print only when BP_LAN=1 or a harness role is set, so the default offline run logs nothing
// new. Each site prints at most KI_WITNESS_LINES_PER_SITE lines.
// ============================================================================

namespace CgsModule
{
    template <s32 BUFSIZE, s32 ALIGN> class VariableEventQueue;
}
namespace BrnNetwork
{
    class BrnNetworkManager;
}
namespace BrnGui
{
    class GuiCache;
}

namespace BrnNetHarnessPC
{
    const s32 KI_WITNESS_LINES_PER_SITE     = 24;
    const s32 KI_WITNESS_TAG_LINES_PER_SITE = 48;   // WitnessTag sites (the per-update traffic families)

    // Hook 1, in BrnGameModule::DoUpdate_NetworkPostSim: under the post-simulation input
    // buffer's write lock, right after the GUI out-events are appended to its GUI queue. Posts
    // the next harness event (if any) into that queue; the network StateManager reads it in the
    // same update.
    void Update(CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue);

    // Hook 2, in BrnNetworkModule::ProcessAfterSimulation: right after the network manager ran
    // (the module is updating, so its manager may be read). Records sign-in / idle / in-game
    // state for the next Update().
    void Observe(BrnNetwork::BrnNetworkManager* lpNetworkManager);

    // Hook 3, in BrnGameModule's game-state post-world pass: right before BridgeGuiToGameState
    // reads the GUI out queue. Posts the due BRN_NET_SCRIPT "gui" records into it.
    void InjectGuiEvents(CgsModule::VariableEventQueue<18432, 16>* lpGuiOutQueue);

    // Hook 4, right after hook 1 and into the same queue. Posts the due BRN_NET_START_MODE record.
    void InjectStartMode(CgsModule::VariableEventQueue<18432, 16>* lpGuiOutQueue, const BrnGui::GuiCache* lpGuiCache);

    // Hook 5, right before TranslateGuiEventsToNetworkEvents. Posts the due "out" script records.
    void InjectGuiOutputBuffer(CgsModule::VariableEventQueue<18432, 16>* lpGuiOutputBufferQueue);

    // True when witness lines print (BP_LAN=1 or a harness role).
    bool WitnessEnabled();

    // "[net] <lpcSite> <printf text>\n", bounded per site.
    void Witness(const char* lpcSite, const char* lpcFormat, ...);

    // "[<lpcTag>] <lpcSite> <printf text>\n", bounded per tag and site.
    void WitnessTag(const char* lpcTag, const char* lpcSite, const char* lpcFormat, ...);

    // The GUI player-list record (up to 8 entries of { s32 player id, char name[16] }, 20-byte
    // stride): one line each time the id/name set changes, bounded like Witness().
    void WitnessPlayerList(const void* lpRecord, s32 liNumPlayers, s32 liTotalPlayers);
}
