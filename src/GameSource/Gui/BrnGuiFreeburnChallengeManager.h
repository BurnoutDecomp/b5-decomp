// BrnGuiFreeburnChallengeManager.h
// Home of BrnGui::FreeburnChallengeManager, the GUI-side freeburn-challenge tracker.
// This slice reconstructs the five out-of-line accessors the X360 ARTIST build emits:
//
//   GetCurrentChallenge          @ 0x8240EC30  -> mpCurrentChallenge                 (a1[5]  / 0x14)
//   GetCurrentAction             @ 0x8240EC88  -> mpCurrentChallenge->GetAction(miCurrentAction)
//   GetCurrentSuccessForARCI     @ 0x8240ED50  -> maaeComplete[miCurrentTargetIndex][arci]
//                                                 (IDA truncated the name to "GetCu")
//   GetCurrentContributionForARCI@ 0x8240EE50  -> maafIndividualTargetContributions[idx][arci]
//   GetCurrentTargetType         @ 0x8240EF50  -> maeChallengeTargetTypes[miCurrentTargetIndex]
//                                                 (IDA truncated the name to "GetCur")
//
// Sources:
//   * X360 BURNOUT_X360_ARTIST.XEX (binary, AUTHORITATIVE on word offsets):
//       meInternalState @ a1[1]=0x04, miCurrentTargetIndex @ a1[4]=0x10,
//       mpCurrentChallenge @ a1[5]=0x14, miTargetsCount @ a1[7]=0x1C,
//       maeChallengeTargetTypes @ a1[8]=0x20, maaeComplete @ a1[10]=0x28,
//       maafIndividualTargetContributions @ a1[26]=0x68, miCurrentAction @ a1[44]=0xB0.
//   * references/DecFIGS/dwarfdump/GameSource/Gui/BrnGuiFreeburnChallengeManager.h
//       (member names/order/types + EInternalState/EPageState enums).
//
// The four index-validating asserts (meInternalState != OFF; miCurrentTargetIndex <
// miTargetsCount; miCurrentTargetIndex < KI_MAX_ACTIONS_PER_CHALLENGE; ARCI bounds)
// are the project CGS_ASSERT (non-fatal; the X360 returns the value even on failure).

#pragma once

#include "types.hpp"                                            // s32/f32 widths, bool
#include "BrnCommonTypes.h"                                     // CgsID, Vector3
#include "GameSource/BurnoutConstants.h"                        // EActiveRaceCarIndex
#include "SharedClasses/DataLists/ChallengeListEntry.h"         // BrnResource::ChallengeListEntry(Action)
#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT

namespace BrnGui
{

class GuiCache; // forward (mpGuiCache; full type in GameSource/Gui/BrnGuiCache.h)

// Freeburn-challenge GUI event payloads (pointer-only parameters of the handlers
// reconstructed below). Full layouts in GameSource/Gui/Events/BrnGuiChallengeEvents.h.
struct GuiChallengeStartEvent;
struct GuiChallengeTriggerResponse;
struct GuiChallengeUpdateEvent;

struct FreeburnChallengeManager
{
    // -- BrnGuiFreeburnChallengeManager.h:184 (DWARF) --
    enum EInternalState
    {
        E_INTERNAL_STATE_OFF         = 0,
        E_INTERNAL_STATE_NOT_ACTIVE  = 1,
        E_INTERNAL_STATE_INITIALISED = 2,
        E_INTERNAL_STATE_RUNNING     = 3,
        E_INTERNAL_STATE_RESULTS     = 4,
        E_INTERNAL_STATE_COUNT       = 5,
    };

    // -- BrnGuiFreeburnChallengeManager.h:173 (DWARF) --
    enum EPageState
    {
        E_PAGE_STATE_NONE        = 0,
        E_PAGE_STATE_AUTO_ROTATE = 1,
        E_PAGE_STATE_SELECT      = 2,
        E_PAGE_STATE_COUNT       = 3,
    };

    // Per-ARCI completion state for a target. Mirrors
    // BrnGameState::GameStateModuleIO::EFreeburnChallengeSuccess
    // (BrnGameStateSharedIO.h:541, DWARF). Reproduced by value here so the
    // maaeComplete[2][8] array has the correct 4-byte element width without pulling
    // the GameState-IO header graph into this GUI tracker.
    enum EFreeburnChallengeSuccess
    {
        E_FREEBURN_CHALLENGE_SUCCESS_NONE             = 0,
        E_FREEBURN_CHALLENGE_SUCCESS_NOT_IN_CHALLENGE = 1,
        E_FREEBURN_CHALLENGE_SUCCESS_CONTRIBUTING     = 2,
        E_FREEBURN_CHALLENGE_SUCCESS_DONE             = 3,
        E_FREEBURN_CHALLENGE_SUCCESS_COUNT            = 4,
    };

