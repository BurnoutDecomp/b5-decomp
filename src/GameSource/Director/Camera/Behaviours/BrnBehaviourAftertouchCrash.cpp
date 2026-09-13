// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourAftertouchCrash slice this TU owns:
// the four transcribed virtuals (Construct, Prepare, GetCollisionPolicy, GetName). The
// Parameters::Serialise<S> field-walk lives in the sibling *Parameters.cpp, out of the link.
// SetParameters and the small setters are defined inline in the header. The rest of the
// behaviour (Update/SetupTweaker and the full rig) lands with its own TU.
//
// SetParameters is adopted by the crash-mode / takedown arbitrator states and the arbitrator
// testbed when they install an aftertouch-crash parameter block.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.h"

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// BehaviourAftertouchCrash::Construct -- seed a freshly pooled instance.
//
//   li     r5, 0 / li r4, 0
//   addi   r3,  r6, 0x60      ; &mCollisionPolicy
//   stb    r5,  +0x008(r6)    ; \
//   stb    r5,  +0x009(r6)    ;  |
//   stb    r5,  +0x00A(r6)    ;  |- the base head: these seven stores ARE Behaviour::Construct,
//   stb    r5,  +0x00B(r6)    ;  |  inlined (meTimestepType, the five flag bytes and the debug
//   stb    r5,  +0x00C(r6)    ;  |  parameters name, all zero)
//   stw    r5,  +0x004(r6)    ;  |
//   stw    r5,  +0x010(r6)    ; /
//   bl     CollisionPolicyAttachedToVehicle::Construct   ; with r4 == 0
//   stb    r5,  +0x2A8(r6)    ; policy +0x248 mbAutoElevate         = false
//   stb    r9,  +0x2A9(r6)    ; policy +0x249 mbSmoothRadiusChanges = true   (r9 == 1)
//   stb    r9,  +0x2AC(r6)    ; policy +0x24C mbTestAgainstWorldOnly = true
//   stb    r9,  +0x2AD(r6)    ; policy +0x24D mbUseFrustrumResolver  = true
//   ... the rig sub-object seeding at +0x300 .. +0x383 (see the FLAG below) ...
//   stvx128 v0,  r6, 0x2F0    ; mCameraPositionLastFrame = zero (v0 == vspltisw 0)
//   stb    r5,  +0x3C0(r6) .. stb r5, +0x3C4(r6)   ; the five flag bytes
//   stvx128 v13, r6, 0x3B0    ; mfManualHeightAdjustment = splat(0.0f)  (reserved run)
//   stfs   f13, +0x3CC(r6)    ; mfBounceShakeMultiplier = 1.0f
//   stfs   f0,  +0x3D0(r6)    ; mfRollAngleRads     = 0.0f
//   stfs   f0,  +0x3D4(r6)    ; mfCloseupAmount0To1 = 0.0f
//
// The two read-only constants were read out of the flat image, not guessed: f0 is 0.0f and f13
// is 1.0f (big-endian 00000000 / 3f800000 at the two referenced slots).
//
// NOT written here, faithfully: mfDebugCrashCameraParam0to1 (Prepare seeds it) and mpParameters
// (SetParameters adopts it, and Prepare asserts it was adopted).
// ----------------------------------------------------------------------------
void BehaviourAftertouchCrash::Construct()
{
    Behaviour::Construct();

    // The embedded vehicle-attached policy, then the four authored flag overrides the console
    // applies to it immediately after its own Construct returns -- by named setter, not by
    // offset (the same shape the rotate-about-vehicle behaviour uses on the same four bools).
    mCollisionPolicy.Construct(false);                  // bl ..., r4 == 0
    mCollisionPolicy.SetAutoElevate(false);             // policy +0x248
    mCollisionPolicy.SetSmoothRadiusChanges(true);      // policy +0x249
    mCollisionPolicy.SetTestAgainstWorldOnly(true);     // policy +0x24C
    mCollisionPolicy.SetUseFrustrumResolver(true);      // policy +0x24D

    mCameraPositionLastFrame.SetZero();                 // stvx128 of a zero vector

    mbManualCameraControl               = false;        // stb 0, +0x3C0
    mbWasFallingDownwards               = false;        // stb 0, +0x3C1
    mbDisableCollision                  = false;        // stb 0, +0x3C2
    mbIsTempDebugCrashCamera            = false;        // stb 0, +0x3C3
    mbIsRandomStartTempDebugCrashCamera = false;        // stb 0, +0x3C4

    mfBounceShakeMultiplier = 1.0f;                     // stfs +0x3CC
    mfRollAngleRads         = 0.0f;                     // stfs +0x3D0
    mfCloseupAmount0To1     = 0.0f;                     // stfs +0x3D4

    // FLAG (not transcribed): the console also seeds the rig sub-objects that this slice still
    //   holds as reserved runs -- two flag bytes raised near the head of the run, the random
    //   generator's 64-bit state plus its ring index and the draws that prime it, and two runs
    //   of zeroed scalars -- and the 16-byte mfManualHeightAdjustment (splat 0.0f). Those
    //   members have no home yet, so there is nothing to name here; every byte the console
    //   writes in that window lands inside
    //   maReservedRigSubObjects / maReservedManualHeight, which nothing in this slice reads.
    //   DELETE-WHEN: the rig TU homes PositionLag / Random / CameraShake / CameraImpactEffect
    //   and carves them out of those runs.
}

