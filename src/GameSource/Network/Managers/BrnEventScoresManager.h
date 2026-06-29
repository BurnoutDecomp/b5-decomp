#pragma once

// ===================================================================================
// BrnNetwork::LocalEventScoreUploadData -- owning header
//   b5-decomp/src/GameSource/Network/Managers/BrnEventScoresManager.h
//
// One pending event-score upload record. The profile keeps a fixed
// Array<LocalEventScoreUploadData, 49> of these (the X360 stores the array at Profile +0x7B8;
// BrnNetwork::EventScoresManager and BrnProgression::Profile add/erase entries via the
// generic Array<T,N>::Append/Erase/EraseFast bodies).
//
// LAYOUT (16-byte record; X360-AUTHORITATIVE -- every Array<...,49> body strides 0x10 and the
// container's live-count word lands at +0x310 == 49 * 0x10; the Array::Append copy is two
// 64-bit `std` over the whole record):
//   +0x00 (8)  mu64EventID    the event's id
//   +0x08 (4)  muScore        the player's score for the event
//   +0x0C (4)  meGameMode     game mode (BrnGameState::GameStateModuleIO::EGameModeType;
//                             a committed s32 enum -- modelled here as its s32 underlying
//                             type so this stays a plain aggregate with no operator==, which
//                             the X360 never emits for this element. FLAG: re-type to the
//                             committed EGameModeType when its home can be included here
//                             without a cycle; the 4-byte size/offset must not change.)
//
// A plain aggregate (no user-declared copy-assignment), so its element copy is the
// compiler-generated 16-byte member-wise move -- matching the two-`std` block move the X360
// Array<...,49>::Append/Erase/EraseFast emit.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"          // Array<T,N> maPendingUploads
#include "GameShared/GameClasses/System/Timer/CgsTime.h"         // CgsSystem::Time mUploadRetryTimer
#include "GameSource/Network/Debug Components/BrnNetworkEventScoresManagerDebugComponent.h" // embedded mDebugComponent

namespace CgsNetwork { struct ServerInterfaceDirtySock; }

// VariableEventQueue<14000,16> is the network-event queue type ProcessNetworkEvents walks; only
// a pointer is named in this header, so forward-declare the class template (full def in
// CgsVariableEventQueue.h, included by the .cpp).
namespace CgsModule { template <s32 BUFSIZE, s32 ALIGN> class VariableEventQueue; }

namespace BrnNetwork
{
    // The post-sim module-IO input buffer ProcessAfterSimulation reads its queue from
    // (pointer-only here; full def in BrnNetworkModuleIO.h, included by the .cpp).
    namespace BrnNetworkModuleIO { class PostSimulationInputBuffer; }

    struct LocalEventScoreUploadData
    {
        u64 mu64EventID;   // +0x00
        u32 muScore;       // +0x08
        s32 meGameMode;    // +0x0C  (EGameModeType underlying s32)
    };

    // Pointer-only dependencies of the manager (full defs in their own headers; included by
    // the .cpp where the bodies actually call through them).
    class BrnServerInterfaceBase;   // mpServerInterface points at the Brn server interface
    class BrnNetworkModule;         // mpNetworkModule -- owning module, supplies the event queues

    // ===============================================================================
    // BrnNetwork::EventScoresManager
    //   The online event-scores upload manager. It accumulates per-event scores the player
    //   posts during a session into a fixed Array<LocalEventScoreUploadData,49>, then -- once
    //   logged in and the custom-commands component is idle -- batches them into one
    //   EventScoreData and pushes it to the server via ServerInterfaceCustomCommands::
    //   UploadEventScoreData, retrying on a timed interval. A small state machine
    //   (meState: 0 idle / 1 logged-in-trigger / 2 uploading / 3 ...) is pumped from
    //   ProcessBeforeSimulation; the inbound network-event queue is drained in
    //   ProcessAfterSimulation -> ProcessNetworkEvents.
    //
    //   LAYOUT (X360-AUTHORITATIVE; offsets from this, read off Construct @ 0x82556AB0 and the
    //   member TU functions):
    //     +0x000  maPendingUploads   Array<LocalEventScoreUploadData,49>  (count word @ +0x310)
    //     +0x314  (reserved -- not touched by any reconstructed function)
    //     +0x318  mUploadRetryTimer  CgsSystem::Time   (SetFloatVal(this+0x318), miSeconds @ +0x318)
    //     +0x320  miUploadCount      s32               (per-batch element counter; stw 0 in Construct)
    //     +0x324  meState            s32               (the upload state machine; stw 0 in Construct)
    //     +0x328  mpNetworkModule    BrnNetworkModule* (a2 of Construct; asserted non-null)
    //     +0x32C  mpServerInterface  BrnServerInterfaceBase* (a3 of Construct; asserted non-null)
    //     +0x330  mDebugComponent    EventScoresManagerDebugComponent (Construct(this+0x330,this))
    // ===============================================================================
    class EventScoresManager
    {
    public:
        // Two-phase init (X360 @ 0x82556AB0): stash the owning module + server interface, clear
        // the pending list, zero the counter/state and the retry timer, and construct the embedded
        // debug component.
        void Construct( BrnNetworkModule* lpNetworkModule, BrnServerInterfaceBase* lpServerInterface );

