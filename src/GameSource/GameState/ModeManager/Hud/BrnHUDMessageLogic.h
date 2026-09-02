#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"               // CgsID (== u64)
#include "GameSource/BurnoutConstants.h"  // EActiveRaceCarIndex, E_ACTIVE_RACE_CAR_INDEX_COUNT (== 8)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // BitArray<N>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // VariableEventQueue<256,16>
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h" // ScoringSystem, CarData (by pointer),
                                                                       // StuntModeScoring + StuntInfo (GenerateStuntMessage)
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

        // ---- the stunt-scorer notification set (X360 GenerateStuntMessage @0x82394DF8) ----
        // GenerateStuntMessage queues exactly ONE of the three notification records per frame and,
        // for the stunt and combo records, follows it with the running-score record. The four
        // literals are the X360's own AddEvent type arguments (`li r5, 0x85` @0x82394E98,
        // `li r5, 0x84` @0x82394EF8, `li r5, 0x86` @0x82394F44, `li r5, 0x14` @0x82394EB0/F20).
        // FLAG (names, not values): the DecFIGS EGameActionType dump is the PS3 enum and carries
        // this semantic triple as E_ACTION_HUD_MESSAGE_STUNT_PERFORMED / _COMBO_PERFORMED /
        // _STUNT_TIME_UP; the names below are taken from it, the VALUES from the X360 asm. The
        // fourth (20) is the running-score push both banking arms make -- name is role-derived.
        // Re-confirm all four when the X360 EGameActionType band holding 20 / 132..134 is dumped.
        E_HUD_MESSAGE_SCORE_UPDATE          = 20,   // 0x14   running score, 4 bytes
        E_HUD_MESSAGE_STUNT_PERFORMED       = 132,  // 0x84   the banked StuntInfo record
        E_HUD_MESSAGE_COMBO_PERFORMED       = 133,  // 0x85   combo timer + score + validity
        E_HUD_MESSAGE_STUNT_TIME_UP         = 134,  // 0x86   1 byte, no payload
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

    // The banked-stunt notification: the StuntInfo record WasStuntRecentlyPerformed hands back.
    // The X360 posts the very buffer it passed to that query (`addi r4, r1, var_30` is both the
    // query's out-param and the AddEvent payload), with liSize 24 -- the X360 sizeof(StuntInfo)
    // (6 dwords, matching the query's own `v5 = 6` copy loop). The host StuntInfo is 20 bytes,
    // so the SIZE is taken from sizeof() at the call site, never from the X360 literal.
    struct StuntPerformedMessage : public CgsModule::Event
    {
        StuntInfo mStuntInfo;
    };

    // 12-byte combo-completed notification. The X360 builds it as the THREE out-params of
    // WasComboRecentlyPerformed in three adjacent stack slots and posts the block from the lowest
    // of them (`addi r6, r1, var_48` = the f32*, `addi r4, r1, var_44` = the s32*, `addi r5, r1,
    // var_40` = the bool*; AddEvent takes var_48 with liSize 12) -- so the record IS
    // { f32; s32; bool } in that order, and the query fills it in place.
    struct ComboPerformedMessage : public CgsModule::Event
    {
        f32  mfComboTime;   // +0  mfRecentComboTime      (+0x88)
        s32  miComboScore;  // +4  miRecentComboScore     (+0x84)
        bool mbValidCombo;  // +8  miRecentComboScore >= miCurrentScore/2
    };

    // 4-byte running-score notification, pushed straight after a stunt or combo record.
    struct ScoreUpdateMessage : public CgsModule::Event
    {
        s32 miScore;
    };

    // 1-byte "stunt time is up" notification. The X360 posts an UNINITIALISED one-byte stack local
    // (`addi r4, r1, var_50`, no preceding store): the type ID carries the whole meaning.
    struct StuntTimeUpMessage : public CgsModule::Event
    {
        u8 muUnused;
    };

    // ------------------------------------------------------------------------
    // Lifecycle + per-frame entry points.
    // ------------------------------------------------------------------------

    // X360 0x8236F530. Binds the action queue's buffer, seeds the latched mode type to
    // E_MODE_NONE and runs Prepare(). ModeManager::Construct calls it (console 0x82340008).
    void Construct();

    // X360 0x82366478. Re-seeds every message edge-tracker. Called by Construct and by
    // PostWorldUpdate the first frame the game mode changes.
    void Prepare();

    // X360 0x82389248. Drains a frame of notifications into the module's outgoing game-action
    // queue and empties the local one. ModeManager::PreWorldUpdate calls it (console 0x82353CF4).
    void PreWorldUpdate(GameStateModuleIO::GameActionQueue* lpOutputGameActionQueue);

    // X360 0x8239D998. Latches the game mode, ticks the in-mode clock and runs the per-mode
    // message generators. REDUCED ARGUMENT SET -- see the body for the console's full ten and
    // for the arms this build does not reproduce.
    void PostWorldUpdate(const StuntModeScoring::ActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
                         GameStateModuleIO::EGameModeType leGameModeType,
                         ScoringSystem* lpScoringSystem,
                         f32 lfDelta);

    // X360 0x82394DF8. THE stunt scorer's notification pump, and the ONLY consumer of the
    // scorer's three one-shot latches (mbRecentCombo / mbRecentStunt / the time-up edge) in the
    // whole image. Runs for the offline stunt-attack mode and the online stunt-run family.
    void GenerateStuntMessage(ScoringSystem* lpScoringSystem);

    // X360 0x82395BA8 (DWARF BrnHUDMessageLogic.h:199). The ROAD RAGE arm (mode 3): when the
    // player's car is not mid-crash and the road-rage scorer has a critical-damage message
    // armed, queue one DamageCriticalMessageAction (id 52, 1 byte, `true`) and disarm the
    // scorer's flag. PostWorldUpdate's case 3 is its only caller (console 0x8239D998).
    void GenerateCriticalDamageMessage(const StuntModeScoring::ActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
                                       ScoringSystem* lpScoringSystem);

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

    // The game mode this object is currently generating messages for (X360 +0x1C0). Construct
    // seeds it to E_MODE_NONE (`stw r28(-1), 0x1C0(r31)` @0x8236F5D8) and PostWorldUpdate latches
    // the incoming mode into it, re-running Prepare() on every change. GenerateStuntMessage reads
    // it to pick the online vs offline stunt scorer -- it is NOT a copy of ModeManager's own
    // meCurrentGameModeType, it is this object's one-frame-latched view of it.
    GameStateModuleIO::EGameModeType meCurrentGameModeType;   // +0x1C0 (448)

    // Seconds spent in the latched mode (X360 +0x1C4). PostWorldUpdate accumulates the frame
    // delta into it; Prepare zeroes it.
    f32                 mfTimeInMode;                 // +0x1C4 (452)

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
