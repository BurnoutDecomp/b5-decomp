// Bodies for the online event-scores upload manager, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct                  @ 0x82556AB0
//   Destruct                   @ 0x82556B58
//   GetScoreboardIndexForEvent @ 0x8254B4B0
//   OnAutoLogin                @ 0x8254B5C0
//   OnLeaveOnline              @ 0x8254B678
//   ProcessAfterSimulation     @ 0x82565430
//   ProcessBeforeSimulation    @ 0x8256FEF0
//   ProcessNetworkEvents       @ 0x82561520
//   StoreEventScoreForUpload   @ 0x8255DFB0
//   UpdateLoggedIn             @ 0x82556BB8
//   UpdateUploadEventScores    @ 0x8256C010
//   _UploadEventScoreCallback  @ 0x825654A0
//
// The manager accumulates per-event scores the player posts during a session, batches the pending
// records into one EventScoreData, and pushes them to the server via ServerInterfaceCustomCommands::
// UploadEventScoreData -- retrying on a downloaded interval -- under a small state machine
// (meState) pumped from ProcessBeforeSimulation. The inbound network-event queue is drained in
// ProcessAfterSimulation -> ProcessNetworkEvents.
//
// FLAGGED rodata gaps (per project rule: never fabricate un-recovered rodata):
//   * ProcessNetworkEvents gates the score-leaderboard store on the same DLC/entitlement
//     capability test the sibling ScoreboardManager flags un-recovered
//     ((dword_82FFA7F4 & dword_82FFA7F8[dword_82FFA864]) ... && byte_82FFA886, and the 868/887
//     twin). The flag table + indices + the two enable bytes are file-scope rodata not recovered
//     in this slice; the gate is preserved structurally behind a documented placeholder.
//   * GetScoreboardIndexForEvent scans per-game-mode scoreboard tables (qword_8207C440 /
//     qword_8207C4B0 .. qword_8207C5C8) that are un-recovered rodata; the scan structure + the
//     game-mode dispatch (5 / 7) + the not-set-up assert are reconstructed, the table extents
//     are left as documented placeholders.

#include "GameSource/Network/Managers/BrnEventScoresManager.h"
#include "GameSource/Network/BrnNetworkModule.h"                    // BrnNetworkModule::GetNetworkManager / GetNetworkEventQueue
#include "GameSource/Network/BrnNetworkManager.h"                   // BrnNetworkManager::OnAutoLoginProcessComplete
#include "GameSource/Network/BrnServerInterfaceBase.h"              // GetCustomCommandsComponent / GetDownloadableConfigComponent / GetStatus
#include "GameSource/Network/BrnNetworkModuleIO.h"                  // BrnNetworkModuleIO::PostSim, PostSimulationInputBuffer, NetworkEventQueue
#include "GameSource/Network/Components/BrnServerInterfaceCustomCommands.h" // UploadEventScoreData
#include "GameSource/Network/Components/BrnServerInterfaceDownloadableConfig.h" // GetEventScoreUploadRetryInterval
#include "GameSource/Network/Parameters/BrnNetworkEventScoreData.h" // EventScoreData
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h" // EComponents / EStatus
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"    // VariableEventQueue<14000,16> (GetFirstEvent/GetNextEvent/AddEvent)
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT

namespace BrnNetwork
{
    // Game-mode selectors the manager dispatches on (X360-authoritative: StoreEventScoreForUpload
    // @ 0x8255DFB0 and GetScoreboardIndexForEvent @ 0x8254B4B0 both branch on cmpwi 5 / cmpwi 7).
    // 5 keeps the MINIMUM score for an event (e.g. timed events -- lower is better); 7 keeps the
    // MAXIMUM (e.g. score events). FLAG: replace with the committed EGameModeType enumerators once
    // its home can be included without a cycle; the compared values 5/7 must not change.
    static const s32 KI_GAME_MODE_KEEP_MIN = 5;   // cmpwi 5
    static const s32 KI_GAME_MODE_KEEP_MAX = 7;   // cmpwi 7

    // Network-event type codes walked by ProcessNetworkEvents (X360: cmpwi r3, 0x38 / 0x39).
    static const s32 KI_NETEVENT_SCORE_LEADERBOARD  = 0x38;   // 56 -- one event score to maybe upload
    static const s32 KI_NETEVENT_NON_UPLOADED_SCORES = 0x39;  // 57 -- the persisted pending-list snapshot

