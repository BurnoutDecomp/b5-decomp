#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"  // CgsContainers::FastBitArray<2000> (mLocalChallengeCompletionData accessor return)
#include "SharedClasses/DataLists/ChallengeListEntry.h"         // BrnResource::ChallengeListEntry (GetChallengeStyle return enum + mpCurrentChallenge->GetChallengeStyle())

// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h
// ============================================================================
// OWNING HEADER for BrnGameState::ChallengeManager -- the top-level CHALLENGE-mode
// manager that owns + drives the online/freeburn challenge subsystem (challenge tracking,
// per-player skill scoring, arbitration, results, network player book-keeping). This grows
// the previously-committed THIN slice (which existed only so the embedded
// ChallengeManagerDebugComponent could reach three members) into the type's real home,
// per the explicit hand-off in that slice's header ("the manager's real byte layout and the
// accessor bodies land with the ChallengeManager TU").
//
// LAYOUT MODEL -- OPAQUE ATTESTED-OFFSET STORAGE (CLASS-TU DWARF-GAP rule):
//   The DecFIGS DWARF (BrnChallengeManager.h) DOES list named members, but the type is
//   dominated by nested aggregates whose sizes are NOT reconstructable in this tree
//   (ObjectPool<StoredLeapingData,7>, FastBitArray<120>[8], CompletedFburnChallenges,
//   FburnChallengeSuccessUpdateAction::LastSecondChallengeSuccess, the embedded
//   ChallengeManagerDebugComponent). The X360 ARTIST asm reaches EVERY member by raw byte
//   offset over `this`. Reproducing a named C++ layout that hits those offsets would require
//   fabricating the interiors of the unhomed aggregates. So `this` is modelled as OPAQUE
//   attested-offset storage: a single u8 buffer sized ABOVE the highest bodied offset, and
//   the asm-recoverable methods are bodied as the asm's exact byte-offset arithmetic over the
//   buffer (the opaque-external-field-by-attested-offset allowance). NO named members are
//   fabricated. The attested offsets the bodied methods touch are listed in BrnChallengeManager.cpp.
//
// SCOPE: a faithful FOUNDATION. The lifecycle/book-keeping methods that touch only `this`
// (plus already-recovered free functions / homed accessors) are bodied; the remainder
// (network-reaching, VMX skill pipelines, methods that raw-index unhomed aggregate interiors)
// are DECLARATION-ONLY + FLAGGED. Bodied vs declaration-only is reported in the .cpp.

namespace BrnResource
{
    class  ChallengeList;
    struct ChallengeListEntry;        // mpCurrentChallenge points at one; GetNumPlayers() used by the bodied counters
}
namespace BrnProgression { class ProgressionManager; }

namespace BrnGameState
{
    class ChallengeManagerDebugComponent;  // embedded by value + friend; full def in BrnChallengeManagerDebugComponent.h

    // ------------------------------------------------------------------------
    // Nested enums (DWARF BrnChallengeManager.h:53 / :63). These are the type's own; not
    // defined anywhere else in the committed tree.
    // ------------------------------------------------------------------------

    // BrnChallengeManager.h:53 -- the manager's top-level state machine.
    enum EChallengeManagerStatus
    {
        E_CHALLENGE_MANAGER_STATUS_NONE    = 0,
        E_CHALLENGE_MANAGER_STATUS_PENDING = 1,
        E_CHALLENGE_MANAGER_STATUS_RUNNING = 2,
        E_CHALLENGE_MANAGER_STATUS_RESULTS = 3,
        E_CHALLENGE_MANAGER_STATUS_COUNT   = 4,
    };

    // BrnChallengeManager.h:63 -- the 19 freeburn skill categories a challenge action can score.
    // NOTE: START and ONCOMING alias 0 in the DWARF (the enumerator order is preserved verbatim).
    enum EFreeburnSkill
    {
        E_FREEBURN_SKILL_START                       = 0,
        E_FREEBURN_SKILL_ONCOMING                    = 0,
        E_FREEBURN_SKILL_FLATSPIN                    = 1,
        E_FREEBURN_SKILL_FLATSPIN_REVERSE            = 2,
        E_FREEBURN_SKILL_DRIFT                       = 3,
        E_FREEBURN_SKILL_NEAR_MISS                   = 4,
        E_FREEBURN_SKILL_SUCCESSFUL_LANDING          = 5,
        E_FREEBURN_SKILL_SUCCESSFUL_LANDING_REVERSE  = 6,
        E_FREEBURN_SKILL_ROAD_RULE_TIME              = 7,
        E_FREEBURN_SKILL_ROAD_RULE_CRASH             = 8,
        E_FREEBURN_SKILL_BARREL_ROLL                 = 9,
        E_FREEBURN_SKILL_BARREL_ROLL_REVERSE         = 10,
        E_FREEBURN_SKILL_PLAYER_POWER_PARKING        = 11,
        E_FREEBURN_SKILL_TRAFFIC_POWER_PARKING       = 12,
        E_FREEBURN_SKILL_BURNOUTS                    = 13,
        E_FREEBURN_SKILL_BOOST_TIME                  = 14,
        E_FREEBURN_SKILL_AIR                         = 15,
        E_FREEBURN_SKILL_AIR_DISTANCE                = 16,
        E_FREEBURN_SKILL_BILLBOARDS                  = 17,
        E_FREEBURN_SKILL_LEAP_CARS                   = 18,
        E_FREEBURN_SKILL_COUNT                       = 19,
    };

    // ========================================================================
    // BrnGameState::ChallengeManager (DWARF spells it `struct`, no base class).
    //
    // The DWARF orders the members but their cumulative byte offsets cannot be reproduced
    // here (unhomed nested-aggregate sizes), so the object is OPAQUE attested-offset storage.
    // KU_CHALLENGE_MANAGER_SIZE is pinned ABOVE the highest offset any bodied method touches
    // (the embedded ChallengeManagerDebugComponent sits at X360 +0x1004 and the two trailing
    // "were all N-player" flag bytes at +0x101C/+0x101D -> the object spans to 0x101E; rounded
    // up to a 16-byte boundary). The class keyword stays `class` (matching the committed thin
    // slice) -- for a non-template type the keyword only changes default access.
    // ========================================================================
    class ChallengeManager
    {
    public:
        // X360 0x82332DB0. Lifecycle construct: zero/initialise the per-frame + per-player
        // scratch state, run the two clear helpers, assert the five injected back-pointers are
        // non-null and cache them. EXECUTED in the boot-trace milestone. Bodied by THIS TU.
        // Signature recovered from the X360 asm argument registers (r3=this then the five
        // pointers in r4..r8 -- lpGameStateModule, lpModeManager, lpProgression,
        // lpRoadRulesManager, lpTriggerQueryManager).
        void Construct(void* lpGameStateModule,
                       void* lpModeManager,
                       void* lpProgression,
                       void* lpRoadRulesManager,
                       void* lpTriggerQueryManager);

        // X360 0x82323408. Age the post-challenge results timer one frame; returns true once it
        // has expired. Pure `this`-offset arithmetic. Bodied by THIS TU.
        bool UpdateResultsTimer(f32 lfTimeStep);

        // X360 0x82316DE8. True once every challenge player has sent its success update this
        // round (compares the per-player received-update byte flags against the current
        // challenge's player count). Bodied by THIS TU.
        bool ReceivedSuccessUpdatesFromAllPlayers();

        // X360 0x82316F00 / 0x82317020. Count, for the given action slot, how many players are
        // currently SUCCEEDING (status==3) / CONTRIBUTING (status==2) in the per-player success
        // status grid. Bodied by THIS TU.
        s32 GetNumPlayerSucceeding(s32 liActionIndex);
        s32 GetNumPlayersContributing(s32 liActionIndex);

        // X360 0x82333100. Re-checks online challenge unlock state after a batch of completions;
        // the debug "Complete all challenges" action tail-calls it. DECLARATION ONLY (its body
        // walks the unhomed progression/challenge-list interiors -- FLAGGED, own slice).
        void CheckForOnlineChallengeUnlocks();

        // X360 0x82355FA8 (DWARF BrnChallengeManager.h:196). The active challenge's freeburn style,
        // or E_FREEBURN_STYLE_NONE unless the manager is RUNNING. Pure `this`-offset read plus a
        // forward to mpCurrentChallenge->GetChallengeStyle(). Bodied by THIS TU.
        BrnResource::ChallengeListEntry::EFreeburnChallengeStyle GetChallengeStyle() const;

        // ----- Named accessors the committed ChallengeManagerDebugComponent reaches (X360-asm-
        // attested offsets; bodied here over the opaque storage). -------------------------------
        const BrnResource::ChallengeList*    GetFreeburnChallengeList() const;   // X360 *(this+0xE38)
        BrnProgression::ProgressionManager*  GetProgressionManager();            // X360 *(this+0xE44)
        CgsContainers::FastBitArray<2000>&   GetLocalChallengeCompletionData();  // X360  this+0xD00

    private:
        friend class ChallengeManagerDebugComponent;

        // X360 0x8233E530. Count the set bits in the local completion bit array
        // (mLocalChallengeCompletionData @ +0xD00, a FastBitArray<2000> == 32 u64 fields). The X360
        // body is the container's set-bit iterator fully inlined (32 fields / 2000-bit bound);
        // reconstructed here as the equivalent GetFirstBitSet/GetNextBitSet scan. Bodied by THIS TU.
        s32 CountCompletedChallenges();

        // X360 0x82334740. Push a completed stunt run's per-skill tallies into the current-frame skill
        // scores: for each non-zero field of the incoming stunt-score struct, call SetCurrentSkillScore
        // with the matching freeburn-skill id. Bodied by THIS TU.
        void UpdateStuntScores(const void* lpStuntScoreInfo);

        // DWARF BrnChallengeManager.h:686. Latches one freeburn-skill score for the current frame.
        // DECLARATION ONLY here (its body -- the per-skill scratch arrays + VMX -- is its own slice);
        // UpdateStuntScores calls it.
        void SetCurrentSkillScore(EFreeburnSkill leSkill, f32 lfScore, bool lbScoredThisFrame);

        static void _AssertLayout();   // never called; documents the attested offsets the bodies use

        // OPAQUE attested-offset storage. Size pinned above the highest bodied offset (the
        // embedded debug component @+0x1004 and the +0x101C/+0x101D flag bytes).
        static const u32 KU_CHALLENGE_MANAGER_SIZE = 0x1020;
        u8 maOpaque[KU_CHALLENGE_MANAGER_SIZE];

        // Static (file-scope) "hack" flags the debug menu toggles (DWARF BrnChallengeManager.h:717/718,
        // `extern bool` == static member). The X360 backs them with the byte_82FAD9C0/C1 globals.
        static bool mbChallengesAreAllOnePlayer;
        static bool mbChallengesAreAllTwoPlayer;
    };
}
