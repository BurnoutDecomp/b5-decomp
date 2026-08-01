#include "GameSource/Director/Camera/Behaviours/Behaviour.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (BeginAssert/FireAssert/EndAssert)

// ============================================================================
// GameSource/Director/Camera/Behaviours/Behaviour.cpp
//
// The BrnDirector::Camera::Behaviour base bodies -- the abstract base every director camera
// behaviour derives from and the BehaviourManager pools by value. The layout / vtable order
// and their asm provenance live in the header banner.
//
// Bodied here:
//   Behaviour::SetCantSwitchFromMeNow @0x82206388
//   Behaviour::Fail                   @0x822063E8
//   Behaviour::SetCantSwitchToMeNow   (the Behaviour.h:447 twin -- see the FLAG below)
//   the base virtual defaults        (DWARF Behaviour.cpp:28 / :49 / :59 / :67 -- the
//                                     bodies the vtable's unoverridden slots point at)
//
// ⭐ WHAT CHANGED (2026-07-29): this TU used to build against a FORKED, offset-modelled
// `Behaviour` slice that lived in BrnBehaviourIceAnim.h, and reached the camera's validity
// account through three DECLARATION-ONLY `detail::` helpers because the account had no named
// path. Both are gone: the base is the canonical Behaviour.h home, and the account is reached
// as `lrCamera.GetValidityAccount()` (the X360's `addi rN, camera, 0x138` is the union alias
// Camera.h now names). No `void*` and no offset arithmetic remains in this file.
// ============================================================================