    // The custom-commands upload event the callback re-queues on the network event queue
    // (X360 _UploadEventScoreCallback @ 0x825654A0: AddEvent(queue, payload, 0x4A, 0x80)).
    static const s32 KI_NETEVENT_UPLOADED_SCORES = 0x4A;   // 74  -- event type
    static const s32 KI_UPLOADED_SCORES_PAYLOAD_SIZE = 0x80; // 128 -- event payload size in bytes

    // meState values (X360-authoritative from the state stores / compares).
    static const s32 KI_STATE_IDLE             = 0;   // nothing to do
    static const s32 KI_STATE_LOGGED_IN        = 1;   // ProcessBeforeSimulation -> UpdateLoggedIn
    static const s32 KI_STATE_UPLOADING        = 2;   // ProcessBeforeSimulation -> UpdateUploadEventScores
    static const s32 KI_STATE_UPLOAD_COMPLETE  = 3;   // callback leaves this state untouched

    // The auto-login process stage every OnAutoLoginProcessComplete call site passes (X360: li r4, 2).
    static const s32 KI_AUTOLOGIN_STAGE_SCORES_DONE = 2;

    namespace
    {
        // The BrnNetworkModuleIO::NetworkEventQueue accessor/PostSim return are typed against an
        // incomplete forward; the concrete queue is the 14000/16 variable-event queue (same pattern
        // as BrnNetworkPlayer.cpp's AsConcreteQueue).
        typedef CgsModule::VariableEventQueue<14000, 16> NetworkEventQueueConcrete;

        inline const NetworkEventQueueConcrete* AsConcreteQueue( const BrnNetworkModuleIO::NetworkEventQueue* lpQueue )
        {
            return reinterpret_cast<const NetworkEventQueueConcrete*>( lpQueue );
        }

        inline NetworkEventQueueConcrete* AsConcreteQueue( BrnNetworkModuleIO::NetworkEventQueue* lpQueue )
        {
            return reinterpret_cast<NetworkEventQueueConcrete*>( lpQueue );
        }

        // ---- network-event payloads (queue protocol; private to this manager) -------------------
        // The score-leaderboard event posted per scored event (type 0x38). Layout from the X360
        // ProcessNetworkEvents call site @ 0x82561678: ld 0(event)=eventID, lwz 0x8(event)=gameMode,
        // lwz 0xC(event)=score (the three reads feeding StoreEventScoreForUpload).
        struct ScoreLeaderboardEvent
        {
            u64 mu64EventID;   // +0x00
            s32 meGameMode;    // +0x08
            u32 muScore;       // +0x0C
        };

        // The persisted "non-uploaded scores" snapshot (type 0x39). The X360 @ 0x825615D0 loads a
        // POINTER from event+0 and hands it to Array<...,49>::AppendArray as the source array, so
        // the payload's first field is a pointer to the saved pending-uploads array.
        struct NonUploadedScoresEvent
        {
            const Array<LocalEventScoreUploadData, 49>* mpPendingUploads;   // +0x00
        };

        // The "uploaded scores" event the callback re-queues (type 0x4A, 128-byte payload). The X360
        // callback drains each uploaded record's eventID into an Array<u64,15>-shaped buffer
        // (_int64_15_::Append), with a leading count word zeroed before the loop. Modelled as the
        // grounded fixed buffer; AddEvent copies the first 128 bytes.
        struct UploadedScoresEvent
        {
            // asm wire layout: the Array<u64,15> eventID body is at the buffer base (+0), the count
            // word trails at +0x78 (var_38). AddEvent copies the first 0x80 bytes from the base.
            u64 maEventIDs[15];       // +0x00  appended eventIDs (Array<u64,15> body)
            s32 miNumEvents;          // +0x78  count (zeroed before the drain loop)

            void Append( u64 lu64EventID )
            {
                CGS_ASSERT( miNumEvents >= 0 && miNumEvents < 15, "UploadedScoresEvent full" );
                maEventIDs[miNumEvents] = lu64EventID;
                ++miNumEvents;
            }
        };
    } // anonymous namespace