        // Teardown (X360 @ 0x82556B58): zero everything and destruct the debug component.
        void Destruct();

        // --- per-frame pumps (called by BrnNetworkManager) -----------------------------------
        // X360 @ 0x8256FEF0: dispatch on meState (1 -> UpdateLoggedIn, 2 -> UpdateUploadEventScores).
        void ProcessBeforeSimulation();
        // X360 @ 0x82565430: pull the post-sim network-event queue from the module IO input and
        // drain it through ProcessNetworkEvents.
        void ProcessAfterSimulation( const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput );

        // --- session-state callbacks (called by BrnNetworkManager) ---------------------------
        void OnAutoLogin();    // X360 @ 0x8254B5C0
        void OnLeaveOnline();  // X360 @ 0x8254B678

        // The LobbyApi completion trampoline handed to ServerInterfaceCustomCommands::
        // UploadEventScoreData as the upload callback (matches CustomCommandCallback:
        // void(void* lpData, void* lpResult, bool lbSuccess)).
        static void _UploadEventScoreCallback( void* lpData, void* lpResult, bool lbSuccess );

        // The debug component reaches the server interface to post the upload; the X360 inlines
        // this trivial getter at the call site. Returns the BrnServerInterfaceBase the manager
        // was constructed with (the committed debug-component TU depends on this exact return type).
        BrnServerInterfaceBase* GetServerInterface() const { return mpServerInterface; }

    private:
        // X360 @ 0x82561520: walk the network-event queue, storing score-leaderboard events for
        // upload and absorbing the persisted non-uploaded-scores snapshot.
        void ProcessNetworkEvents( const CgsModule::VariableEventQueue<14000, 16>* lpNetworkEventQueue );

        // X360 @ 0x8255DFB0: merge one (eventID, score, gameMode) score into the pending list,
        // updating an existing record's best score (game mode 5 keeps the min, 7 the max) or
        // appending a new record. (Register order from the ASM: r4 eventID, r5 score, r6 gameMode.)
        void StoreEventScoreForUpload( u64 lu64EventID, u32 luScore, s32 leGameMode );

        // X360 @ 0x82556BB8: when there are pending uploads, arm the retry timer and move to the
        // uploading state; otherwise tell the network manager the auto-login flow is complete.
        void UpdateLoggedIn();

        // X360 @ 0x8256C010: once the retry timer has elapsed (and the custom-commands component
        // is idle), batch the pending records into one EventScoreData and upload it.
        void UpdateUploadEventScores();

        // X360 @ 0x8254B4B0: map an (eventID, gameMode) onto its scoreboard slot using the
        // per-mode scoreboard tables. Static helper (no `this` in the ASM).
        static s32 GetScoreboardIndexForEvent( u64 lu64EventID, s32 leGameMode );

        Array<LocalEventScoreUploadData, 49> maPendingUploads;   // +0x000 (count word @ +0x310)
        u8                    mPad314[0x318 - 0x314];             // +0x314 (reserved)
        CgsSystem::Time       mUploadRetryTimer;                  // +0x318
        s32                   miUploadCount;                      // +0x320
        s32                   meState;                            // +0x324
        BrnNetworkModule*     mpNetworkModule;                    // +0x328
        BrnServerInterfaceBase* mpServerInterface;                // +0x32C
        EventScoresManagerDebugComponent mDebugComponent;         // +0x330
    };
} // namespace BrnNetwork