namespace BrnDirector
{
namespace Camera
{

// ============================================================================
// Behaviour::SetCantSwitchFromMeNow @0x82206388
//
// Record "the director cannot cut away from me this frame" with the given reason.
//
// asm:  lbz  r11,9(this); cmplwi; beq          : if (mbHasFailed) fire the Behaviour.h:460
//                                                 assert "Setting \"Cant switch from me
//                                                 now\" when behaviour has failed"
//       mr   r4,r5; addi r3,r30,0x138; bl sub_82204148
//                                                : camera-validity-account SetNoCutFromFlag
//       li   r11,0; stb r11,0xC(this)            : mbCanSwitchFromMeNow = false
// ============================================================================
void Behaviour::SetCantSwitchFromMeNow(Camera& lrCamera, s32 leNoCutFromFlag)
{
    CGS_ASSERT(!mbHasFailed,
               "Setting \"Cant switch from me now\" when behaviour has failed");   // Behaviour.h:460

    lrCamera.GetValidityAccount().SetNoCutFromFlag(leNoCutFromFlag);

    mbCanSwitchFromMeNow = false;   // stb 0, 0xC(this)
}

// ============================================================================
// Behaviour::SetCantSwitchToMeNow (Behaviour.h:447)
//
// The "the director cannot cut TO me this frame" twin. No standalone X360 export exists for
// it in the available dumps (it is inlined at every call site), so the body below is the
// SHAPE its twin proves -- account flag + the matching gate byte -- and nothing more.
// FLAG: `ValidityAccount::SetNoCutToFlag` is itself declaration-only (its flag BAND is not
// attested; see BrnCameraValidityAccount.h). Nothing on the live director path calls this
// yet, so it links only if a future consumer needs it.
// DELETE-WHEN: the no-cut-TO account setter's address/band is identified.
// ============================================================================
void Behaviour::SetCantSwitchToMeNow(Camera& lrCamera, s32 leNoCutToFlag)
{
    CGS_ASSERT(!mbHasFailed,
               "Setting \"Cant switch to me now\" when behaviour has failed");

    lrCamera.GetValidityAccount().SetNoCutToFlag(leNoCutToFlag);

    mbCanSwitchToMeNow = false;
}

// ============================================================================
// Behaviour::Fail @0x822063E8
//
// Give up: flag the failure reason in the camera's validity account, raise this behaviour's
// failed state (so the director is free to cut away from it and forbidden to cut to it), and
// drop the camera-state "follow" bit.
//
// asm:  addi r3,r30,0x138; bl ValidityAccount::SetFlag  : account SetFlag(camera +0x138)
//       li r11,1;  stb r11,0xC(this)                    : mbCanSwitchFromMeNow = true
//                  stb r11,9(this)                      : mbHasFailed          = true
//       li r10,0;  stb r10,0xB(this)                    : mbCanSwitchToMeNow   = false
//       li r12,-3; ld r11,0x140(r30); and r11,r11,r12; std r11,0x140(r30)
//                                                        : camera-state current-flag set
//                                                          &= ~2  (clear flag 1)
//
// FLAG (flag id 1): the camera-state flag the 64-bit `and ~2` clears is bit 1 of
// CameraState::mCurrentFlags. Its NAME is not recovered (CameraState's flag enum is not in
// the DWARF this project has); the retired BrnBehaviourIceAnim.h slice called the same write
// "drop the follow request bit". Expressed here through CameraState's own named ClearFlag so
// no offset is poked; the literal 1 is the console's own index, not a guess.
// ============================================================================
void Behaviour::Fail(Camera& lrCamera, s32 leFailedFlag)
{
    lrCamera.GetValidityAccount().SetFlag(leFailedFlag);

    mbCanSwitchFromMeNow = true;    // stb 1, 0xC(this)
    mbHasFailed          = true;    // stb 1, 9(this)
    mbCanSwitchToMeNow   = false;   // stb 0, 0xB(this)

    lrCamera.GetState().ClearFlag(1u);   // ld/and ~2/std on camera +0x140
}

// ============================================================================
// The base virtual defaults (DWARF Behaviour.cpp:28 / :49 / :59 / :67).
//
// These are the bodies the vtable slots of a behaviour that does NOT override them point at.
// The DWARF places all four in this .cpp; none has a standalone X360 export (they are tiny
// and ICF-folded), so each is reconstructed as the only shape consistent with its callers:
//
//   * Construct()            slot 0 -- BehaviourHelper::Prepare @0x82255F48 dispatches it
//                            immediately after a fresh pool slot is handed out, and every
//                            concrete override observed (e.g. BehaviourRoadRunner::Construct
//                            @0x8222BCE0, whose first six stores are exactly the base's six
//                            fields) zeroes the base's own fields first. The base default is
//                            that zeroing.
//   * PostCollisionUpdate()  slot 3 -- BehaviourHelper::PostCollisionUpdate forwards the
//                            return value; a behaviour with no collision pass reports "done".
//   * Release()              slot 4 -- ReleaseBehaviours @0x8221FDE8 dispatches it and
//                            ignores the result; the base has nothing to hand back.
//   * GetCollisionPolicy()   slot 5 -- callers null-check the result; a behaviour with no
//                            policy answers null.
// FLAG: the four bodies above are SHAPE-attested (by their dispatch sites and by every
// concrete override), not byte-attested. They are deliberately trivial -- nothing is
// fabricated beyond "the base does nothing".
// ============================================================================
// NOTE on meTimestepType's default: the console stores a literal ZERO at behaviour +0x04
// (BehaviourRoadRunner::Construct @0x8222BCE0 `*(result + 4) = 0`), i.e. E_WORLD -- NOT
// E_TIMESTEP_INVALID (-1). That also settles a self-inconsistency in the retired
// BehaviourRig.h fork, whose own EType put INVALID at 0 while BehaviourRig::Update asserts
// `meTimestepType > E_TIMESTEP_INVALID`: with the canonical enum the default passes.
void Behaviour::Construct()
{
    meTimestepType         = BrnDirector::Timestep::E_WORLD;
    mbIsPrepared           = false;
    mbHasFailed            = false;
    mbTweakerAttached      = false;
    mbCanSwitchToMeNow     = false;
    mbCanSwitchFromMeNow   = false;
    mpcDebugParametersName = 0;
}

bool Behaviour::PostCollisionUpdate(Camera& /*lrCamera*/, const BehaviourSharedInfo& /*lrInfo*/)
{
    return true;
}

void Behaviour::Release(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
}

CollisionPolicy* Behaviour::GetCollisionPolicy()
{
    return 0;
}

// ----------------------------------------------------------------------------
// The remaining base virtuals are PURE in effect -- every concrete behaviour overrides them
// and the DWARF gives the base no .cpp definition line, so there is no console body to
// reconstruct. They are given the only safe default here (succeed / no parameters / a name)
// rather than being made pure-virtual, because the manager's pools CONSTRUCT behaviours by
// value through placement-new and a pure-virtual base would forbid that for the base itself.
// ⛔⛔ FLAG CORRECTED 2026-08-01 -- IT USED TO READ "never dispatched on the live path (every
// pooled behaviour is a concrete leaf)". THAT WAS FALSE, and it cost a whole wave to find.
// BehaviourInterpolate IS a concrete leaf, it IS pooled, it IS dispatched every frame by the
// arbitrator's car-select state -- and until 2026-08-01 it declared NONE of these virtuals, so
// `Behaviour::Update` below (a `return true;` that never touches the camera) was its real
// Update. The behaviour looked completely healthy from the outside: allocated, prepared, ready,
// producing a camera every frame -- a camera nothing had ever written, i.e. exactly what
// BehaviourHelper::Prepare's Camera::Construct left there. "A concrete leaf overrides these"
// is an ASSUMPTION about every derived class, not a property of this file, and it goes stale
// the moment a leaf lands without its virtuals.
// The remaining risk is the same shape: these bodies are the safe default for a leaf that has
// not been reconstructed yet, and a leaf that reaches them SILENTLY DOES NOTHING. If a camera
// or behaviour "runs but produces nothing", check here FIRST.
// DELETE-WHEN: the base is proven abstract in the console (a vtable dump showing __purecall in
// slots 1/2/6/7/8/9).
// ----------------------------------------------------------------------------
bool Behaviour::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    return true;
}

bool Behaviour::Update(Camera& /*lrCamera*/, const BehaviourSharedInfo& /*lrInfo*/)
{
    return true;
}

const Behaviour::Parameters* Behaviour::GetParameters() const
{
    return 0;
}

void Behaviour::SetParameters(const Parameters* /*lpParameters*/)
{
}

void Behaviour::SetupTweaker(Utils::Tweaker& /*lrTweaker*/)
{
}

const char* Behaviour::GetName() const
{
    return "Behaviour";
}

// ----------------------------------------------------------------------------
// Behaviour::IsDebugDisplayActive (Behaviour.h:510) -- whether the manager's debug camera
// display is showing this behaviour. DECLARATION-ONLY: the X360 reads it off the owning
// BehaviourManager's mbDebugDisplayAllCameras plus the per-helper debug state, and a
// Behaviour holds no back-pointer to its manager in the DWARF layout -- so the resolution
// path is genuinely unrecovered and is NOT fabricated here.
// DELETE-WHEN: the behaviour->manager back-reach is identified (the most likely shape is the
// manager threading it through BehaviourSharedInfo::mpBehaviourManager).
// ----------------------------------------------------------------------------

} // namespace Camera
} // namespace BrnDirector