    // ---------------------------------------------------------------------------------------------
    // Construct  @ 0x82556AB0
    // ---------------------------------------------------------------------------------------------
    void EventScoresManager::Construct( BrnNetworkModule* lpNetworkModule,
                                        BrnServerInterfaceBase* lpServerInterface )
    {
        // The X360 constructs the embedded debug component first (DebugComponent::Construct(this+0x330, this)).
        mDebugComponent.Construct( this );

        mpNetworkModule    = lpNetworkModule;
        mpServerInterface  = lpServerInterface;

        CGS_ASSERT( mpNetworkModule != nullptr, "mpNetworkModule" );
        CGS_ASSERT( mpServerInterface != nullptr, "mpServerInterface" );

        maPendingUploads.Clear();          // stw 0 -> +0x310 (live count word)
        miUploadCount = 0;                 // stw 0 -> +0x320
        mUploadRetryTimer.SetFloatVal( 0.0f ); // SetFloatVal(this+0x318, 0.0)
        meState = KI_STATE_IDLE;           // stw 0 -> +0x324
    }

    // ---------------------------------------------------------------------------------------------
    // Destruct  @ 0x82556B58
    // ---------------------------------------------------------------------------------------------
    void EventScoresManager::Destruct()
    {
        meState           = KI_STATE_IDLE;     // +0x324 = 0
        miUploadCount     = 0;                 // +0x320 = 0
        mUploadRetryTimer.SetFloatVal( 0.0f ); // SetFloatVal(this+0x318, 0.0)
        maPendingUploads.Clear();              // +0x310 = 0
        mpServerInterface = nullptr;           // +0x32C = 0
        mpNetworkModule   = nullptr;           // +0x328 = 0
        mDebugComponent.Destruct();            // DebugComponent::Destruct(this+0x330)
    }

    // ---------------------------------------------------------------------------------------------
    // GetScoreboardIndexForEvent  @ 0x8254B4B0
    // Map an (eventID, gameMode) onto its scoreboard slot. Each game mode has its own scoreboard
    // table; the slot index is the position of lu64EventID in that mode's table, or -1 when absent.
    // FLAGGED: the per-mode tables (qword_8207C4B0 .. qword_8207C5C8 for mode 5; qword_8207C440 ..
    // for mode 7) are un-recovered file-scope rodata, so the scan extents cannot be reconstructed
    // without fabricating data. The dispatch + the not-set-up assert ARE reconstructed; the table
    // walk is left as a documented placeholder that always reports "not found".
    // ---------------------------------------------------------------------------------------------
    s32 EventScoresManager::GetScoreboardIndexForEvent( u64 lu64EventID, s32 leGameMode )
    {
        (void)lu64EventID;

        if ( leGameMode == KI_GAME_MODE_KEEP_MIN || leGameMode == KI_GAME_MODE_KEEP_MAX )
        {
            // FLAGGED-(-1) placeholder: the per-mode scoreboard tables are un-recovered rodata.
            // The X360 scans the mode's table for lu64EventID, returning its index, or -1 when the
            // event is not present in the table. Reconstruct the scan when the tables are homed.
            return -1;
        }

        // Any other game mode means no scoreboard was set up for this event.
        CGS_ASSERT( false, "Trying to upload scores for event with no scoreboard set up\n" );
        return -1;
    }

    // ---------------------------------------------------------------------------------------------
    // OnAutoLogin  @ 0x8254B5C0
    // First call after sign-in arms the logged-in state; a second call (already armed) reports the
    // auto-login process complete to the network manager.
    // ---------------------------------------------------------------------------------------------
    void EventScoresManager::OnAutoLogin()
    {
        if ( meState != KI_STATE_IDLE )
        {
            CGS_ASSERT( mpNetworkModule != nullptr, "mpNetworkModule" );
            CGS_ASSERT( mpNetworkModule->GetNetworkManager() != nullptr,
                        "mpNetworkModule->GetNetworkManager()" );
            mpNetworkModule->GetNetworkManager()->OnAutoLoginProcessComplete( KI_AUTOLOGIN_STAGE_SCORES_DONE );
        }
        else
        {
            meState = KI_STATE_LOGGED_IN;
        }
    }

    // ---------------------------------------------------------------------------------------------
    // OnLeaveOnline  @ 0x8254B678
    // ---------------------------------------------------------------------------------------------
    void EventScoresManager::OnLeaveOnline()
    {
        meState = KI_STATE_IDLE;
    }

    // ---------------------------------------------------------------------------------------------
    // ProcessAfterSimulation  @ 0x82565430
    // ---------------------------------------------------------------------------------------------
    void EventScoresManager::ProcessAfterSimulation( const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput )
    {
        CGS_ASSERT( lpInput != nullptr, "lpInput" );
        ProcessNetworkEvents( AsConcreteQueue( BrnNetworkModuleIO::PostSim( lpInput ) ) );
    }

