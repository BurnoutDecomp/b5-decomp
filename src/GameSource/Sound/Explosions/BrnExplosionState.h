#ifndef BRN_SOUND_LOGIC_EXPLOSION_BRN_EXPLOSION_STATE_H
#define BRN_SOUND_LOGIC_EXPLOSION_BRN_EXPLOSION_STATE_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnState.h"

// =============================================================================
// BrnSound::Logic::Explosion::ExplosionState
//   GameSource/Sound/Explosions/BrnExplosionState.h (DWARF home) +
//   GameSource/Sound/Explosions/BrnExplosionState.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// ExplosionState is the sound-logic state node for explosion effects. DWARF
// (BrnExplosionState.h:34):
//   struct BrnSound::Logic::Explosion::ExplosionState : public BrnSound::Logic::BrnState
// so it inherits the COMMITTED BrnState base (the CgsSound::Logic::State sound-logic
// state base) reused BY NAME from
// GameSource/Sound/Module/LogicModule/BrnState.h, and overrides the per-class RTTI
// hooks plus the state lifecycle (Attach / Detach / UpdateParams).
//
// This TU's recon'd function set is exactly ONE entry:
//   Attach(void*)  @ 0x826891B8
// whose X360 body is a single argument null-assert: it fires the baked assert
// "lpvAttachment" (BrnExplosionState.cpp:69) when the attachment pointer is null
// and otherwise does nothing observable. (The Hex-Rays `return result` is the
// uninitialised r3 -- the DWARF signature is `virtual void Attach(void*)`.) The
// remaining surface (the RTTI hooks, ctor/dtor, Detach, UpdateParams,
// GetExplosionStateManager) is DEFERRED to its own recon slices (the boot trace
// executed only Attach; declared here only as needed for the state vtable shape).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): Attach touches no member offsets (it
// reads only its argument), so none are asserted here. The committed BrnState base
// (BrnState.h) is reused BY NAME.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Explosion
{

// BrnExplosionState.h:34 (DWARF). Derives the committed BrnState sound-logic state
// base. Only the surface needed for Attach's vtable slot is declared; the remaining
// overrides are DEFERRED (declared where required for the override shape).
struct ExplosionState : public BrnSound::Logic::BrnState
{
    ExplosionState() {}

    // @ 0x826D5740 — vector deleting destructor. The X360 thunk installs
    // ExplosionState's own vtable (off_820AE1F4), calls State::DestroyEffects()
    // to tear down attached effects, re-installs the MemBase base vtable
    // (off_820AA820) as the chain unwinds, and (deleting flavour) routes the
    // storage back through the sound allocator. Observable body = the
    // DestroyEffects() call; the vtable re-installs and allocator free are the
    // compiler-synthesised deleting-destructor parts. Bodied out-of-line in
    // BrnExplosionState.cpp (was an inline no-op).
    virtual ~ExplosionState();

    // — per-class RTTI. DEFERRED bodies (declared for the state vtable shape).
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetTypeInfo() const;
    virtual const char*                                            GetTypeName() const;

    // @ 0x82689198 — per-class static RTTI descriptor accessor. Bodied in the .cpp.
    // typeName "ExplosionState" is inferred from class identity (FLAG: not GetTypeName-proven).
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetStaticTypeInfo();

    // @ 0x826891B8 — null-asserts the attachment pointer ("lpvAttachment"). Bodied
    // in this TU's .cpp. DWARF: virtual void Attach(void*) (BrnExplosionState.cpp:66).
    virtual void Attach(void* lpvAttachment);
};

} // namespace Explosion
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_EXPLOSION_BRN_EXPLOSION_STATE_H
