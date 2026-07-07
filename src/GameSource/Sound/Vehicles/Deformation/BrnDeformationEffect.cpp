// ============================================================================
// b5-decomp/src/GameSource/Sound/Vehicles/Deformation/BrnDeformationEffect.cpp
//
// BrnSound::Vehicles::Deformation::DeformationEffect -- the car-body crumple/deformation
// sound effect object. Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
//
// This batch bodies the four verified functions:
//   DeformationEffect()  0x826CF6C0   (MSVC inlined full-object ctor)
//   GetTypeName()        0x82685730   (interned type-tag leaf)
//   SetupLoadData()      0x826E4C88   (request the crumple patch bank bundle)
//   Detach()             0x826F39C8   (base detach + release the patch voice)
//
// AttachController (0x82685740) is now bodied below (the controller-class gate +
// mpPhysicsControl latch). Attach (0x826F37E8) remains BLOCKED: its store-for-store body
// needs the un-homed CgsSound::Logic::VoiceWrapper::Create/Play + VoiceWrapper::CreateParams
// (a foundational shared-header grow disallowed by scope), the un-homed AEMS interned-hash
// static-init globals, and an un-homed base +0xE sequence counter -- so it is left
// declared-only in the header for the vtable shape.
// ============================================================================

#include "GameSource/Sound/Vehicles/Deformation/BrnDeformationEffect.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h" // complete PhysicsControl for the AttachController downcast (BY NAME)

namespace BrnSound
{
namespace Vehicles
{
namespace Deformation
{

// ---------------------------------------------------------------------------
// DeformationEffect::DeformationEffect()  @ 0x826CF6C0
//
// MSVC's INLINED full-object constructor. It does NOT `bl` a base ctor -- it inlines
// the BrnEffectObject dual-base member zero-inits + installs both leaf vptrs (this+0
// off_820B3E44 + this+4 IResourceRequester off_820B3E10), then default-constructs
// every leaf member. In reconstructed C++ the dual-base settle is produced structurally
// by the BrnEffectObject base default ctor (BY NAME); the leaf members' zero-inits are
// their own default ctors (DataPoint / Average / InterpolateLine all zero-init;
// InterpolateLine seeds mbComplete=true == the X360 stb 1 @+0x80), and the tail
// `bl VoiceWrapper::VoiceWrapper(this+0x8C)` is mPatchVoice's default ctor.
// The X360 leaves mfAemsIntensity / mfTimeDeforming / mpPhysicsControl UNINITIALIZED
// (populated on Attach / AttachController) -- same convention as the sibling
// ReverbEffect ctor -- so they are not seeded here.
// ---------------------------------------------------------------------------
DeformationEffect::DeformationEffect()
    : BrnSound::Logic::BrnEffectObject()  // dual-base vptr settle + base zero-init (BY NAME)
    // mbDeforming/mDeformAmount/mDeformDeltaAverage/mDeformIntensityLagged/mFadeOut/
    // mPatchVoice: default-constructed (each embedded default ctor zeroes; InterpolateLine
    // mbComplete=true -> stb 1 @+0x80; VoiceWrapper ctor is the tail bl @ +0x8C).
    // mfAemsIntensity(+0x64)/mfTimeDeforming(+0x84)/mpPhysicsControl(+0x88): intentionally
    // UNINITIALIZED -- the X360 ctor writes nothing there (seeded on Attach/AttachController).
{
}

// ---------------------------------------------------------------------------
// DeformationEffect::GetTypeName() const  @ 0x82685730  -> "DeformationEffect"
//   The X360 leaf loads the interned type-name string (off_82F2F684, the tag
//   CreateObject's operator new / RTTI uses) and returns it. Mirrors the
//   committed PlayerVehicleStateManager / TrafficStateManager GetTypeName leaves.
// ---------------------------------------------------------------------------
const char* DeformationEffect::GetTypeName() const
{
    return "DeformationEffect";
}

// ---------------------------------------------------------------------------
// DeformationEffect::SetupLoadData()  @ 0x826E4C88
//   Tail-forwards to the IResourceRequester base's LoadAsset: request the crumple
//   patch bank bundle. The X360 `addi r3,r3,-4` is the multiple-inheritance base
//   adjustment recovering the IResourceRequester sub-object from the BrnEffectObject
//   `this`; reproduced here as a static_cast to the IResourceRequester base plus a
//   plain member call. r5=0 -> the 2nd (const char*) arg is null; r6=0 -> EType == E_DATA.
// ---------------------------------------------------------------------------
void DeformationEffect::SetupLoadData()
{
    static_cast<BrnSound::Logic::IResourceRequester*>(this)->LoadAsset(
        "sound\\aems\\CRUMPLEPATCHBANK.BUNDLE", nullptr,
        BrnSound::Logic::ResourceRegistrar::E_DATA);
}

// The controller-class band the AttachController gate tests. The X360 masks the
// supplied controller's object-id (`*(controller+0x14) & 0x7F0`) -- the class-tag band
// shared with the sibling SingleGinsuEffect::AttachController. DeformationEffect accepts
// ONLY a controller whose class band is 0 (the PhysicsControl class); any set bit in the
// band is a mis-attached controller and trips the assert.
static const s32 KI_CONTROLLER_CLASS_MASK = 0x7F0;

// ---------------------------------------------------------------------------
// DeformationEffect::AttachController  @ 0x82685740   (override of EffectBase::AttachController)
//
//   r11 = *(controller+0x14);            ; controller->miObjectId  (GetObjectId)
//   r11 &= 0x7F0;                         ; class-tag band
//   if (r11 != 0)                         ; wrong controller class?
//       << assert "Cound't attach controller " >>   ; (Begin/Fire/EndAssert chain)
//   else
//       this->mpPhysicsControl = controller - 4;     ; latch the physics controller
//
// The controller is handed in via its CgsSound::Logic::EffectBase sub-object pointer; the
// X360 `controller - 4` recovers the primary object, modelled here as a by-name downcast
// to PhysicsControl (its EffectBase primary base performs the equivalent adjustment). The
// de-inlined BeginAssert/Clear/FireAssert/EndAssert chain (which streams the failing
// object-id after the message) collapses to one CGS_ASSERT with the verbatim rodata
// string; the file-path/line args are dropped per convention.
// ---------------------------------------------------------------------------
void DeformationEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    if ((apController->GetObjectId() & KI_CONTROLLER_CLASS_MASK) != 0)
    {
        CGS_ASSERT(false, "Cound't attach controller ");
    }
    else
    {
        mpPhysicsControl = static_cast<BrnSound::Vehicles::Engines::PhysicsControl*>(apController);
    }
}

// ---------------------------------------------------------------------------
// DeformationEffect::Detach  @ 0x826F39C8   (override of EffectBase::Detach)
//
//   if (!BrnEffectObject::Detach()) return false;   ; base teardown must succeed
//   VoiceWrapper::Release(this+0x88);                 ; release mPatchVoice
//   return true;
//
// Forwards to the committed BrnEffectObject::Detach (resource-request teardown +
// attach-state finalise). If that succeeds, releases the crumple patch voice.
// ---------------------------------------------------------------------------
bool DeformationEffect::Detach()
{
    if (!BrnSound::Logic::BrnEffectObject::Detach())
    {
        return false;
    }
    mPatchVoice.Release();
    return true;
}

} // namespace Deformation
} // namespace Vehicles
} // namespace BrnSound