    // ---------------------------------------------------------------------------------------------
    // ProcessBeforeSimulation  @ 0x8256FEF0
    // ---------------------------------------------------------------------------------------------
    void EventScoresManager::ProcessBeforeSimulation()
    {
        if ( meState == KI_STATE_LOGGED_IN )
        {
            UpdateLoggedIn();
        }
        else if ( meState == KI_STATE_UPLOADING )
        {
            UpdateUploadEventScores();
        }
    }

    // ---------------------------------------------------------------------------------------------
    // ProcessNetworkEvents  @ 0x82561520
    // Walk the inbound network-event queue: store each score-leaderboard event for upload (gated on
    // the DLC/entitlement capability flags) and absorb the persisted non-uploaded-scores snapshot.
    // ---------------------------------------------------------------------------------------------
    void EventScoresManager::ProcessNetworkEvents( const CgsModule::VariableEventQueue<14000, 16>* lpNetworkEventQueue )
    {
        CGS_ASSERT( lpNetworkEventQueue != nullptr, "lpNetworkEventQueue" );

        // FLAGGED: the score-leaderboard upload is gated on the same DLC/entitlement capability test
        // the sibling ScoreboardManager flags un-recovered: two independent feature bits
        // ((dword_82FFA7F4 & dword_82FFA7F8[dword_82FFA864]) == dword_82FFA7F8[...] && byte_82FFA886,
        // and the 868/887 twin). The flag table, the two table indices and the two enable bytes are
        // un-recovered file-scope rodata. Reconstruct the real test when that rodata is homed; until
        // then the gate is preserved structurally as a documented placeholder.
        const bool lbScoreLeaderboardUploadEnabled = false;   // FLAGGED placeholder (un-recovered rodata)

        const CgsModule::Event* lpEvent = nullptr;
        s32 liSize = 0;
        s32 leEventType = lpNetworkEventQueue->GetFirstEvent( &lpEvent, &liSize );

        while ( lpEvent != nullptr )
        {
            if ( leEventType == KI_NETEVENT_SCORE_LEADERBOARD )
            {
                if ( lbScoreLeaderboardUploadEnabled )
                {
                    const ScoreLeaderboardEvent* lpScoreLeaderboardEvent =
                        reinterpret_cast<const ScoreLeaderboardEvent*>( lpEvent );
                    CGS_ASSERT( lpScoreLeaderboardEvent != nullptr, "lpScoreLeaderboardEvent" );
                    StoreEventScoreForUpload( lpScoreLeaderboardEvent->mu64EventID,
                                              lpScoreLeaderboardEvent->muScore,
                                              lpScoreLeaderboardEvent->meGameMode );
                }
            }
            else if ( leEventType == KI_NETEVENT_NON_UPLOADED_SCORES )
            {
                const NonUploadedScoresEvent* lpNonUploadedScoresEvent =
                    reinterpret_cast<const NonUploadedScoresEvent*>( lpEvent );
                CGS_ASSERT( lpNonUploadedScoresEvent != nullptr, "lpNonUploadedScoresEvent" );
                // Replace the live pending list with the saved snapshot: clear (count word -> 0),
                // then append every record of the saved array.
                maPendingUploads.Clear();
                maPendingUploads.AppendArray( *lpNonUploadedScoresEvent->mpPendingUploads );
            }

            const CgsModule::Event* lpNextEvent = nullptr;
            leEventType = lpNetworkEventQueue->GetNextEvent( lpEvent, &lpNextEvent, &liSize );
            lpEvent = lpNextEvent;
        }
    }

