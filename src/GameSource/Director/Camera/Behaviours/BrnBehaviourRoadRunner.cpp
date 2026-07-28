// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.cpp
//
// BrnDirector::Camera::BehaviourRoadRunner -- THE DJ FLY-BY CAMERA BEHAVIOUR.
//
// Bodied here:
//   BehaviourRoadRunner::Construct           @0x8222BCE0   (full field sweep)
//   BehaviourRoadRunner::Prepare             @0x8220F748   (full float sweep; returns true)
//   BehaviourRoadRunner::Reverse             (BrnBehaviourRoadRunner.h:331, inlined on console)
//   BehaviourRoadRunner::GetName             @0x821FB130
//   BehaviourRoadRunner::GetParameters / SetParameters
//   BehaviourRoadRunner::Update              @0x82247E98   -- ⚠️ the un-prepared leg only (see below)
//   BehaviourRoadRunner::PostCollisionUpdate @0x8220F850   -- ⚠️ gated
//   TrafficLaneTruck::GetTransform           @0x821F53D8
//   TrafficLaneTruck::GetLocalAngularVelocity@0x821F5470
//   TrafficLaneTruck::GetLinearVelocity      @0x821F54E0
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.h"
#include <cstddef>   // offsetof

namespace BrnDirector
{
namespace Camera
{

// Pin the asm-attested member offsets the accessors sample. Every member of the truck is
// size-stable (no pointers), so these hold exactly on the host too.
static_assert(offsetof(TrafficLaneTruck, mTransform)            == 0x20, "transform @ +0x20");
static_assert(offsetof(TrafficLaneTruck, mLocalAngularVelocity) == 0x60, "angular velocity @ +0x60");
static_assert(offsetof(TrafficLaneTruck, mLinearVelocity)       == 0x70, "linear velocity @ +0x70");
static_assert(offsetof(TrafficLaneTruck, mfSpeed)               == 0x80, "speed @ +0x80");
static_assert(offsetof(TrafficLaneTruck, mbPrepared)            == 0x8C, "mbPrepared @ +0x8C");

// ----------------------------------------------------------------------------
// TrafficLaneTruck::GetTransform @0x821F53D8
// Asserts the truck is prepared, then copies the 64-byte transform (four 16-byte aligned rows,
// lvx128/stvx128 at base this+0x20, stride 16) into the by-value return slot.
// ----------------------------------------------------------------------------
rw::math::vpu::Matrix44Affine TrafficLaneTruck::GetTransform() const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    return mTransform;                                 // 4x lvx128 (this+0x20) -> 4x stvx128 (sret)
}

// ----------------------------------------------------------------------------
// TrafficLaneTruck::GetLocalAngularVelocity @0x821F5470
// ----------------------------------------------------------------------------
rw::math::vpu::Vector3 TrafficLaneTruck::GetLocalAngularVelocity() const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    return mLocalAngularVelocity;                      // lvx128 (this+0x60) -> stvx128 (sret)
}

// ----------------------------------------------------------------------------
// TrafficLaneTruck::GetLinearVelocity @0x821F54E0
// ----------------------------------------------------------------------------
rw::math::vpu::Vector3 TrafficLaneTruck::GetLinearVelocity() const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    return mLinearVelocity;                            // lvx128 (this+0x70) -> stvx128 (sret)
}

