#ifndef BRN_SOUND_LOGIC_BRN_STATE_H
#define BRN_SOUND_LOGIC_BRN_STATE_H

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Logic/CgsClassTypeInfo.h"  // ClassTypeInfo<T> (canonical)
#include "GameShared/GameClasses/Sound/Logic/CgsState.h"          // CgsSound::Logic::State (the canonical base)

// =============================================================================
// BrnSound::Logic::BrnState
//   GameSource/Sound/Module/LogicModule/BrnState.h (DWARF home) +
//   GameSource/Sound/Module/LogicModule/BrnState.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. BrnState is a sound-logic state
// (one node of the sound state machine). DWARF: BrnState : public State, where
// State is CgsSound::Logic::State : public CgsSound::MemBase.
//
// RE-HOME DONE (2026-08-25, audio-faithfulness wave 5): this header used to carry
// a minimal local rival CgsSound::Logic::State (the canonical CgsState.h could not
// be included while it embedded its own rival Passby/Streaming/Vehicle state
// leaves -- the C2011 collision its old "RE-HOME ATTEMPTED AND REVERTED" note
// recorded). Those leaves now live only in their DWARF homes
// (BrnPassbyState.h / BrnStreamingState.h / BrnVehicleState.h), so BrnState
// derives the REAL canonical State (members, IsAttached(), DestroyEffects()/
// Attach() declarations, RTTI virtuals) via the include above.
//
// This TU's ONLY X360-attested function is the scalar deleting destructor
// @0x826C84D8 (class:BrnSound::Logic::BrnState dossier confirmed — no
// GetTypeInfo/GetTypeName override is attested for BrnState itself, so neither
// is declared/bodied here; those per-class RTTI hooks are each derived leaf's
// own concern). The destructor tears down the state's attached effects via the
// inherited State::DestroyEffects() by name.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): no member offsets are touched by
// this TU's function (the destructor's only named side effect is the
// DestroyEffects() call, which takes no explicit offset), so no absolute
// offsets are asserted here.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// BrnState.h:60 (DWARF): BrnState : public State.
struct BrnState : public CgsSound::Logic::State
{
    BrnState() {}
    virtual ~BrnState();
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_STATE_H