    static const s32 KI_MAX_TARGETS = 2; // == BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE
    static const s32 KI_MAX_ARCI    = 8; // == E_ACTIVE_RACE_CAR_INDEX_COUNT

    // ---- reconstructed state transitions (X360 out-of-line) ----
    void StartChallenge(const GuiChallengeStartEvent* lpEvent);          // @0x82509D60
    void TriggerChallenge(const GuiChallengeTriggerResponse* lpEvent);   // @0x8250A160
    void HandleNewData(const GuiChallengeUpdateEvent* lpEvent);          // @0x824F3FC8

    // ---- reconstructed accessors (X360 out-of-line) ----
    const BrnResource::ChallengeListEntry*       GetCurrentChallenge() const;          // @0x8240EC30
    const BrnResource::ChallengeListEntryAction* GetCurrentAction() const;             // @0x8240EC88
    EFreeburnChallengeSuccess GetCurrentSuccessForARCI(EActiveRaceCarIndex leARCI) const;  // @0x8240ED50
    f32  GetCurrentContributionForARCI(EActiveRaceCarIndex leARCI) const;              // @0x8240EE50
    f32  GetCurrentContributionOverall() const;                                       // @0x824F4160
    BrnResource::ChallengeListEntryAction::EChallengeDataType GetCurrentTargetType() const; // @0x8240EF50

    // ADDITIVE GROW (PlayerPositionSingle::RenderValue @0x824220C4, which inlines all
    // three; DWARF decls h:148/h:152). IsRunning/IsShowingResults are the two live
    // display states the HUD value renderer draws for.
    bool IsRunning() const        { return meInternalState == E_INTERNAL_STATE_RUNNING; }
    bool IsShowingResults() const { return meInternalState == E_INTERNAL_STATE_RESULTS; }

    // ADDITIVE GROW (CompassComponent::ShowChallengeOnCompass @0x82428CC0, which inlines
    // this as the `meInternalState in {INITIALISED, RUNNING, RESULTS}` branch set -- the
    // manager is "active" once a challenge is initialised through until its results are
    // being shown). DWARF-attested helper (BrnGuiFreeburnChallengeManager DWARF), folded
    // inline by the X360 compiler. The compare set is exactly {2,3,4}.
    bool IsActive() const
    {
        return meInternalState >= E_INTERNAL_STATE_INITIALISED
            && meInternalState <= E_INTERNAL_STATE_RESULTS;
    }

    // FLAG: accessor name not DWARF-attested (the count read is inlined
    // @0x82422100); the member itself is the DWARF miTargetsCount.
    s32 GetTargetsCount() const   { return miTargetsCount; }


private:
    // ---- layout (DWARF order; word offsets X360-verified through miCurrentAction) ----
    GuiCache*                  mpGuiCache;             // :198  @0x00  a1[0]
    EInternalState             meInternalState;        // :199  @0x04  a1[1]
    f32                        mfTimeToNextChange;     // :200  @0x08  a1[2]
    EPageState                 mePageState;            // :201  @0x0C  a1[3]
    s32                        miCurrentTargetIndex;   // :202  @0x10  a1[4]
    const BrnResource::ChallengeListEntry* mpCurrentChallenge; // :205 @0x14 a1[5]
    bool                       mbIsLocalHost;          // :206  @0x18  a1[6]
    s32                        miTargetsCount;         // :207  @0x1C  a1[7]
    BrnResource::ChallengeListEntryAction::EChallengeDataType maeChallengeTargetTypes[KI_MAX_TARGETS]; // :208 @0x20 a1[8..9]
    EFreeburnChallengeSuccess  maaeComplete[KI_MAX_TARGETS][KI_MAX_ARCI];               // :211  @0x28  a1[10..25]
    f32                        maafIndividualTargetContributions[KI_MAX_TARGETS][KI_MAX_ARCI]; // :212 @0x68 a1[26..41]
    s32                        maiOverallTargetRemaining[KI_MAX_TARGETS];               // :213  @0xA8  a1[42..43]
    s32                        miCurrentAction;        // :214  @0xB0  a1[44]
    // :217 mCompletedData (BrnGameState::GameStateModuleIO::FburnChallengeEveryPlayerStatusData)
    // is the trailing member. It is NOT touched by any reconstructed accessor in this
    // slice and its real home is GameSource/GameState/BrnGameStateSharedIO.h; pulling
    // that header graph in here is unwarranted coupling, so the tail is left
    // unmodelled (the struct is never size-asserted). HONEST BOUNDARY.
};

} // namespace BrnGui
