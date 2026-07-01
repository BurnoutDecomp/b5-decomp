#ifndef BRN_SOUND_LOGIC_BRN_STATE_H
#define BRN_SOUND_LOGIC_BRN_STATE_H

#include "types.hpp"

// =============================================================================
// BrnSound::Logic::BrnState
//   GameSource/Sound/Module/LogicModule/BrnState.h (DWARF home) +
//   GameSource/Sound/Module/LogicModule/BrnState.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. BrnState is a sound-logic state
// (one node of the sound state machine). DWARF: BrnState : public State, where
// State is CgsSound::Logic::State : public CgsSound::MemBase.
//
// RE-HOME ATTEMPTED AND REVERTED: the canonical base lives in
// GameShared/GameClasses/Sound/Logic/CgsState.h (status done), and this header
// SHOULD #include it and derive BrnState from CgsSound::Logic::State directly
// (see that TU's DestroyEffects() declaration + the committed
// Streaming::StreamingState::~StreamingState() precedent this TU's destructor
// mirrors). That re-home was tried here and reverted: CgsState.h independently
// (and incompatibly) embeds its OWN BrnSound::Logic::Passby::PassbyState
// definition + a non-aggregate ClassTypeInfo<T> (ctor-based, member `typeName`),
// which collides (struct redefinition + aggregate-init mismatch, C2011/C2027)
// with the separate, already-committed BrnPassbyState.h/.cpp's PassbyState
// (aggregate ClassTypeInfo, member `mpcTypeName`) the instant both headers are
// included together — e.g. via BrnPassbyState.h -> BrnState.h -> CgsState.h.
// Reconciling CgsState.h's embedded Passby/Streaming/Vehicle state classes
// against their own dedicated homes is its own out-of-scope recon slice (touches
// multiple other already-done TUs); NOT fabricated/forked here to route around
// it. Until that reconciliation lands, this header keeps the minimal local
// State/ClassTypeInfo<T> shape (matching CgsState.h's field semantics but NOT
// physically including it) so the 8+ already-committed BrnState derivers
// (BrnGlobalState, BrnCollisionState, BrnExplosionState, BrnEmitterState,
// BrnTrafficState, BrnAIVehicleState, BrnPlayerVehicleState, BrnPassbyState)
// keep compiling standalone.
//
// This TU's ONLY X360-attested function is the scalar deleting destructor
// @0x826C84D8 (class:BrnSound::Logic::BrnState dossier confirmed — no
// GetTypeInfo/GetTypeName override is attested for BrnState itself, so neither
// is declared/bodied here; those per-class RTTI hooks are each derived leaf's
// own concern). The destructor tears down the state's attached effects via the
// inherited State::DestroyEffects() by name (identical pattern to the committed
// sibling CgsState.cpp Streaming::StreamingState::~StreamingState()).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): no member offsets are touched by
// this TU's function (the destructor's only named side effect is the
// DestroyEffects() call, which takes no explicit offset), so no absolute
// offsets are asserted here.
// =============================================================================

namespace CgsSound
{
namespace Logic
{

// Per-class RTTI descriptor. CgsState.h (DWARF). Templated on the leaf class;
// only the shape (id/name/base/factory) is load-bearing here. Field names match
// the aggregate-init usage in the already-committed BrnPassbyState.cpp
// (GetStaticTypeInfo @ 0x82688FC8).
// FLAG: minimal — the owning CgsState/CgsEffectBase RTTI home is not yet
// reconstructed; this is the shape needed to declare BrnState-derived RTTI hooks.
template <typename T>
struct ClassTypeInfo
{
    s32               ObjectID;
    const char*       mpcTypeName;
    ClassTypeInfo<T>* mpBaseTypeInfo;
    T* (*mpfnCreateObject)(u32);
};

// CgsState.h:75 (DWARF): CgsSound::Logic::State : public CgsSound::MemBase.
// Engine sound-logic state base. Only the RTTI virtuals overridden by BrnState's
// derivers are declared here; the full base API is reconstructed in its own home
// (GameShared/GameClasses/Sound/Logic/CgsState.h — NOT included here, see the
// RE-HOME note above).
// FLAG: minimal reconstruction of an un-homed base (MemBase base elided; only
// the polymorphic RTTI surface needed by BrnState's derivers is modelled).
struct State
{
    State() {}
    virtual ~State() {}

    virtual ClassTypeInfo<State>* GetTypeInfo() const;
    virtual const char*           GetTypeName() const;

    // CgsSound::Logic::State::DestroyEffects -- tears down the state's attached effect
    // objects. Called by this TU's destructor (X360 `bl ...DestroyEffects` @0x826C8500)
    // and by GlobalState's destructor (X360 `bl ...DestroyEffects` @0x826D2278).
    // The body is un-homed (a separate sound-logic recon slice; also declared,
    // un-bodied, on the canonical CgsState.h State), so it is declaration-only
    // here -- exists so the destructor can call it BY NAME without fabricating its body.
    // FLAG: declaration-only un-homed base member (do not body here).
    void DestroyEffects();
};

} // namespace Logic
} // namespace CgsSound

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
