#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"  // CgsContainers::FastBitArray<2000> (mLocalChallengeCompletionData)

// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h
// ============================================================================
// MINIMAL OWNING SLICE for BrnGameState::ChallengeManager.
//
// SCOPE: this is a deliberately THIN slice. The full ChallengeManager is a very
// large TU (GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.cpp
// in the DecFIGS DWARF: ~90 methods and a wide member layout dominated by per-player
// freeburn-skill scratch arrays, FastBitArray<120>[8] success masks, an ObjectPool of
// leaping data, the road-rules / trigger-query / progression / game-state back-pointers
// and the embedded debug component). Reconstructing all of that is separate future work
// owned by the ChallengeManager TU itself; do NOT grow this header into that shape --
// extend it member-by-member only as concrete callers need each piece.
//
// This header exists so the embedded ChallengeManagerDebugComponent (a by-value member
// of ChallengeManager, and its friend) can reach the three manager data members its
// "Complete all challenges" debug action touches, plus the CheckForOnlineChallengeUnlocks
// call it tail-invokes. The DebugComponent reaches those three members through the named
// accessors below rather than raw offsets (semantic parity by named members, per the
// reconstruction rules); the manager's real byte layout and the accessor bodies land with
// the ChallengeManager TU. The X360 ARTIST asm (CompleteAllChallenges @0x82335010) attests
// the access pattern: mpFreeburnChallengeList @this+0xE38, mpProgression @this+0xE44, and
// the completion bit store @this+0xD00.
//
// FLAG: declare-only additive slice; member offsets / the full layout are out of scope.

namespace BrnResource    { class ChallengeList; }
namespace BrnProgression { class ProgressionManager; }

namespace BrnGameState
{
    class ChallengeManagerDebugComponent;  // embedded by value + friend; full def in BrnChallengeManagerDebugComponent.h

    // The DWARF spells this `struct ChallengeManager` (BrnChallengeManager.h:104). Written as
    // `class` with explicit access sections here; for a non-template type the keyword only
    // changes default access, so callers that use the pointer + the accessors below are
    // unaffected. The keyword/full layout reconcile when the full TU is reconstructed.
    class ChallengeManager
    {
    public:
        // X360 0x82333100. Re-checks online challenge unlock state after a batch of challenge
        // completions; the debug "Complete all challenges" action tail-calls it. DWARF
        // BrnChallengeManager.h:702 -> void(), this-only (the X360 asm passes only r3=this).
        // DECLARATION ONLY: body lands with the ChallengeManager TU.
        void CheckForOnlineChallengeUnlocks();

        // ------------------------------------------------------------------------
        // ADDITIVE GROW (declare-only) for the ChallengeManagerDebugComponent TU.
        // Named accessors for the three members CompleteAllChallenges reads (X360-asm-attested
        // offsets in the file header above). Bodies + the real members land with the
        // ChallengeManager TU. Declare-only suffices for the `cl /c` compile gate.
        // ------------------------------------------------------------------------

        // The loaded freeburn challenge list (X360 *(this+0xE38)); the debug action walks its
        // entries by index and reads each entry's challenge id.
        const BrnResource::ChallengeList* GetFreeburnChallengeList() const;

        // The progression manager (X360 *(this+0xE44)); the debug action routes the per-challenge
        // profile complete + achievement notify through it.
        BrnProgression::ProgressionManager* GetProgressionManager();

        // The local player's completed-challenge bit store (X360 this+0xD00, a FastBitArray<2000>
        // == KI_MAX_PROFILE_CHALLENGES bits). The debug action SetBit()s every challenge index.
        CgsContainers::FastBitArray<2000>& GetLocalChallengeCompletionData();

    private:
        friend class ChallengeManagerDebugComponent;

        // Static (file-scope) "hack" flags the debug menu toggles (DWARF BrnChallengeManager.h:717/718,
        // `extern bool` == static member). When set, the manager treats all challenges as one/two player.
        static bool mbChallengesAreAllOnePlayer;
        static bool mbChallengesAreAllTwoPlayer;
    };
}