// ============================================================================
// BehaviourRoadRunner::Construct @0x8222BCE0  -- vtable slot 0, dispatched by
// BehaviourHelper::Prepare the instant the pool hands out a slot.
//
// Every store below is in the asm; the offset each one names is resolved by the layout in the
// header. The first seven (`*(this+4)`, `*(this+8..12)`, `*(this+16)`) are the base's own six
// fields -- i.e. the inlined Behaviour::Construct.
// ============================================================================
void BehaviourRoadRunner::Construct()
{
    Behaviour::Construct();                        // *(this+4)=0, +8..+12=0, +16=0

    mpParameters = 0;                              // *(this+20) = 0   (the LAST store in the asm)

    mTrafficLaneTruck.mLanePosition.mbValid = false;   // *(this+62)  == truck +0x1E
    mTrafficLaneTruck.mfSpeed               = 0.0f;    // *(this+160) == truck +0x80
    mTrafficLaneTruck.mbPrepared            = false;   // *(this+172) == truck +0x8C

    // The two near-clip diagonal post boxes start idle (`*(this+176)` / `*(this+256)` -- the
    // post box's own head word). Reached through the named sub-objects; the interiors are
    // un-homed (see the header FLAG), so the console's single head store is expressed as a
    // whole-object reset.
    mTopLeftBottomRight = LineTestNearestPostBox();     // *(this+176) = 0
    mTopRightBottomLeft = LineTestNearestPostBox();     // *(this+256) = 0
    mLineTestFineA      = LineTestFinePostBox();        // *(this+336) = 0
    mLineTestFineB      = LineTestFinePostBox();        // *(this+344) = 0

    // The shake transform the console builds with four stvx128s at +0x200/+0x210/+0x220/+0x230
    // (r10 = this+512, offsets 0/16/32/48) from two register constants.
    mLastShakeTransform = rw::math::vpu::Matrix44Affine();

    // ⚠️ QUIET GATE: the shake seeds. The asm's `*(this+576..588) = 0.0` (mShake) and
    //   `*(this+592..604) = {0.0, 0.0, 1.0, 0.25}` (mShakeParams, written twice -- 0.06/1.15/
    //   0.11 first, then overwritten) land inside the two sub-objects this header models
    //   opaquely because Utils::CameraShake has no home outside BehaviourRig.h (see the header
    //   FLAG). Writing them would mean fabricating that type's field order.
    //   CONSEQUENCE: the fly-by's camera shake starts zeroed rather than at the authored
    //   amplitude -- cosmetic on a camera that cannot move yet anyway.
    //   DELETE-WHEN: Utils::CameraShake gets its own header.

    mfDirection             = 1.0f;                // *(this+668)
    mbIsColliding           = false;               // *(this+692)
    mbWasCollidingLastFrame = false;               // *(this+693)
    mbFixationsAllowed      = false;               // *(this+700)
    mbOccluded              = false;               // *(this+701)
}

// ============================================================================
// BehaviourRoadRunner::Prepare @0x8220F748 -- vtable slot 1, dispatched by
// BehaviourManager::PrepareBehaviours once per allocation.
//
// ⭐ IT TOUCHES NOTHING OUTSIDE ITSELF AND ALWAYS RETURNS TRUE. That matters: the road-runner's
// Prepare cannot fail, so BehaviourManager::PrepareBehaviours always clears the "needs
// preparing" bit on the very next pass -- which is precisely what lets
// ArbStateAttractMode::Prepare stop returning false and the attract state leave
// E_STATE_PREPARING. The world-data dependency lives in Update, not here.
//
// Every store is in the asm; the offsets resolve through the header's layout.
// ============================================================================
bool BehaviourRoadRunner::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    SetNotPrepared();                              // *(this+8) = 0 -- Update re-seeds the truck

    // ⚠️ QUIET GATE (one call, not a branch): the console opens with
    //     Utils::TransitionSmoother::Set(&mHeight, 4.0f, <3 more floats>)
    //   Only the 4.0f survives the decompile as a named constant; the other three arguments
    //   (the ideal lerp amount, its own lerp amount and the similarity tolerance scale) are
    //   register garbage in the pseudocode and are NOT recoverable from this site alone.
    //   Guessing them would silently set the camera-height smoothing rate. The one store the
    //   asm DOES pin at that address (`*(this+640) = 4.0` == mHeight.mfTarget) is performed
    //   through the smoother's own named setter, so the height TARGET is right and only the
    //   smoothing RATE is left at whatever Construct left it.
    //   CONSEQUENCE: the fly-by camera's height chases its 4.0 target with an unseeded lerp
    //   amount. Cosmetic until Update's prepared leg runs.
    //   DELETE-WHEN: TransitionSmoother::Set's three lerp arguments are read off a second call
    //   site (the other callers of @0x821F22A0).
    mHeight.SetTarget(4.0f);                       // *(this+640) = 4.0

    mfFixationAmount              = 0.0f;          // *(this+608)
    mfFixationBlendTimeReciprocal = 0.125f;        // *(this+612)
    mfFixationStartDistance       = 100.0f;        // *(this+616)
    mfFixationEndDistance         = 120.0f;        // *(this+620)

    mfCurrentModeTime             = 0.0f;          // *(this+628)
    mfCurrentModeDuration         = 30.0f;         // *(this+632)

    mfDesiredSpeed                = mfDirection * 3.0f;   // *(this+660) = *(this+668) * 3.0
    mfSpeedBlendAmount            = 0.0099999998f; // *(this+664)

    mfDesiredBankingScale         = 0.0f;          // *(this+672)
    mfBankingScale                = 0.15000001f;   // *(this+676)
    mfBankingScaleBlendAmount     = 0.1f;          // *(this+680)

    mfTimeSinceLastCollisionStarted = 0.0f;        // *(this+684)
    mfTimeSinceLastCollision        = 0.0f;        // *(this+688)

    meMode              = E_MODE_LOW_SLOW;         // *(this+696) = 0
    mbHasFixation       = false;                   // *(this+702)
    mbStartingFixation  = false;                   // *(this+703)
    mbFixationIsValid   = false;                   // *(this+704)

    return true;                                   // li r3, 1 (unconditional)
}