    // ---------------------------------------------------------------------------------------------
    // StoreEventScoreForUpload  @ 0x8255DFB0
    // Merge one (eventID, score, gameMode) score into the pending list: update an existing record's
    // best score (game mode 5 keeps the min, 7 keeps the max) or append a new record.
    // (ASM register order: r4 lu64EventID, r5 luScore, r6 leGameMode.)
    // ---------------------------------------------------------------------------------------------
    void EventScoresManager::StoreEventScoreForUpload( u64 lu64EventID, u32 luScore, s32 leGameMode )
    {
        const u32 luCount = maPendingUploads.GetLength();
        for ( u32 luIndex = 0; luIndex < luCount; ++luIndex )
        {
            if ( maPendingUploads[luIndex].mu64EventID == lu64EventID )
            {
                // Existing record for this event: keep the better score for the mode.
                if ( leGameMode == KI_GAME_MODE_KEEP_MIN )
                {
                    if ( static_cast<s32>( luScore ) < maPendingUploads[luIndex].muScore )
                    {
                        maPendingUploads[luIndex].muScore = luScore;
                    }
                }
                else if ( leGameMode == KI_GAME_MODE_KEEP_MAX )
                {
                    if ( static_cast<s32>( luScore ) > maPendingUploads[luIndex].muScore )
                    {
                        maPendingUploads[luIndex].muScore = luScore;
                    }
                }
                else
                {
                    CGS_ASSERT( false, "Trying to upload scores for event with no scoreboard set up\n" );
                }
                return;
            }
        }

        // No existing record: append a fresh one.
        LocalEventScoreUploadData* lpNewRecord = maPendingUploads.AddNew();
        lpNewRecord->mu64EventID = lu64EventID;
        lpNewRecord->meGameMode  = leGameMode;
        lpNewRecord->muScore     = luScore;
    }

    // ---------------------------------------------------------------------------------------------
    // UpdateLoggedIn  @ 0x82556BB8
    // Once logged in: if there are pending uploads, arm the retry timer and move to the uploading
    // state; otherwise report the auto-login process complete.
    // ---------------------------------------------------------------------------------------------
    void EventScoresManager::UpdateLoggedIn()
    {
        if ( maPendingUploads.GetLength() != 0 )
        {
            miUploadCount = 0;                          // *(this+0x320) = 0
            mUploadRetryTimer.SetFloatVal( 0.0f );      // SetFloatVal(this+0x318, 0.0)
            meState = KI_STATE_UPLOADING;
        }
        else
        {
            CGS_ASSERT( mpNetworkModule != nullptr, "mpNetworkModule" );
            CGS_ASSERT( mpNetworkModule->GetNetworkManager() != nullptr,
                        "mpNetworkModule->GetNetworkManager()" );
            mpNetworkModule->GetNetworkManager()->OnAutoLoginProcessComplete( KI_AUTOLOGIN_STAGE_SCORES_DONE );
            meState = KI_STATE_IDLE;
        }
    }

    // ---------------------------------------------------------------------------------------------
    // UpdateUploadEventScores  @ 0x8256C010
    // When the retry timer has elapsed (and the custom-commands component is idle), batch the
    // pending records into one EventScoreData and upload it.
    // ---------------------------------------------------------------------------------------------
    void EventScoresManager::UpdateUploadEventScores()
    {
        // The X360 builds a Time(0.0) and subtracts it from mUploadRetryTimer (a no-op identity used
        // to read the elapsed value), then tests whether the timer + its fraction has elapsed.
        const CgsSystem::Time lZeroTime( 0.0f );
        const CgsSystem::Time lElapsed = mUploadRetryTimer - lZeroTime;

        // lbReadyToUpload == (miUploadCount == 0) && ((seconds + fraction) <= 0), i.e. the retry
        // window has expired and no batch is currently in flight.
        bool lbReadyToUpload = true;
        if ( miUploadCount != 0 )
        {
            lbReadyToUpload = false;
        }
        else
        {
            const f32 lfElapsed = static_cast<f32>( lElapsed.GetSeconds() ) + lElapsed.GetFraction();
            if ( lfElapsed > 0.0f )
            {
                lbReadyToUpload = false;
            }
        }

        if ( maPendingUploads.GetLength() != 0
             && lbReadyToUpload
             && mpServerInterface->GetStatus( CgsNetwork::E_COMPONENTS_CUSTOM_COMMANDS )
                    == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE )
        {
            EventScoreData lEventScoreData;

            const u32 luCount = maPendingUploads.GetLength();
            for ( u32 luIndex = 0; luIndex < luCount; ++luIndex )
            {
                ++miUploadCount;
                LocalEventScoreUploadData& lrRecord = maPendingUploads[luIndex];
                const s32 liScoreboardIndex = GetScoreboardIndexForEvent( lrRecord.mu64EventID,
                                                                          lrRecord.meGameMode );
                // SetScoreData(scoreboardIndex, score, gameMode); stops once the batch is full.
                if ( lEventScoreData.AddEventScore( liScoreboardIndex,
                                                    static_cast<s32>( lrRecord.muScore ),
                                                    lrRecord.meGameMode ) )
                {
                    break;
                }
            }

            CGS_ASSERT( mpServerInterface != nullptr, "mpServerInterface" );
            CGS_ASSERT( mpServerInterface->GetCustomCommandsComponent() != nullptr,
                        "mpServerInterface->GetCustomCommandsComponent()" );
            mpServerInterface->GetCustomCommandsComponent()->UploadEventScoreData(
                &lEventScoreData, &EventScoresManager::_UploadEventScoreCallback, this );
        }
        else
        {
            if ( maPendingUploads.GetLength() == 0 )
            {
                meState = KI_STATE_LOGGED_IN;
            }
        }
    }

    // ---------------------------------------------------------------------------------------------
    // _UploadEventScoreCallback  @ 0x825654A0
    // The LobbyApi completion trampoline. On success (and while uploading), drain the just-uploaded
    // records out of the pending list onto a network event, re-arm the retry timer from the
    // downloaded interval, and clear the in-flight counter. On failure, clear the component error,
    // reset the upload state and report the auto-login process complete.
    // ---------------------------------------------------------------------------------------------
    void EventScoresManager::_UploadEventScoreCallback( void* lpData, void* lpResult, bool lbSuccess )
    {
        (void)lpResult;

        EventScoresManager* lpEventScoresManager = static_cast<EventScoresManager*>( lpData );
        CGS_ASSERT( lpEventScoresManager != nullptr, "lpEventScoresManager" );

        BrnServerInterfaceBase* lpServerInterface = lpEventScoresManager->mpServerInterface;
        CGS_ASSERT( lpServerInterface != nullptr, "lpServerInterface" );

        if ( lbSuccess && lpEventScoresManager->meState == KI_STATE_UPLOADING )
        {
            // Move every uploaded record off the pending list onto a fresh "uploaded scores" event.
            UploadedScoresEvent lUploadedScoresEvent;
            lUploadedScoresEvent.miNumEvents = 0;

            for ( s32 liIndex = 0; liIndex < lpEventScoresManager->miUploadCount; ++liIndex )
            {
                lUploadedScoresEvent.Append( lpEventScoresManager->maPendingUploads[0].mu64EventID );
                lpEventScoresManager->maPendingUploads.Erase( 0 );
            }

            CGS_ASSERT( lpEventScoresManager->mpNetworkModule != nullptr,
                        "lpEventScoresManager->mpNetworkModule" );
            CGS_ASSERT( lpEventScoresManager->mpNetworkModule->GetNetworkEventQueue() != nullptr,
                        "lpEventScoresManager->mpNetworkModule->GetNetworkEventQueue()" );
            AsConcreteQueue( lpEventScoresManager->mpNetworkModule->GetNetworkEventQueue() )->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &lUploadedScoresEvent ),
                KI_NETEVENT_UPLOADED_SCORES, KI_UPLOADED_SCORES_PAYLOAD_SIZE );

            CGS_ASSERT( lpServerInterface->GetDownloadableConfigComponent() != nullptr,
                        "lpServerInterface->GetDownloadableConfigComponent()" );
            lpEventScoresManager->mUploadRetryTimer.SetFloatVal(
                lpServerInterface->GetDownloadableConfigComponent()->GetEventScoreUploadRetryInterval() );
            lpEventScoresManager->miUploadCount = 0;
        }
        else
        {
            CGS_ASSERT( lpServerInterface->GetCustomCommandsComponent() != nullptr,
                        "lpServerInterface->GetCustomCommandsComponent()" );
            ServerInterfaceCustomCommands* lpCustomCommands = lpServerInterface->GetCustomCommandsComponent();
            if ( lpCustomCommands->GetStatus() == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_ERROR )
            {
                lpCustomCommands->ClearLastError();
            }

            if ( lpEventScoresManager->meState != KI_STATE_UPLOAD_COMPLETE )
            {
                lpEventScoresManager->meState = KI_STATE_IDLE;
            }

            CGS_ASSERT( lpEventScoresManager->mpNetworkModule != nullptr,
                        "lpEventScoresManager->mpNetworkModule" );
            CGS_ASSERT( lpEventScoresManager->mpNetworkModule->GetNetworkManager() != nullptr,
                        "lpEventScoresManager->mpNetworkModule->GetNetworkManager()" );
            lpEventScoresManager->mpNetworkModule->GetNetworkManager()->OnAutoLoginProcessComplete(
                KI_AUTOLOGIN_STAGE_SCORES_DONE );
        }
    }
} // namespace BrnNetwork
