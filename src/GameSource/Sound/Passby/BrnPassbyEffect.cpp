#include "GameSource/Sound/Passby/BrnPassbyEffect.h"

// =============================================================================
// BrnSound::Logic::Passby::Passby3DControl — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnPassbyEffect.h for the
// inheritance rationale and the X360-32-bit-vs-host-64-bit offset note.
//
// This TU's recon'd function set is exactly ONE entry:
//   `scalar deleting destructor'  @ 0x826E8ED0
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Passby
{

// ---------------------------------------------------------------------------
// ~Passby3DControl  @ 0x826E8ED0  (the X360 `scalar deleting destructor')
//
//   bl   Attrib::Instance::~Instance(this + 176)  ; destroy mEngineDataAtrib
//   li   r7, 3 ; stw r7, 0x24(this)               ; meDetachState = FINISHED
//   stw  &off_820AA820, 0(this)                   ; primary vptr settle
//   stb  0, 0x2D(this)                            ; control bookkeeping flag = false
//   stw  0, 0x20(this)                            ; mfDeltaTime = 0
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound allocator) }
//   return this
//
// The Attrib::Instance member teardown (mEngineDataAtrib) is produced by the
// inherited ~Brn3DEffectControl destructor chain, which destroys the member via
// the committed Attrib::Instance::~Instance. The remaining stores settle members
// owned by the inherited bases, so this leaf destructor body adds nothing of its
// own.
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to
// free the object; that allocator is not homed here, so operator-delete dispatch
// is left to the host toolchain (the `delete` half of the X360 scalar deleting
// destructor) rather than reproducing the raw allocator vtable call.
// ---------------------------------------------------------------------------
Passby3DControl::~Passby3DControl()
{
}

// ---------------------------------------------------------------------------
// ~PassbyEffect  @ 0x826C9200  (the X360 `scalar deleting destructor')
//
//   bl   ~PassbyEffect                   ; run the (leaf, empty-here) destructor
//   if (r30 & 1) {                       ; deleting flavour
//       memset(&v5[1], 0, 16) ; v5[0] = this
//       (*(*off_82FFB954 + 0x14))(off_82FFB954, v5)   ; sound allocator Free(this)
//   }
//   return this
//
// The single observable source-level side effect is the ~PassbyEffect() call, which
// forwards to the inherited BrnEffectObject base destructor chain (committed at
// GameSource/Sound/Module/LogicModule/BrnEffectObject.h) -- no member stores are
// attested inline at this address. The (a2 & 1) tail routes the object through the
// global sound allocator (off_82FFB954, vtable slot +0x14 == Free); that allocator is
// not homed in this group, so the `delete` half is left to the host toolchain's
// virtual-destructor + operator-delete lowering -- identical treatment to the committed
// Passby3DControl (0x826E8ED0) and ExplosionEffect (0x826E8F78) scalar deleting
// destructors.
// ---------------------------------------------------------------------------
PassbyEffect::~PassbyEffect()
{
}

} // namespace Passby
} // namespace Logic
} // namespace BrnSound