// ============================================================================
// BehaviourRoadRunner::Reverse (BrnBehaviourRoadRunner.h:331)
//
// Turn the fly-by round. X360: inlined at BrnArbStateCrashNav::Update's ACTIVE_TURNABOUT case
// (asm @0x8226DF7C), where three `fmuls` against flt_820037C8 (== -1.0) negate, IN THIS ORDER,
// the words at +0x29C, +0x294 and +0xA0 -- which this layout resolves to mfDirection, the
// desired speed, and the truck's live speed. The crash-nav state calls it right after clearing
// the prepared gate, so Update re-seeds the lane walking the other way.
// ============================================================================
void BehaviourRoadRunner::Reverse()
{
    mfDirection    = mfDirection    * -1.0f;                           // +0x29C
    mfDesiredSpeed = mfDesiredSpeed * -1.0f;                           // +0x294
    mTrafficLaneTruck.SetSpeed(mTrafficLaneTruck.GetSpeed() * -1.0f);  // +0x0A0 (truck +0x80)
}

// ============================================================================
// BehaviourRoadRunner::Update @0x82247E98 -- vtable slot 2, the per-frame camera producer.
//
// ⚠️ THE PREPARED LEG IS A DOCUMENTED QUIET GATE. The un-prepared leg below is REAL and is,
// today, the branch the console itself would take.
//
// Console shape:
//     if (mbHasFailed) return true;                                     // early-out, frame 2+
//     if (!mbIsPrepared)
//     {
//         if (!mTrafficLaneTruck.Prepare(*lrInfo.mpWorldMap, lrInfo.<+0x280>))
//         { Behaviour::Fail(lrCamera, 6); return true; }                // <-- TODAY'S BRANCH
//         <first-frame mode seed>; mbIsPrepared = true;
//     }
//     <~280 lines: the two diagonal near-clip line tests, the collision timers, the height
//      smoother, TrafficLaneTruck::Update, camera = truck transform + up*height + right*2.25,
//      SetFOV(95), a VMX sin/cos banking-roll pipeline, the fixation SLerp, the fixation
//      acquisition + occlusion probes, and the camera-shake concat>
//     return true;                                                       // always
//
// WHY THE PREPARED LEG IS GATED, precisely:
//   1. ⭐ THE DATA IS NOT THERE. TrafficLaneTruck::Prepare @0x82247A08 calls
//      WorldMap::GetLanePositionNearestPoint @0x8221CE98, which reads the world's TRAFFIC DATA
//      (BrnTraffic::TrafficData -> Pvs::GetHullIndexForPoint -> hull rungs) and leaves
//      mLanePosition.mbValid == 0 when there is none. WorldMap::LoadData @0x8225F5A0 is itself
//      a documented gate (the GameDataEvent request RECORD SHAPE is unproven -- see
//      Utils/BrnDirectorWorldMap.cpp), so GetTrafficData() returns null and the truck's Prepare
//      fails. Every lane-walk entry point additionally asserts
//      `worldMap->meLoadingState == E_LOADING_STATE_LOADED`, which that same gate never sets.
//      ⇒ THE CONSOLE WOULD TAKE THE FAIL BRANCH TOO. The gated leg is unreachable today even in
//        principle, so gating it costs nothing.
//   2. The banking block is a genuine VMX lane pipeline (~40 vmaddfp against the coefficient
//      tables at unk_82000BD0..C20 computing sin and cos in separate lanes, then a vperm128 /
//      vrlimi128 assembly of a roll matrix), and so are the normalise-with-Newton-Raphson
//      blocks feeding the two line tests. The project rules forbid paraphrasing those scalar-wise.
//   3. It needs LineTestNearestPostBox / LineTestFinePostBox interiors, Utils::CameraShake,
//      Utils::CreateLookAt / SineLerp / CalcNearClipCorners, SceneQueryInterface::LineTestNearest
//      / LineTestFine, and WorldMap::GetInterestingPointNear -- none of which is homed.
//
// CONSEQUENCE, stated plainly: the road-runner behaviour allocates, constructs and prepares,
// the attract state advances to E_STATE_ACTIVE, and the behaviour then FAILS on its first
// Update with ValidityAccount reason 6 -- exactly as retail would with no traffic data loaded.
// The camera it produces stays the one BehaviourHelper::Prepare constructed.
// ⚠️ CONDUCTOR: that means the published attract camera is a DEFAULT-CONSTRUCTED camera at the
// origin. Do NOT call Arbitrator::SetDoAttractMode(true) in a build where the world streams off
// the director camera until the traffic data loads -- a frozen eye outside the city footprint
// empties the PVS set (the streaming wave's documented regression).
//
// DELETE-WHEN: WorldMap::LoadData's GameDataEvent record shape is settled AND the three items
// above are homed. Item 1 is the real blocker and it is the LAST MILE to a moving fly-by.
// ============================================================================
bool BehaviourRoadRunner::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
{
    if (HasFailed())
    {
        return true;                                   // lbz 9(this); bne -> li r3,1
    }

    if (!IsPrepared())
    {
        // The console hands the truck the world map and a Vector3 taken from the shared info at
        // +0x280 (inside mPlayerInfo -- the subject the fly-by seeds its lane from; that
        // member's interior is not mapped yet, see Behaviour.h). Both are gated, and the call
        // fails with no traffic data either way, so the fail branch is taken directly.
        (void)lrInfo;
        Fail(lrCamera, 6);                             // bl Behaviour::Fail(this, camera, 6)
        return true;
    }

    // ⚠️ GATE: the ~280-line prepared body (see the banner).
    return true;                                       // every console exit is `li r3, 1`
}

