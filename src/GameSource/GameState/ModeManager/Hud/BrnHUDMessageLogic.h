#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"               // CgsID (== u64)
#include "GameSource/BurnoutConstants.h"  // EActiveRaceCarIndex, E_ACTIVE_RACE_CAR_INDEX_COUNT (== 8)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // BitArray<N>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // VariableEventQueue<256,16>
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h" // ScoringSystem, CarData (by pointer)
#include "GameSource/GameState/BrnGameStateSharedIO.h"               // GameStateModuleIO::EPlayerTeam
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"          // BrnNetwork::NetworkPlayerID
#include "GameShared/GameClasses/System/Timer/CgsTime.h"             // CgsSystem::Time

// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.h
// ============================================================================
// Home for BrnGameState::HUDMessageLogic -- the per-frame HUD message / notification
// generator for the online stunt-run game modes. It watches the ScoringSystem each
// frame and pushes typed notification records (team eliminated / new leader / victory /
// combo-ending / score-milestone / time-warning) into its embedded action queue, which
// the HUD presentation layer drains.
//
// LAYOUT NOTE: the DecFIGS DWARF carries no dump for this TU, so this layout is
// semantic-parity (named members, right order/types), NOT byte-exact. The X360 binary
// reaches each member at a fixed offset (the action queue at object offset 0; the
// stunt-run tracker scalars at +0x1D0..+0x1E8; the team-change bit set at +0x240); the
// member run below preserves that ordering -- queue first, then the tracker scalars,
// then the bit set -- so the by-name accesses in the .cpp mirror the X360 store-for-store.
namespace BrnGameState
{
class HUDMessageLogic
{
public:
    // BrnHUDMessageLogic.h:244 -- one car queued for an online-crash HUD message.
    struct BufferedCrashingCar
    {
        CgsID               mRivalID;               // BrnHUDMessageLogic.h:246 (u64, +0)
        f32                 mfTimeUntilUnbuffered;  // BrnHUDMessageLogic.h:247 (+8)
        EActiveRaceCarIndex meActiveRaceCarIndex;   // BrnHUDMessageLogic.h:248 (+12)
    };

    // ------------------------------------------------------------------------
    // HUD notification (action-queue event) types. The X360 queues each record into the
    // action queue with one of these literal type IDs (0x10A..0x10F); each ID selects how
    // the HUD presentation layer renders the record. Names are domain-derived from the
    // generator method that emits each ID.
    // ------------------------------------------------------------------------
    enum EHUDMessageType : s32
    {
        E_HUD_MESSAGE_STUNT_RUN_ELIMINATION = 266,  // 0x10A  GenerateOnlineStuntRunEliminationMessages
        E_HUD_MESSAGE_STUNT_RUN_LEADING     = 267,  // 0x10B  GenerateOnlineStuntRunLeadingMessages
        E_HUD_MESSAGE_STUNT_RUN_VICTORY     = 268,  // 0x10C  GenerateOnlineStuntRunVictoryMessages
        E_HUD_MESSAGE_STUNT_RUN_COMBO_END   = 269,  // 0x10D  GenerateOnlineStuntRunTimeMessages (combo)
        E_HUD_MESSAGE_STUNT_RUN_SCORE       = 270,  // 0x10E  GenerateOnlineStuntRunScoreMessages
        E_HUD_MESSAGE_STUNT_RUN_TIME        = 271,  // 0x10F  GenerateOnlineStuntRunTimeMessages (30s)
    };

    // ------------------------------------------------------------------------
    // Action-queue payload records. Each is the exact byte image the X360 builds on the
    // stack and hands to VariableEventQueue::AddEvent (the trailing liSize argument gives
    // the byte count). Named so the generators fill them by member, not raw offset.
    // ------------------------------------------------------------------------

    // 12-byte record for the team-eliminated / new-leader / victory notifications.
    struct StuntRunTeamMessage : public CgsModule::Event
    {
        s32                         miTeam;               // +0 the team this message is about (0 when reporting an individual)
        BrnNetwork::NetworkPlayerID mNetworkPlayerID;     // +4 the reported player (-1 when reporting a team)
        bool                        mbLocalPlayerOnTeam;  // +8 the local player belongs to miTeam
        bool                        mbLocalPlayerIsSubject;// +9 the message subject is the local player
    };

