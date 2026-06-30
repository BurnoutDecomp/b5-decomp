#include "types.hpp"

#include "GameSource/Network/BrnNetworkManager.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"  // CgsNetwork::PackOrUnpackInt field primitive

// ============================================================================================
// Reconstructed from BURNOUT_X360_ARTIST.XEX -- the top-level online network manager TU.
// Ledger key: GameSource/Unity/../Network/BrnNetworkManager.cpp  (collapses to
//             GameSource/Network/BrnNetworkManager.cpp).
//
// The TU's postmortem dossier enumerates 23 functions (the session/online lifecycle spine,
// per-frame dispatch, the staged Prepare/Release machines, the player/stats queries, the
// telemetry forwarder, and three static callbacks). All 23 method declarations are homed in
// BrnNetworkManager.h (ADDITIVE GROW); their signatures are reconstructed from the X360 asm.
//
// RECONSTRUCTION STATUS
// ---------------------
//   * BODIED (1):  BrnNetworkManager::PackOrUnpack
//         The only function whose X360 body does NOT touch the manager's own object layout.
//         It (de)serialises a single NetworkPlayerID field through the shared quantised-int
//         message primitive (sub_82881370 == CgsNetwork::PackOrUnpackInt, committed in
//         CgsMessage.h) over the full s32 range. Faithfully reconstructed below.
//
//   * DECLARATION-ONLY (22):  every other function.
//         Their X360 bodies operate entirely over the full ~613 KB BrnNetworkManager aggregate
//         -- 28+ embedded manager sub-objects (TeamSelection / AutoLogin / EventScores /
//         GamerCard / ChallengeSuccess / RoadRules / State / MarkedMan / SelectedRoutes /
//         Invite / GamerPicture / Image / DirtyTrick / LiveRevenge / AggressiveDriving /
//         PlayerStats / Buddy / TextureCompress / Camera / Traffic / Standings / PostRound /
//         Suspension / MatchMaking / Connection / Launch / Login ...) plus dozens of scalar
//         fields reached by raw byte offset (e.g. session time/round at +0x95D70..+0x95DAF,
//         the GUI-stats cursor at +0x95D70/+613744, the texture header at +613756). That full
//         layout is GENUINELY UN-HOMED -- the committed BrnNetworkManager.h is an explicit
//         minimal slice (3 materialised members). Per the anti-fabrication rule, fabricating
//         that ~613 KB layout (or raw-offset-poking it) is forbidden, so these bodies are
//         deferred until the full aggregate is homed. Each is FLAGGED at its declaration in
//         the header.
//
//         Additionally several of these reach still-[todo] embedded-manager methods and/or a
//         multi-stage pipeline (Prepare/Release's EPrepareStage/EReleaseStage walks), which
//         independently require declaration-only treatment.
// ============================================================================================

namespace BrnNetwork
{
    // ----------------------------------------------------------------------------------------
    // BrnNetwork::BrnNetworkManager::PackOrUnpack  @ 0x82582128  [EXECUTED in goal trace]
    //
    // Static field (de)serialise helper for a NetworkPlayerID carried in a reliable message:
    // copy the field into a local, route it through the shared quantised-int primitive over
    // the full signed-32-bit range, write the (possibly updated) value back, and return the
    // per-field pack/unpack status (0 == success). The X360 call site passes only the message
    // and the field pointer (no manager `this`), so this is a static helper.
    //
    //   asm: r5 = 0x80000000 (INT_MIN), r6 = 0x7FFFFFFF (INT_MAX); the field word is staged
    //        on the stack (var_20), passed by pointer to sub_82881370, then stored back.
    // ----------------------------------------------------------------------------------------
    BrnNetworkManager::PackOrUnpackResult BrnNetworkManager::PackOrUnpack(
            CgsNetwork::Message* lpMessage,
            NetworkPlayerID*     lpNetworkPlayerID )
    {
        s32 liValue = *lpNetworkPlayerID;
        const BrnNetworkManager::PackOrUnpackResult lxResult =
            CgsNetwork::PackOrUnpackInt( lpMessage, &liValue, 0x80000000, 0x7FFFFFFF );
        *lpNetworkPlayerID = liValue;
        return lxResult;
    }

    // ----------------------------------------------------------------------------------------
    // The remaining 22 functions are DECLARATION-ONLY (see file header). Reconstructing their
    // bodies requires the full BrnNetworkManager aggregate layout, which is un-homed; per the
    // anti-fabrication rule their bodies are intentionally omitted here rather than fabricated.
    //
    //   Destruct, Prepare, Release,
    //   ProcessBeforeSimulation, ProcessHLUpdateFlags, ProcessNetworkTextureDecodeEvent,
    //   OnEnterGame, OnLeaveGame, OnGameLaunching, JoinGameSession,
    //   IsLocalPlayer, OutputPlayerResultsInfo, OutputPlayerStatusInfo, OutputPlayerStatsToGui,
    //   CaptureTelemetryEvent, TriggerEventFromLogin, TriggerEventFromServerInterface,
    //   GetLocalConsoleFrameRate, GetTime,
    //   SyncTimeClientReadyCallback, DxtDecodeCallback, PlayerManagerEventCallback.
    //
    // GetMaxMessageSize @0x825885E8 is ALSO declaration-only: although it has NO manager-`this`
    // dependency, its body stages a 41-entry {s32 miMessageSize; bool mbReliable} MessageDef
    // table from pure immediates then runs a rw::math::vpu::Max<s32> reduction
    // (BrnNetworkManager.cpp:1839..1897). Deferred rather than hand-transcribed to avoid
    // fabricating the 41 immediate rows -- bodies land with a careful per-row asm pass.
    // ----------------------------------------------------------------------------------------
}
