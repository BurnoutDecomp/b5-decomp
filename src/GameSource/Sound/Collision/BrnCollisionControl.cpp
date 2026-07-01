#include "GameSource/Sound/Collision/BrnCollisionControl.h"

// =============================================================================
// BrnSound::Logic::Collision::CollisionControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF (BrnCollisionControl.h:38):
//   struct CollisionControl : public BrnSound::Logic::BrnEffectControl
//
// This TU's recon'd function set is exactly two entries:
//   CreateObject(u32)              @ 0x826D34B0
//   `vector deleting destructor'   @ 0x826BD4D8
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

// ---------------------------------------------------------------------------
// CollisionControl::CreateObject(u32)  @ 0x826D34B0   (the factory hook)
//
//   if ( a1 ) { if ( MemBase::operator new(176, "CollisionControl", 1) ) return new'd ctor+4; }
//   else      { if ( MemBase::operator new(176, "CollisionControl", 0) ) return new'd ctor+4; }
//   return 0;
//
// The X360 allocates a 176-byte (0xB0) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "CollisionControl" and inline-constructs a
// CollisionControl. Both arms call the SAME size+ctor; the `a1` argument only selects
// the operator-new flavour (0/1). The returned pointer is the constructed object
// upcast to CgsSound::Logic::EffectControl* (the compiler synthesises the base-ptr
// adjustment for `new CollisionControl()`).
//
// FLAG (allocator gate): CgsSound::MemBase does NOT model operator new(size, tag,
// flavour) (off_82FFB954 sound allocator not homed in this group), so a faithful
// placement-new is not yet expressible. This uses the host global `new`; the
// observable result -- a constructed CollisionControl* (or null) handed back as the
// base EffectControl* -- matches. Replace with the sound-allocator placement-new once
// MemBase::operator new is homed. The 0xB0 size is documentation of the X360 inline
// ctor; the zero-init is done by CollisionControl's (deferred) default constructor.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectControl* CollisionControl::CreateObject( u32 /*luType*/ )
{
    return new CollisionControl();
}

// ---------------------------------------------------------------------------
// ~CollisionControl  (the out-of-line anchor the X360 `vector deleting destructor'
// @ 0x826BD4D8 forwards to). The vector deleting destructor is MSVC's compiler-
// synthesised thunk: it chains this virtual destructor and, when bit0 of its flags
// arg is set, frees the object through the global sound MemBase allocator
// (off_82FFB954, vtable slot +0x14). The observable member teardown lives in the
// inherited ~BrnEffectControl base chain (which settles meAttachState/meDetachState/
// mbResourcesReady + the dual-base vptrs), so this leaf body is empty. It exists so
// the class has a defined key function (the vtable emission point), mirroring the
// committed CollisionStateManager / CollisionEffect precedent; the deleting-destructor
// thunk + the delete-half are re-synthesised by the host toolchain from this virtual
// destructor + operator delete (no fabricated allocator).
// ---------------------------------------------------------------------------
CollisionControl::~CollisionControl()
{
}

} // namespace Collision
} // namespace Logic
} // namespace BrnSound