    // 8-byte record for the score-milestone notification.
    struct StuntRunScoreMessage : public CgsModule::Event
    {
        BrnNetwork::NetworkPlayerID mNetworkPlayerID;     // +0 the player who crossed the milestone
        s32                         miScore;              // +4 the milestone score (1,000,000)
    };

    // 1-byte combo-ending notification (no payload fields; the type ID carries the meaning).
    struct StuntRunComboEndMessage : public CgsModule::Event
    {
        u8 muUnused;
    };

    // 4-byte time-warning notification carrying the warning threshold (seconds).
    struct StuntRunTimeMessage : public CgsModule::Event
    {
        f32 mfWarningTimeSeconds;
    };

    // ------------------------------------------------------------------------
    // Online stunt-run per-frame message generators (this TU). Each takes the live
    // ScoringSystem and the local player's active-race-car index, examines the relevant
    // scoring state, and -- on a state transition -- pushes one record into mActionQueue.
    // ------------------------------------------------------------------------
    void GenerateOnlineStuntRunEliminationMessages(ScoringSystem* lpScoringSystem,
                                                   EActiveRaceCarIndex leLocalPlayerIndex); // X360 0x82394A78
    void GenerateOnlineStuntRunVictoryMessages(ScoringSystem* lpScoringSystem,
                                               EActiveRaceCarIndex leLocalPlayerIndex);     // X360 0x82394838
    void GenerateOnlineStuntRunLeadingMessages(ScoringSystem* lpScoringSystem,
                                               EActiveRaceCarIndex leLocalPlayerIndex,
                                               const CgsSystem::Time& lCurrentTime);        // X360 0x82394920
    void GenerateOnlineStuntRunTimeMessages(ScoringSystem* lpScoringSystem,
                                            const CgsSystem::Time& lCurrentTime,
                                            f32 lfComboWarningTime);                        // X360 0x82394B88
    void GenerateOnlineStuntRunScoreMessages(ScoringSystem* lpScoringSystem);               // X360 0x82394CC0

    // Record that the given active-race-car has changed team this round (drives a HUD
    // team-change notification elsewhere). X360 0x8231E498.
    void OnlineTeamChange(EActiveRaceCarIndex leActiveRaceCarIndex);

private:
    // ===== data members (semantic-parity order; see LAYOUT NOTE above) =====
    // The action queue is the first member (object offset 0): the X360 passes `this`
    // directly as the queue pointer to AddEvent.
    CgsModule::VariableEventQueue<256, 16> mActionQueue;

    // Stunt-run score-milestone tracker (X360 +0x1D0..+0x1D8): the rival whose milestone
    // is being watched plus the previous/current score samples used to detect the
    // 1,000,000-point crossing.
    EActiveRaceCarIndex miScoreMessageRaceCarIndex;   // +0x1D0  (-1 == none pending)
    s32                 miScoreSampleThisFrame;       // +0x1D4
    s32                 miScoreSampleLastFrame;        // +0x1D8

    // Stunt-run notification edge trackers (X360 +0x1DC..+0x1E4): the last value that
    // triggered each notification, so the message fires only on a transition.
    EActiveRaceCarIndex meEliminationRaceCarIndex;    // +0x1DC  pending "rival eliminated" car (-1 == none)
    s32                 miLastLeadingTeam;            // +0x1E0  last team reported as leading
    s32                 miLastVictoryTeam;            // +0x1E4  last team reported as victorious (0 == none yet)

    // Last time-warning threshold already announced (X360 +0x1E8), so the 30-second
    // warning fires once.
    f32                 mfLastTimeWarningAnnounced;   // +0x1E8

    // Per-car "has changed team this round" flags (X360 +0x240). One bit per active race
    // car slot (E_ACTIVE_RACE_CAR_INDEX_COUNT == 8).
    CgsContainers::BitArray<E_ACTIVE_RACE_CAR_INDEX_COUNT> mTeamChangedBits; // +0x240
};
}