// ============================================================================
// BehaviourRoadRunner::PostCollisionUpdate @0x8220F850 -- vtable slot 3. Drains the two fine
// line-test post boxes, transforms each hit into the fixation's unit box through
// mWorldToFixation and raises mbOccluded, then ages mfFixationOccludedTime and drops
// mbFixationIsValid once the fixation has been occluded for too long.
//
// ⚠️ QUIET GATE, same three reasons as Update (post-box interiors + a VMX abs/compare block +
// no fixation can exist while Update fails). It only ever NARROWS the fixation state, so a
// no-op leaves the behaviour consistent. The one unconditional store at the tail of the console
// body -- clearing mbStartingFixation -- is reproduced.
// DELETE-WHEN: with Update's prepared leg.
// ============================================================================
bool BehaviourRoadRunner::PostCollisionUpdate(Camera& /*lrCamera*/,
                                              const BehaviourSharedInfo& /*lrInfo*/)
{
    mbStartingFixation = false;
    return true;
}

// ============================================================================
// GetName @0x821FB130 -- a single `lis/addi` of the literal, then blr.
// ============================================================================
const char* BehaviourRoadRunner::GetName() const
{
    return "BehaviourRoadRunner";
}

// ============================================================================
// GetParameters / SetParameters (BrnBehaviourRoadRunner.cpp:1172 / :1185 + the typed :312
// setter). The typed setter is the one the arbitrator states call; the virtual pair is how the
// generic (attribute-driven) NewBehaviour overload reaches it.
// ============================================================================
const Behaviour::Parameters* BehaviourRoadRunner::GetParameters() const
{
    return mpParameters;
}

void BehaviourRoadRunner::SetParameters(const Behaviour::Parameters* lpParameters)
{
    SetParameters(static_cast<const Parameters*>(lpParameters));
}

void BehaviourRoadRunner::SetParameters(const Parameters* lpParameters)
{
    mpParameters = lpParameters;
    SetNotPrepared();   // every behaviour's typed SetParameters drops the prepared gate
}

// ============================================================================
// SetupTweaker (BrnBehaviourRoadRunner.cpp:1200) -- hand this behaviour's editable parameters
// to the dev-tools tweaker.
// ⚠️ QUIET GATE: Utils::Tweaker's authoring surface (the AxisMapping tables) is homed but this
// behaviour's per-parameter registration call list is not recovered, and nothing on the live
// path attaches a tweaker (mbTweakerAttached is only ever raised by the manager's AttachTweaker,
// which no committed caller reaches). Marking the base flag is the one observable effect.
// DELETE-WHEN: the tweaker registration call list is read off the console body.
// ============================================================================
void BehaviourRoadRunner::SetupTweaker(Utils::Tweaker& /*lrTweaker*/)
{
    SetTweakerAttached(true);
}

// ============================================================================
// Parameters::Construct (BrnBehaviourRoadRunner.h:300) -- the base parameter head plus this
// behaviour's own type tag.
// FLAG: the road-runner behaviour-TYPE tag value is NOT attested (the block's serialise walk is
// attested empty and Update never dereferences mpParameters, so no site compares the tag). The
// base head is constructed and the tag deliberately left at the base default rather than
// guessed.
// DELETE-WHEN: a SetParameters assert quoting the tag is found for this behaviour.
// ============================================================================
void BehaviourRoadRunner::Parameters::Construct()
{
    Behaviour::Parameters::Construct();
}

} // namespace Camera
} // namespace BrnDirector
