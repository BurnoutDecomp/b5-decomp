#include "GameSource/Sound/Module/LogicModule/BrnEmitterStateManager.h"

// =============================================================================
// BrnSound::Logic::World::EmitterStateManager — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnEmitterStateManager.h for the
// CgsSound::Logic::StateManager-descendant shape and the offset/ODR notes.
//
// This TU's recon'd function set is exactly two entries:
//   EmitterStateManager()    (the ctor)                       @ 0x826FE290
//   ~EmitterStateManager()   (the `vector deleting destructor`) @ 0x826FE2E0
//
// dep_flags: depends on two DONE TUs, called BY NAME / structurally:
//   * CgsSound::Logic::StateManager::StateManager @ 0x826FAA18 — the base ctor the
//     X360 ctor `bl`s into; reproduced here as the implicit base-class construction.
//   * ObjectPool<StateManager::RegisteredContent,4,int>::~ObjectPool @ 0x826EAC90 —
//     the member-pool destructor the X360 dtor calls on the +0x30 sub-object;
//     reproduced here as the implicit member destruction of maContentPool.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace World
{

// ---------------------------------------------------------------------------
// EmitterStateManager  @ 0x826FE290
//
//   bl   CgsSound::Logic::StateManager::StateManager   ; base ctor
//   stw  off_820AB608, 0x90(this)                      ; (transient) secondary vtable
//   stw  off_820B81C0, 0(this)                         ; primary vtable
//   stw  off_820B81B8, 0x90(this)                      ; secondary vtable (final)
//   stw  off_820B1738, 0x98(this)                      ; secondary vtable (final)
//   return this
//
// The four vtable stores are the compiler's devirtualisation of the base/derived and
// secondary sub-object vptrs; in reconstructed C++ they are produced implicitly by the
// construction of a polymorphic class deriving from CgsSound::Logic::StateManager, so
// the BODY here is empty — the only observable work is the base construction (handled
// by the member-init list / implicit base ctor) and the member-pool default
// construction (maContentPool).
// ---------------------------------------------------------------------------
EmitterStateManager::EmitterStateManager()
    : CgsSound::Logic::StateManager()
{
}

// ---------------------------------------------------------------------------
// ~EmitterStateManager  @ 0x826FE2E0  (the X360 `vector deleting destructor`)
//
//   stw  off_820B66E4, 0(this)                                  ; (transient) vtable
//   bl   ObjectPool<RegisteredContent,4,int>::~ObjectPool(this+0x30) ; destruct pool
//   stw  off_820AA820, 0(this)                                  ; MemBase vtable
//   if (a2 & 1) { ... deallocate via off_82FFB954 (the MemBase allocator) }
//   return this
//
// The pool teardown at +0x30 and the vtable re-installs are produced implicitly by the
// destructor chain: destructing maContentPool runs its ~ObjectPool (and therefore each
// RegisteredContent reference drop), which is exactly the X360 sub-object destructor
// call. The BODY here is therefore empty.
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free the
// object; that allocator is not homed here, so operator-delete dispatch is left to the
// host toolchain (the `delete` half of the X360 vector deleting destructor).
// ---------------------------------------------------------------------------
EmitterStateManager::~EmitterStateManager()
{
}

} // namespace World
} // namespace Logic
} // namespace BrnSound
