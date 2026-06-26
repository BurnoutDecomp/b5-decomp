#ifndef CGS_SOUND_LOGIC_CGSENVIRONMENT_H
#define CGS_SOUND_LOGIC_CGSENVIRONMENT_H

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Logic/CgsStateManager.h"  // CgsSound::Logic::StateManager (CANONICAL)

// =============================================================================
// CgsSound::Logic::Environment
//   GameShared/GameClasses/Sound/Logic/CgsEnvironment.h (DWARF home) +
//   GameShared/GameClasses/Sound/Logic/CgsEnvironment.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The sound-logic Environment owns a
// map of StateManager* keyed by state type. AddStateManager (the one function homed
// here) registers a manager into that map.
//
// X360 offset map proven by AddStateManager @ 0x82680D60 (over the 32-bit layout):
//   Environment:
//     +0x00 -> mapStateManagers[]   (StateManager*; the routine indexes the array
//                                     at [GetStateType()+1] -- (stateType+1)*4 bytes
//                                     off `this`).
//   StateManager (read by name only):
//     GetStateType() == the manager's map-state, read at +0x14 in the X360 layout.
//
// The asserts cite CgsEnvironment.h:475 (lpStateManager non-null),
// :476 (GetStateType() < KI_MAX_NUMBER_OF_STATES) and
// :477 (mapStateManagers[GetStateType()] == NULL).
//
// ODR FOLD (2026-06-25): this header previously declared its OWN minimal
// `struct CgsSound::Logic::StateManager { s32 GetStateType(); s32 miStateType; }`
// -- a placeholder that would clash with the canonical CgsSound::Logic::StateManager
// (CgsStateManager.h) the moment a TU needed both (which the SoundLogicModule wiring
// now does: BrnSoundLogicModule embeds Environment BY VALUE *and* calls
// StateManager::CreateStateMan, returning the canonical StateManager*). That minimal
// struct is now DELETED; this header includes the canonical CgsStateManager.h and
// Environment stores canonical CgsSound::Logic::StateManager*. The fold is clean
// because AddStateManager/GetStateManager only ever call GetStateType(), which the
// canonical class already exposes (returning meMapState @ +0x14) -- the exact member
// the old minimal view modelled. Same fold pattern already applied to
// BrnStateManager.h. Absolute member offsets remain NOT static_asserted (host 64-bit
// pointer width differs from the X360 32-bit pointer); only the by-name semantics
// (indexing mapStateManagers[GetStateType()+1]) are load-bearing.
// =============================================================================

namespace CgsSound
{
namespace Logic
{

const s32 KI_MAX_NUMBER_OF_STATES = 16; // CgsEnvironment.h (DWARF): asserted bound

struct Environment
{
    Environment()
    {
        for (s32 liSlot = 0; liSlot < KI_MAX_NUMBER_OF_STATES + 1; ++liSlot)
            mapStateManagers[liSlot] = nullptr;
    }

    // @ 0x82680D60. Register a StateManager into the map keyed by its state type.
    // Returns true (the X360 returns li r3,1 on every path).
    bool AddStateManager(StateManager* apStateManager);

    // @ 0x8268D1C0. Fetch the StateManager registered for state id liStateManId
    // (the X360 indexes mapStateManagers[liStateManId + 1], same +1 reserved-slot
    // convention as AddStateManager). Asserts liStateManId < KI_MAX_NUMBER_OF_STATES
    // (CgsEnvironment.cpp:481). Returns the slot (may be null if nothing registered).
    // Callers: DynamicMixer::GetStateCount/ConnectDMixIO, Environment::Notify.
    StateManager* GetStateManager(s32 liStateManId) const;

    // The X360 indexes the array at [GetStateType()+1] (the (stateType+1)*4-byte
    // displacement off `this`); slot 0 of the map is reserved. The array is sized
    // KI_MAX_NUMBER_OF_STATES+1 so the highest valid state index (15) lands in-bounds.
    StateManager* mapStateManagers[KI_MAX_NUMBER_OF_STATES + 1];
};

} // namespace Logic
} // namespace CgsSound

#endif // CGS_SOUND_LOGIC_CGSENVIRONMENT_H
