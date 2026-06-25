#ifndef CGS_SOUND_LOGIC_CGSENVIRONMENT_H
#define CGS_SOUND_LOGIC_CGSENVIRONMENT_H

#include "types.hpp"

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
// FLAG (minimal, ODR-safe partial view): StateManager has a large owning keystone
// home (CgsStateManager.{h,cpp}); to keep this Environment home self-contained and
// avoid pulling that keystone in, only the GetStateType() accessor StateManager
// exposes here is modelled, BY NAME. This mirrors the existing partial-view pattern
// (CgsStateManagerRegisteredContent.h) -- the views never share a TU. Absolute
// member offsets are deliberately NOT static_asserted (the host 64-bit pointer
// differs in width from the X360 32-bit pointer); only the by-name semantics are
// reproduced.
// =============================================================================

namespace CgsSound
{
namespace Logic
{

const s32 KI_MAX_NUMBER_OF_STATES = 16; // CgsEnvironment.h (DWARF): asserted bound

// Minimal by-name view of the sound-logic StateManager. The full surface lives in
// the owning keystone home; AddStateManager only reads GetStateType().
struct StateManager
{
    s32 GetStateType() const { return miStateType; } // X360 reads at +0x14 (meMapState)

    s32 miStateType;
};

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
