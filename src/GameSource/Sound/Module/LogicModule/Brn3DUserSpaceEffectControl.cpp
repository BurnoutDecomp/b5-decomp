#include "GameSource/Sound/Module/LogicModule/Brn3DUserSpaceEffectControl.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include <vectorintrinsics.h>

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
// Tagged allocation now dispatches through the shared sound MemBase allocator.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectControl* Brn3DUserSpaceEffectControl::CreateObject(u32 luType)
{
    return new ("Brn3DUserSpaceEffectControl", luType != 0) Brn3DUserSpaceEffectControl();
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

// ARTIST 826968E8: the supplied point is WORLD space. Store it in the
// attached transform's local space, then let UpdateParams regenerate the world
// position from the current transform. A forward transform here applies the
// camera translation twice and places audible collisions kilometres away.
void Brn3DUserSpaceEffectControl::AttachEmitterPosition(const Vector3* apPosition)
{
    CGS_ASSERT(mpTransform != 0, "mpTransform");
    CGS_ASSERT(apPosition != 0, "lpPosition");
    CGS_ASSERT(rw::math::vpu::IsValid(*mpTransform), "IsValid( *mpTransform )");
    CGS_ASSERT(rw::math::vpu::IsValid(*apPosition), "IsValid( *lpPosition )");

    // ARTIST82696BA0..82696C14: transpose the rotation, rotate the
    // negated translation, then accumulate the world position. These are the
    // original VMX operations, including their fused rounding and denormal rules.
    const __vector4 lvZero = __vspltisw(0);
    const __vector4 lvPosition = __lvx(apPosition, 0);
    const __vector4 lvNegativeTranslation = __vsubfp(lvZero, __lvx(&mpTransform->wAxis, 0));
    const __vector4 lvX = __lvx(&mpTransform->xAxis, 0);
    const __vector4 lvY = __lvx(&mpTransform->yAxis, 0);
    const __vector4 lvZ = __lvx(&mpTransform->zAxis, 0);
    const __vector4 lvXZLow = __vmrglw(lvX, lvZ);
    const __vector4 lvYZeroLow = __vmrglw(lvY, lvZero);
    const __vector4 lvXZHigh = __vmrghw(lvX, lvZ);
    const __vector4 lvYZeroHigh = __vmrghw(lvY, lvZero);
    const __vector4 lvTransposeX = __vmrghw(lvXZHigh, lvYZeroHigh);
    const __vector4 lvTransposeY = __vmrglw(lvXZHigh, lvYZeroHigh);
    const __vector4 lvTransposeZ = __vmrghw(lvXZLow, lvYZeroLow);

    __vector4 lvResult = __vmulfp(__vspltw(lvNegativeTranslation, 2), lvTransposeZ);
    lvResult = __vmaddfp(__vspltw(lvNegativeTranslation, 1), lvTransposeY, lvResult);
    lvResult = __vmaddfp(__vspltw(lvNegativeTranslation, 0), lvTransposeX, lvResult);
    lvResult = __vmaddfp(lvTransposeX, __vspltw(lvPosition, 0), lvResult);
    lvResult = __vmaddfp(lvTransposeY, __vspltw(lvPosition, 1), lvResult);
    lvResult = __vmaddfp(lvTransposeZ, __vspltw(lvPosition, 2), lvResult);
    __stvx(lvResult, &mPositionInUserSpace, 0);

    // 82696BD8 publishes the stable member address; generation occurs later.
    mpEmitterPosition = &mGeneratedPosition;
}

// ARTIST @0x82696C20. Direction transformation deliberately excludes translation.
void Brn3DUserSpaceEffectControl::AttachEmitterDirection(const Vector3* apDirection)
{
    CGS_ASSERT(mpTransform != 0, "mpTransform");
    CGS_ASSERT(apDirection != 0, "lpDirection");
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
