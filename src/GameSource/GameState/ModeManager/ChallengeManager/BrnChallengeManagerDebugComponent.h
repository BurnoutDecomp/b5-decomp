#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // CgsDev::DebugComponent (real base)

// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h
// ============================================================================
// BrnGameState::ChallengeManagerDebugComponent -- the in-game debug menu for the freeburn
// challenge manager. Derives from the real CgsDev::DebugComponent and is embedded by value
// inside (and is a friend of) BrnGameState::ChallengeManager. It registers a couple of
// "make all challenges N-player" toggles, a challenge-index selector + a couple of actions
// (start the selected challenge, complete every challenge) with the debug UI.
//
// SHAPE + member order are DWARF-authoritative (DecFIGS dwarfdump
// GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h:52/89/91/92) and
// reproduce the X360 object layout: the CgsDev::DebugComponent base sub-object occupies
// +0x00..+0x0B (vtable@0, mbActive@4, mpDebugLinkedListNext@8); then
//   mpChallengeManager           = +0x0C  (X360 OnActivate `lwz r11,0xC(r31)` / CompleteAllChallenges `lwz r26,0xC(r31)`)
//   miChallengeIndex             = +0x10  (X360 OnActivate `addi r30,r31,0x10` -> the RegisterVariable/SetRange target)
//   mbDebugBeginChallengePending = +0x14
//
// SCOPE: only the three functions this TU's X360 ledger attests are bodied here
// (CompleteAllChallenges, GetName, OnActivate). The other DWARF-listed methods
// (Construct/Destruct/RenderHUD/GetPath/StartChallenge/DebugStartChallenge) are owned by
// their own passes and are intentionally NOT declared here (gate every DWARF declaration on
// X360 attestation -- declaring them now would force unimplemented vtable slots / link refs).

namespace BrnGameState
{
    class ChallengeManager;  // pointer member only; full def in BrnChallengeManager.h

    class ChallengeManagerDebugComponent : public CgsDev::DebugComponent
    {
    protected:
        // Menu label (X360 0x823171E0). Overrides CgsDev::DebugComponent::GetName.
        const char* GetName() const override;        // @ BrnChallengeManagerDebugComponent.cpp:69

        // Registers this component's tweakables + actions with the debug menu (X360 0x8233EA18).
        void OnActivate() override;                  // @ BrnChallengeManagerDebugComponent.cpp:93

        // The "Start Challenge" debug action callback (registered with `this` as user-data, so STATIC).
        // DECLARATION ONLY here: its body is owned by a separate pass (DWARF cpp:114) and is NOT in this
        // TU's X360 ledger, so it is referenced (its address is taken in OnActivate) but not defined here
        // -- the body resolves at link from its own TU.
        //
        // ICF NOTE: in the X360 ARTIST build OnActivate's "Start Challenge" callback address resolves to
        // CgsDev::DebugComponentPerfMonCpu::DebugCallbackResetCounters (0x82817350) -- i.e. this static
        // wrapper was identical-code-folded onto that symbol. The DWARF (cpp:114) names the real callback
        // StartChallenge, which is what the original C++ registered; we register StartChallenge here and
        // treat the folded-symbol resolution as the ICF artifact it is.
        static void StartChallenge(void* lpContext); // @ BrnChallengeManagerDebugComponent.cpp:114 (own pass)

        // The "Complete all challenges" debug action callback (X360 0x82335010). Registered with the
        // debug menu as a plain DebugCallbackFunction with `this` handed back as the user-data, so it
        // is STATIC (the void* context IS the component) -- the X360 asm loads its bare address into
        // the RegisterFunction callback arg and passes the component as a separate user-data arg, which
        // a non-static member could not be. (The DecFIGS DWARF lists it as a plain member; the X360
        // calling convention proves the static-callback shape.) Marks every challenge in the freeburn
        // list as completed on the local profile (notifying achievements for the freshly-completed
        // ones) and sets the matching bit in the local completion store, then re-checks online unlocks.
        static void CompleteAllChallenges(void* lpContext); // @ BrnChallengeManagerDebugComponent.cpp:140

    private:
        // Layout (DWARF offsets above). Access by name, never by raw offset.
        ChallengeManager* mpChallengeManager;        // +0x0C
        s32               miChallengeIndex;          // +0x10
        bool              mbDebugBeginChallengePending; // +0x14
    };
}
