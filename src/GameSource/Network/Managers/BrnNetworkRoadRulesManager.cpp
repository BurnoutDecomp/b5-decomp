// ===================================================================================
// BrnNetwork::NetworkRoadRulesManager  -- recovered function bodies
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkRoadRulesManager.cpp
//
// This TU homes the four X360-recovered functions of the online road-rules sync manager:
//   NetworkRoadRulesManager()              @ 0x827E2BE8  (the C++ constructor)
//   GetNextFreeRoadRulesDataEntry()        @ 0x82549F60  (first free per-player slot)
//   GetRoadRulesDataForPlayer(NetworkPlayerID) @ 0x82549ED0 (slot lookup by player)
//   OnAutoLogin()                          @ 0x82564338  (auto-login config pull + kickoff)
//
// Every store / branch / constant below is grounded in the X360 assembly listed in the
// dossier postmortem (scratchpad/wave33). Sub-object layout/offsets are by-name only; the
// absolute X360 byte offsets are quoted as provenance comments.
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkRoadRulesManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                  // CGS_ASSERT
#include "GameSource/Network/BrnNetworkModule.h"                                    // BrnNetworkModule (GetGameStateToNetworkInterface / GetNetworkManager / GetNetworkEventQueue)
#include "GameSource/Network/BrnNetworkManager.h"                                   // BrnNetworkManager::OnAutoLoginProcessComplete
#include "GameSource/Network/BrnNetworkModuleIO.h"                                  // BrnNetworkModuleIO::NetworkEventQueue
#include "GameSource/Network/BrnServerInterface.h"                                  // BrnServerInterface::GetServerInfoComponent
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"      // GameStateToNetworkInterface::GetCurrentGameMode
#include "GameSource/GameState/BrnGameStateSharedIO.h"                              // BrnGameState::GameStateModuleIO::EGameModeType
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceServerInfo.h" // ServerInterfaceServerInfo
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                    // VariableEventQueue<14000,16> (AddEvent)

namespace BrnNetwork
{
    namespace
    {
        // The BrnNetworkModuleIO::NetworkEventQueue accessor return is typed against an incomplete
        // forward; the concrete queue is the 14000/16 variable-event queue (same pattern as
        // BrnChallengeSuccessManager.cpp / BrnEventScoresManager.cpp).
        typedef CgsModule::VariableEventQueue<14000, 16> NetworkEventQueueConcrete;

        inline NetworkEventQueueConcrete* AsConcreteQueue(
            BrnNetworkModuleIO::NetworkEventQueue* lpQueue)
        {
            return reinterpret_cast<NetworkEventQueueConcrete*>(lpQueue);
        }

        // X360 immediates from OnAutoLogin's AddEvent(&v10, 0x26, 4): the reset-date OUT-event
        // type id and its 4-byte timestamp payload size.
        const s32 KI_OUTEVENT_ROAD_RULES_RESET_DATE      = 0x26;   // 38
        const s32 KI_OUTEVENT_ROAD_RULES_RESET_DATE_SIZE = 4;

        // X360 immediate: miState value once the auto-login config has been primed (stw 5,0x3634).
        const s32 KI_STATE_PRIMED = 5;

        // X360 GetStringFromClientConfig out-length immediate (li r6,0x10).
        const s32 KI_SCORE_KEY_LENGTH = 16;
    }

    // -----------------------------------------------------------------------------
    // NetworkRoadRulesManager() @ 0x827E2BE8
    //
    // The compiler-synthesised C++ constructor. The X360 body installs the four embedded
    // sub-object vtables inside each of the seven PerPlayerData slots (the `stw off_820CFD98`
    // / `stw off_820CFDC0` stores at slot-relative +0x000/+0x120/+0x240/+0x278, repeated at the
    // +0x2B8 slot stride) plus the trailing sub-object vtable at +0x3650 (`stw off_820CE18C`),
    // and clears this object's own score/time scalar pair to 0 / 0.0f
    // (the two `stw 0` + two `stfs flt_82001CC0` (== 0.0f) stores at +0x3610..+0x361C).
    //
    // The per-slot vtable installs are the inlined construction of each slot's embedded
    // sub-objects (and the +0x3650 install is a trailing embedded sub-object's vtable); those
    // are owned by the sub-objects' own constructors, exactly as the binary inlines them here,
    // so the human constructor lets them construct themselves and only writes this object's own
    // grounded scalar defaults. (The manager's pointer/state/flag members -- mpServerInterface,
    // mpNetworkModule, miState, mbHaveLocalRoadRulesScores -- are NOT touched by this ctor in the
    // asm; they are initialised later by the manager's lifecycle Construct().)
    // -----------------------------------------------------------------------------
    NetworkRoadRulesManager::NetworkRoadRulesManager()
        : miField3610( 0 )    // X360 +0x3610  stw r8(=0)
        , mfField3614( 0.0f ) // X360 +0x3614  stfs flt_82001CC0 (== 0.0f)
        , miField3618( 0 )    // X360 +0x3618  stw r8(=0)
        , mfField361C( 0.0f ) // X360 +0x361C  stfs flt_82001CC0 (== 0.0f)
    {
        // (the per-slot sub-object vtables and the +0x3650 sub-object vtable are installed by the
        //  embedded sub-objects' own inlined constructors, matching the binary.)
    }

