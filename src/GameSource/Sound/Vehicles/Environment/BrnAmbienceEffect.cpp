#include "GameSource/Sound/Vehicles/Environment/BrnAmbienceEffect.h"

// =============================================================================
// BrnSound::Vehicles::Environment::AmbienceEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnAmbienceEffect.h for the
// BrnEffectObject dual-base reuse rationale + the deferred +0x38 third-sub-object FLAG.
// Recon'd function set:
//   AmbienceEffect()   @ 0x826B9600 (leaf constructor)
//   ~AmbienceEffect    (out-of-line anchor -- key function / leaf vtable emission point)
//   CreateObj(u32)     @ 0x826D0CC0 (the effect-object factory hook)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

// ---------------------------------------------------------------------------
// AmbienceEffect::AmbienceEffect  @ 0x826B9600
//
// Same dual-base shape as the committed siblings FxEffect / CollisionEffect: MSVC's
// INLINED full-object constructor installs the BrnEffectObject dual-vptr pair directly
// and inlines the base member zero-inits. In reconstructed C++ those installs + base
// zero-inits are produced implicitly by the BrnEffectObject base sub-object's own
// default constructor (reused BY NAME).
//
// FLAG (third polymorphic sub-object base @ +0x38 NOT modelled): beyond the
// BrnEffectObject dual base this ctor ALSO installs a THIRD vptr @ +0x38 and zero-seeds
// that sub-object's member words (+0x3C..+0x64) plus a -1 sentinel @ +0x68 -- an
// ADDITIONAL embedded polymorphic sub-object whose class is UN-RECOVERED. It is
// DECLARATION-ONLY / DEFERRED (this leaf is layout-incomplete vs the X360); NOT
// fabricated as a named base. The pre-+0x38 leaf scalar zero-inits are likewise
// un-homed and deferred.
// ---------------------------------------------------------------------------
AmbienceEffect::AmbienceEffect()
    : BrnSound::Logic::BrnEffectObject()   // installs the base vptrs + zero-inits base members (BY NAME)
{
    // The remaining inlined stores target AmbienceEffect's OWN un-homed leaf scalars
    // and the deferred +0x38 polymorphic sub-object base. DECLARATION-ONLY; see FLAGs.
}

// ---------------------------------------------------------------------------
// ~AmbienceEffect  (the leaf destructor / vtable emission point off_820B1F80). Its full
// member-teardown slice is DEFERRED; this out-of-line anchor forwards to the inherited
// BrnEffectObject base destructor chain (BY NAME). It exists so the class has a defined
// key function -- identical treatment to the committed sibling FxEffect::~FxEffect.
// (Without this definition the leaf vtable the ctor installs would be an undefined
// reference at link time -- the LINK DEFECT the reviewer flagged.)
// ---------------------------------------------------------------------------
AmbienceEffect::~AmbienceEffect()
{
}

// ---------------------------------------------------------------------------
// AmbienceEffect::CreateObj(u32)  @ 0x826D0CC0   (the effect-object factory hook)
//
// The X360 allocates a 112-byte (0x70) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "AmbienceEffect" and placement-constructs an
// AmbienceEffect, returning the IResourceRequester SUB-OBJECT pointer (this+4 per the
// BrnEffectObject dual base -- the `addi r3,r3,4`). The `a1` argument only selects the
// operator-new flavour (0/1).
// FLAG (allocator gate): CgsSound::MemBase::operator new is not homed here, so this uses
// the host `new`; the +4 adjust is the static_cast to the IResourceRequester base. The
// 0x70 size is documentation only.
// ---------------------------------------------------------------------------
BrnSound::Logic::IResourceRequester* AmbienceEffect::CreateObj( u32 /*luFlavour*/ )
{
    return static_cast<BrnSound::Logic::IResourceRequester*>( new AmbienceEffect() );
}

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound
