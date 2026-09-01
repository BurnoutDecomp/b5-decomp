#include "GameSource/Sound/Module/LogicModule/Brn3DUserSpaceEffectControl.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/matrix44affine_operation.h"

// =============================================================================
// BrnSound::Logic::Brn3DUserSpaceEffectControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF (BrnEffectControl.h:158):
//   struct Brn3DUserSpaceEffectControl : public BrnSound::Logic::Brn3DEffectControl
// so it inherits the committed Brn3DEffectControl base, which owns the
// Attrib::Instance member (mEngineDataAtrib) and the EffectBase meDetachState/
// meAttachState fields that the destructor tears down.
//
// This TU's recon'd function set is exactly two entries:
//   CreateObject(u32)              @ 0x826E0E10
//   `vector deleting destructor'   @ 0x826E0D60
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// Brn3DUserSpaceEffectControl::CreateObject(u32)  @ 0x826E0E10  (the RTTI factory hook)
//
//   if ( a1 ) { if ( MemBase::operator new(288, "Brn3DUserSpaceEffectControl", 1) )
//                   return new'd ctor; }
//   else      { if ( MemBase::operator new(288, "Brn3DUserSpaceEffectControl", 0) )
//                   return new'd ctor; }
//   return 0;
//
// The X360 allocates a 288-byte (0x120) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "Brn3DUserSpaceEffectControl" and placement-
// constructs a Brn3DUserSpaceEffectControl into it. Both arms call the SAME size +
// SAME ctor; the `a1` argument only selects the operator-new flavour (0/1) and is
// never read as `this` nor forwarded to the ctor. On allocation failure both arms
// fall through to `li r3,0; blr`. Mirrors the committed BrnStateManager /
// EmitterStateManager CreateObject bodies exactly.
//
// FLAG (allocator gate): CgsSound::MemBase (CgsMemBase.h) does NOT model operator
// new(size, tag, flavour) -- the sound allocator (off_82FFB954) is not homed in this
// group -- so a faithful placement-new is not yet expressible. This uses the host
// global `new`; the observable result -- a constructed Brn3DUserSpaceEffectControl*
// (or null) handed back as the base CgsSound::Logic::EffectControl* -- matches.
// Replace with the sound-allocator placement-new once MemBase::operator new is homed.
// The 288-byte size is the X360 0x120; the host object differs in size, so the literal
// is documentation only and is NOT passed to host new.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectControl* Brn3DUserSpaceEffectControl::CreateObject( u32 /*luType*/ )
{
    return new Brn3DUserSpaceEffectControl();
}

// ---------------------------------------------------------------------------
// ~Brn3DUserSpaceEffectControl  @ 0x826E0D60  (the X360 `vector deleting destructor')
//
//   stw  off_820B3670, 0(this)                ; leaf vptr install
//   Attrib::Instance::~Instance(this + 0xB0)   ; destroy mEngineDataAtrib (base-owned)
//   *(this+0x24) = 3                           ; meDetachState = E_DETACH_STATE_FINISHED
//   *(this+0)    = &off_820AA820               ; base sub-object vptr settle
//   *(this+0x2D) = 0                           ; (control bookkeeping flag) = false
//   *(this+0x20) = 0                           ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound allocator) }
//   return this
//
// EVERY store above settles a member owned by the inherited Brn3DEffectControl base:
// the Attrib::Instance member teardown (+0xB0) and the meDetachState/meAttachState
// writes are produced by the inherited ~Brn3DEffectControl destructor chain, and the
// vptr installs are the compiler-emitted sub-object devirtualisation. Therefore this
// LEAF destructor body adds NOTHING of its own -- bodying meDetachState/meAttachState
// here would DUPLICATE the base dtor's stores. (Identical treatment to the committed
// sibling Passby3DControl::~Passby3DControl(), same base + same asm shape, whose body
// is likewise empty.)
//
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free
// the object; that allocator is not homed here, so operator-delete dispatch is left to
// the host toolchain (the `delete` half of the X360 vector deleting destructor).
// ---------------------------------------------------------------------------
Brn3DUserSpaceEffectControl::~Brn3DUserSpaceEffectControl()
{
}

// ARTIST @0x82682CA0.
void Brn3DUserSpaceEffectControl::AttachTransform(const Matrix44Affine* apTransform)
{
    mpTransform = apTransform;
}

// ARTIST @0x826968E8. Positions supplied by collision states are in the
// attached transform's user space; the Cgs3d base always receives the stable
// generated world-space member.
void Brn3DUserSpaceEffectControl::AttachEmitterPosition(const Vector3* apPosition)
{
    CGS_ASSERT(mpTransform != nullptr, "mpTransform");
    CGS_ASSERT(apPosition != nullptr, "lpPosition");
    if (!mpTransform || !apPosition)
        return;

    mPositionInUserSpace = *apPosition;
    mGeneratedPosition = rw::math::vpu::TransformPoint(
        *mpTransform, mPositionInUserSpace);
    CgsSound::Logic::Cgs3dEffectControl::AttachEmitterPosition(&mGeneratedPosition);
}

// ARTIST @0x82696C20. Direction transformation deliberately excludes translation.
void Brn3DUserSpaceEffectControl::AttachEmitterDirection(const Vector3* apDirection)
{
    CGS_ASSERT(mpTransform != nullptr, "mpTransform");
    CGS_ASSERT(apDirection != nullptr, "lpDirection");
    if (!mpTransform || !apDirection)
        return;

    mDirectionInUserSpace = *apDirection;
    mGeneratedDirection = rw::math::vpu::TransformVector(
        *mpTransform, mDirectionInUserSpace);
    CgsSound::Logic::Cgs3dEffectControl::AttachEmitterDirection(&mGeneratedDirection);
}

// ARTIST @0x826FB7C8. Rebuild the generated world-space emitter values each
// frame before the common 3D control publishes mixer distance/pan inputs.
void Brn3DUserSpaceEffectControl::UpdateParams(f32 afDeltaTime)
{
    if (mpTransform)
    {
        mGeneratedPosition = rw::math::vpu::TransformPoint(
            *mpTransform, mPositionInUserSpace);
        mGeneratedDirection = rw::math::vpu::TransformVector(
            *mpTransform, mDirectionInUserSpace);
    }

    BrnSound::Logic::Brn3DEffectControl::UpdateParams(afDeltaTime);
}

} // namespace Logic
} // namespace BrnSound