    // -----------------------------------------------------------------------------
    // GetNextFreeRoadRulesDataEntry() @ 0x82549F60
    //
    // Walk the seven per-player slots looking for a free one (mPlayerID == -1). Return the first
    // free slot. If all seven are in use, fire the "no free entry" assert and return nullptr.
    // (X360: scan starts at this+0x2B0 == &maPlayerData[0].mPlayerID, stride 0x2B8; the sentinel
    //  compared is -1; the returned pointer is the slot base 696*index + this.)
    // -----------------------------------------------------------------------------
    NetworkRoadRulesManager::PerPlayerData* NetworkRoadRulesManager::GetNextFreeRoadRulesDataEntry()
    {
        for ( s32 liIndex = 0; liIndex < KI_NUMBER_OF_PLAYER_DATA_SLOTS; ++liIndex )
        {
            if ( maPlayerData[liIndex].mPlayerID == -1 )
            {
                return &maPlayerData[liIndex];
            }
        }

        CGS_ASSERT( false, "Unable to find a free road rules data entry\n" );
        return nullptr;
    }

    // -----------------------------------------------------------------------------
    // GetRoadRulesDataForPlayer(NetworkPlayerID lPlayerID) @ 0x82549ED0
    //
    // Assert the player id is valid, then walk the seven per-player slots for the one keyed by
    // lPlayerID and return it; return nullptr if no slot matches.
    // (X360: scan starts at this+0x2B0 == &maPlayerData[0].mPlayerID, stride 0x2B8; the returned
    //  pointer is the slot base 696*index + this.)
    // -----------------------------------------------------------------------------
    NetworkRoadRulesManager::PerPlayerData* NetworkRoadRulesManager::GetRoadRulesDataForPlayer(
        NetworkPlayerID lPlayerID )
    {
        CGS_ASSERT( lPlayerID != -1, "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID" );

        for ( s32 liIndex = 0; liIndex < KI_NUMBER_OF_PLAYER_DATA_SLOTS; ++liIndex )
        {
            if ( maPlayerData[liIndex].mPlayerID == lPlayerID )
            {
                return &maPlayerData[liIndex];
            }
        }

        return nullptr;
    }

    // -----------------------------------------------------------------------------
    // OnAutoLogin() @ 0x82564338
    //
    // If the auto-login config has already been primed (miState != 0): just tell the network
    // manager the auto-login process completed (stage 0).
    //
    // Otherwise (miState == 0): only proceed when the current game mode warrants pulling road-rules
    // config -- offline modes (gameMode < E_MODE_ONLINE_MODE_START), the online free-burn lobby
    // (E_MODE_ONLINE_FREE_BURN_LOBBY), or online showtime (E_MODE_ONLINE_SHOWTIME). When it does:
    //   * read the road-rules score key + reset date out of the server-info client config,
    //   * post the reset date onto the network event queue (out-event 0x26, 4-byte payload),
    //   * download local road-rules high scores (unless we already have them),
    //   * download the global road-rules high scores, upload any new local scores,
    //   * and mark the manager primed (miState = 5).
    //
    // X360 offsets (provenance): miState @+0x3634, mpServerInterface @+0x3644,
    // mpNetworkModule @+0x3648, mbHaveLocalRoadRulesScores @+0x364C, maRoadRulesScoreKey @+0x3588.
    // The server-info component pointer is *(mpServerInterface + 0x2C) == GetServerInfoComponent().
    // Game-mode immediates: 0xA == E_MODE_ONLINE_MODE_START, 0xF == E_MODE_ONLINE_FREE_BURN_LOBBY,
    // 0x10 == E_MODE_ONLINE_SHOWTIME (read from the asm cmpwi's).
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::OnAutoLogin()
    {
        if ( miState != 0 )
        {
            CGS_ASSERT( mpNetworkModule != nullptr, "mpNetworkModule" );
            CGS_ASSERT( mpNetworkModule->GetNetworkManager() != nullptr,
                        "mpNetworkModule->GetNetworkManager()" );

            BrnNetworkManager* lpNetworkManager = mpNetworkModule->GetNetworkManager();
            lpNetworkManager->OnAutoLoginProcessComplete( 0 );
            return;
        }

        const BrnGameState::GameStateModuleIO::EGameModeType leGameMode =
            mpNetworkModule->GetGameStateToNetworkInterface()->GetCurrentGameMode();

        const bool lbShouldPrime =
            ( leGameMode <  BrnGameState::GameStateModuleIO::E_MODE_ONLINE_MODE_START )
            || ( leGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY )
            || ( leGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME );

        if ( !lbShouldPrime )
        {
            return;
        }

        CGS_ASSERT( mpServerInterface != nullptr, "mpServerInterface" );
        CGS_ASSERT( mpServerInterface->GetServerInfoComponent() != nullptr,
                    "mpServerInterface->GetServerInfoComponent()" );

        mpServerInterface->GetServerInfoComponent()->GetStringFromClientConfig(
            "ROAD_RULES_SKEY", maRoadRulesScoreKey, KI_SCORE_KEY_LENGTH );

        const s32 liResetDate =
            mpServerInterface->GetServerInfoComponent()->GetTimeStampFromClientConfig(
                "ROAD_RULES_RESET_DATE" );

        CGS_ASSERT( mpNetworkModule != nullptr, "mpNetworkModule" );
        CGS_ASSERT( mpNetworkModule->GetNetworkEventQueue() != nullptr,
                    "mpNetworkModule->GetNetworkEventQueue()" );

        AsConcreteQueue( mpNetworkModule->GetNetworkEventQueue() )->AddEvent(
            reinterpret_cast<const CgsModule::Event*>( &liResetDate ),
            KI_OUTEVENT_ROAD_RULES_RESET_DATE, KI_OUTEVENT_ROAD_RULES_RESET_DATE_SIZE );

        if ( !mbHaveLocalRoadRulesScores )
        {
            AttemptToDownloadLocalRoadRulesHighScores();
        }

        AttemptToDownloadRoadRulesHighScores();
        AttemptToUploadNewRoadRulesScores();

        miState = KI_STATE_PRIMED;
    }
}