// ----------------------------------------------------------------------------
// BehaviourAftertouchCrash::Prepare
//   li     r11, 0
//   lwz    r10, +0x3D8(r31)   ; mpParameters
//   stb    r11, +0x008(r31)   ; SetNotPrepared()
//   cmplwi r10, 0
//   bne    ...                ; assert mpParameters != NULL
//   lwz    r11, +0x3D8(r31)
//   li     r3,  1             ; the return value: it cannot fail
//   lfs    f13, +0x48(r11)    ; mpParameters->mfMinimumBlendFactor
//   stfs   f13, +0x38C(r31)   ; mfBlendFactor
//   lfs    f13, +0x38(r11)    ; mpParameters->mfFastHeight
//   stfs   f13, +0x384(r31)   ; mfHeight
//   lfs    f13, +0x34(r11)    ; mpParameters->mfFastDistance
//   lfs    f0,  <rodata>      ; 0.5f
//   stfs   f13, +0x388(r31)   ; mfDistance
//   stfs   f0,  +0x3C8(r31)   ; mfDebugCrashCameraParam0to1
//
// The shared prepare/release info block is not read: this behaviour seeds entirely from its own
// adopted parameter block. Dropping the prepared latch here is the same first-frame idiom the
// road-runner and interpolate behaviours use -- Update re-seeds the rig on the frame after.
// ----------------------------------------------------------------------------
bool BehaviourAftertouchCrash::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    SetNotPrepared();                                       // stb 0, +0x08

    CGS_ASSERT(mpParameters != 0, "mpParameters != NULL");

    mfBlendFactor = mpParameters->mfMinimumBlendFactor;     // +0x48 -> +0x38C
    mfHeight      = mpParameters->mfFastHeight;             // +0x38 -> +0x384
    mfDistance    = mpParameters->mfFastDistance;           // +0x34 -> +0x388

    // The authored midpoint the debug crash camera restarts from (a 0.5f in read-only data).
    mfDebugCrashCameraParam0to1 = 0.5f;                     // stfs +0x3C8

    return true;                                            // li r3, 1
}

// ----------------------------------------------------------------------------
// BehaviourAftertouchCrash::GetCollisionPolicy
//   lbz    r11, +0x3C2(r3)    ; mbDisableCollision
//   addi   r3,  r3, 0x60      ; r3 = &this->mCollisionPolicy (the candidate return)
//   cmplwi r11, 0
//   beqlr                     ; flag clear -> return &mCollisionPolicy
//   li     r3, 0              ; flag set   -> return null
//   blr
//
// The `addi r3, r3, 0x60` is the derived-to-base adjustment folded into the return: on the
// console CollisionPolicyAttachedToVehicle's CollisionPolicy sub-object is at its own +0x00, so
// the policy's address and the interface pointer coincide. On the host the compiler emits
// whatever adjustment the real base sub-object needs; parity is by named member.
// ----------------------------------------------------------------------------
CollisionPolicy* BehaviourAftertouchCrash::GetCollisionPolicy()
{
    if (mbDisableCollision)                                 // lbz +0x3C2; bne -> null
    {
        return 0;
    }
    return &mCollisionPolicy;                               // this + 0x60
}

// ----------------------------------------------------------------------------
// BehaviourAftertouchCrash::GetName -- returns the class's own literal.
// ----------------------------------------------------------------------------
const char* BehaviourAftertouchCrash::GetName() const
{
    return "BehaviourAftertouchCrash";
}

} // namespace Camera
} // namespace BrnDirector
