#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ROAD_RUNNER_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ROAD_RUNNER_H

#include "types.hpp"
#include "rw/math/vpu/types.h"                                     // Matrix44Affine / Vector3
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT (the mbPrepared guards)
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"        // THE Behaviour base
#include "GameSource/Director/Camera/Utils/CameraUtils.h"           // Utils::TransitionSmoother (mHeight)
#include "GameSource/Director/Utils/BrnDirectorWorldMap.h"          // WorldMap::LanePosition (the truck's lane)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.h
//
// BrnDirector::Camera::BehaviourRoadRunner -- THE DJ FLY-BY CAMERA. The attract-mode
// arbitrator state allocates exactly one of these and copies the camera it produces every
// frame; ArbStateRoaming / ArbStateCrashNav drive it too.
//
// It is a "road runner": a virtual truck (TrafficLaneTruck) drives itself along the world's
// traffic-lane graph, and the camera rides it -- offset up by a smoothed height and sideways
// by a fixed 2.25, banked by the truck's yaw rate, optionally slerped toward an "interesting
// point" fixation, and finally shaken. Nothing else moves the camera.
//
// ⭐ RE-BASED (2026-07-29): this class used to be a raw-offset SLICE with a `void* mpVTable`
// head and four reserved byte spans; it now derives from the canonical
// BrnDirector::Camera::Behaviour and carries the DWARF member list by name. That is what lets
// BehaviourManager::NewBehaviour<BehaviourRoadRunner> allocate it, BehaviourHelper::Prepare
// dispatch its Construct, and PrepareBehaviours dispatch its Prepare.
//
// LAYOUT AUTHORITY: the DECFIGS DWARF (BrnBehaviourRoadRunner.h:146, members :230..:280) plus
// the X360 Construct @0x8222BCE0 / Prepare @0x8220F748 / Update @0x82247E98 asm. The base
// occupies +0x00..+0x13, so mpParameters lands at +0x14 -- which Construct's last store
// (`*(this + 20) = 0`) pins exactly. Every other console offset quoted below is a store or
// load in one of those three bodies. x64: parity is BY NAMED MEMBER (pointers widen).
//
// ⭐ THREE "un-recovered" crash-nav accessors are now IDENTIFIED (they were modelled as
// anonymous lanes in the retired slice, at offsets that this layout resolves):
//     +0x08  "mbDontSmoothNextSample"  == the BASE mbIsPrepared    -> SetNotPrepared()
//     +0x09  "mbHasFinished"           == the BASE mbHasFailed     -> HasFailed()
//     +0xA0  "mfReverseLaneA"          == mTrafficLaneTruck.mfSpeed
//     +0x294 "mfReverseLaneB"          == mfDesiredSpeed
//     +0x29C "mfReverseLaneC"          == mfDirection
// -- i.e. the crash-nav "turnabout" is `SetNotPrepared(); Reverse();`, and Reverse (DWARF
// BrnBehaviourRoadRunner.h:331) negates the direction, the desired speed and the truck's live
// speed. The fly-by turns round and re-Prepares its lane. That is a real recovery, not a rename.
// ============================================================================

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// TrafficLaneTruck -- the virtual vehicle the road-runner camera rides. It walks the
// WorldMap's traffic-lane graph and publishes a world transform plus its velocities.
//
// Layout: DWARF (BrnBehaviourRoadRunner.h:124..:133) + the X360 accessors' load offsets. All
// members are size-stable (no pointers), so the offsets hold on the host too and the .cpp
// still pins them with offsetof.
// ----------------------------------------------------------------------------
class TrafficLaneTruck
{
public:
    // Seed the truck from the lane nearest a world point. @0x82247A08:
    //     WorldMap::GetLanePositionNearestPoint(&mLanePosition, lrWorldMap, lPoint);
    //     if (!mLanePosition.mbValid) return false;                 // lbz 0x1E; li r3,0
    //     ... CalcTransformFromLanePosition ... mbPrepared = true;
    // ✅ BODIED 2026-07-29 (the lane graph loads now -- see BrnDirectorWorldMap.cpp).
    bool Prepare(const BrnDirector::WorldMap& lrWorldMap, rw::math::vpu::Vector3 lPoint);

    // Rebuild mTransform from the current lane position (@0x8222A640): sample the lane's
    // forward direction at (section, rung, parameter) and make a look-at frame from the lane
    // point toward point + direction. ✅ BODIED 2026-07-29.
    void CalcTransformFromLanePosition(const BrnDirector::WorldMap& lrWorldMap);

    // Advance along the lane graph by mfSpeed * dt (@0x82247AC0). DECLARATION-ONLY.
    void Update(const BrnDirector::WorldMap& lrWorldMap, f32 lfTimeStep, void* lpRandom);

    // Return (by value) the truck's world-space transform. Asserts the truck is prepared.
    // @0x821F53D8: four 16-byte aligned rows copied from +0x20 (lvx128 stride 16) into the sret.
    rw::math::vpu::Matrix44Affine GetTransform() const;

    // Return (by value) the truck's local angular velocity. Asserts the truck is prepared.
    // @0x821F5470: a single 16-byte aligned vector copied from +0x60 into the sret.
    rw::math::vpu::Vector3 GetLocalAngularVelocity() const;

    // Return (by value) the truck's linear velocity. Asserts the truck is prepared.
    // @0x821F54E0: a single 16-byte aligned vector copied from +0x70 into the sret.
    rw::math::vpu::Vector3 GetLinearVelocity() const;

    f32  GetSpeed() const { return mfSpeed; }
    void SetSpeed(f32 lfSpeed) { mfSpeed = lfSpeed; }

    // Public so the .cpp's offsetof pins can verify them (all size-stable -- no pointers
    // intervene, so the layout is exact on the host too).

    // +0x00 (0x20) the lane the truck is walking. Its `mbValid` byte sits at +0x1E -- the
    // console asserts on it by name ("lpPositionInOut->mbValid") in every lane-walk entry
    // point, and Construct's `*(this + 62) = 0` clears exactly that byte.
    BrnDirector::WorldMap::LanePosition mLanePosition;              // :124  +0x00

    rw::math::vpu::Matrix44Affine       mTransform;                 // :125  +0x20  (64B)
    rw::math::vpu::Vector3              mLocalAngularVelocity;      // :126  +0x60
    rw::math::vpu::Vector3              mLinearVelocity;            // :127  +0x70

    f32                                 mfSpeed;                    // :129  +0x80
    f32                                 mfTransformBlendAmount;     // :130  +0x84
    f32                                 mfBlendDistance;            // :131  +0x88
    bool                                mbPrepared;                 // :133  +0x8C
};

// ----------------------------------------------------------------------------
// The two scene-query post boxes the road runner uses. FLAG: neither
// `LineTestNearestPostBox` nor `LineTestFinePostBox` exists as a named type anywhere in the
// tree -- the generic BrnDirector::PostBox<T> (Utils/BrnPostBox.h) is instantiated for the
// line-test result events, but the DWARF names these two aliases and the road runner embeds
// them BY VALUE. Their console sizes are pinned by the gaps between the members either side
// (nearest: +0xB0 -> +0x100 == 0x50; fine: +0x150 -> +0x158 == 8). Modelled as named, sized
// sub-objects until the post-box aliases are homed; the .cpp NEVER reaches inside one.
// DELETE-WHEN: Utils/BrnPostBox.h grows the two aliases.
// ----------------------------------------------------------------------------
struct LineTestNearestPostBox { u8 maOpaque[0x50]; };   // FLAG: size console-pinned, interior un-homed
struct LineTestFinePostBox    { u8 maOpaque[0x08]; };   // FLAG: size console-pinned, interior un-homed

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourRoadRunner
// ----------------------------------------------------------------------------
class BehaviourRoadRunner : public Behaviour
{
public:
    // DWARF BrnBehaviourRoadRunner.h:150. NOTE the console's own enumerator VALUES -- the
    // "count" enumerator sits at 1, in the middle of the range. Reproduced verbatim.
    enum EMode
    {
        E_MODE_LOW_SLOW       = 0,
        E_MODE_COUNT          = 1,
        E_MODE_HIGH_SLOW      = 2,
        E_MODE_LOW_FAST       = 3,
        E_MODE_VERY_HIGH_SLOW = 4
    };

    // ------------------------------------------------------------------------
    // The "road runner" parameter block (DWARF BrnBehaviourRoadRunner.h:296, deriving
    // Behaviour::Parameters).
    //
    // FLAG: the text-serialise field-walk for this block is ATTESTED EMPTY. The X360
    //   instantiation @0x82214E78 emits only the section-header label line + recursion-depth
    //   accounting; it discards the parameter-block register (mr r5,r4 overwrites the params
    //   ptr before FormatName) and makes NO `bl` to any inner Parameters::Serialise field
    //   walker -- the compiler inlined the inner visitor to nothing because it serialises zero
    //   fields to text. The visitor below is therefore an empty (zero-field) walk, faithful to
    //   the attested asm; NO field offsets are fabricated. Corroborating: Update @0x82247E98
    //   NEVER dereferences mpParameters -- every tunable it uses is a .data global -- so the
    //   block's field set is genuinely unexercised on the live path.
    // ------------------------------------------------------------------------
    class Parameters : public Behaviour::Parameters
    {
    public:
        // X360 visitor: `void Serialise<S>(S&)` for the camera-tunings serialiser S. Attested
        // EMPTY for the text writer (see the class FLAG): walks zero fields.
        template<class TSerialiser> void Serialise(TSerialiser& /*lrSerialiser*/) {}

        void Construct();   // BrnBehaviourRoadRunner.h:300
    };

    // ---- Behaviour overrides (vtable order -- see Behaviour.h) ---------------
    void        Construct() override;                                                    // @0x8222BCE0
    bool        Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo) override;        // @0x8220F748
    bool        Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo) override;     // @0x82247E98
    bool        PostCollisionUpdate(Camera& lrCamera, const BehaviourSharedInfo& lrInfo) override; // @0x8220F850
    const Behaviour::Parameters* GetParameters() const override;                          // :1172
    void        SetParameters(const Behaviour::Parameters* lpParameters) override;        // :1185
    void        SetupTweaker(Utils::Tweaker& lrTweaker) override;                         // :1200
    const char* GetName() const override;                                                 // @0x821FB130

    // ---- public API (DWARF BrnBehaviourRoadRunner.h) -------------------------
    void Reset();                                    // :162 (declaration-only)
    void Reverse();                                  // :331
    void SetParameters(const Parameters* lpParameters);  // :312

    bool HasJustStartedColliding() const { return mbIsColliding && !mbWasCollidingLastFrame; }   // :209
    bool HasJustStoppedColliding() const { return !mbIsColliding && mbWasCollidingLastFrame; }   // :213
    bool IsColliding() const            { return mbIsColliding; }                                // :217
    void SetFixationsAllowed(bool lbAllowed) { mbFixationsAllowed = lbAllowed; }                 // :222

    // ---- the crash-nav call surface, now expressed through the recovered members -----------
    // (BrnArbStateCrashNav drives the fly-by through these; see the ⭐ note in the file banner
    // for how each maps onto the real member the console touches.)

    // The behaviour is done with this take. X360 `lbz +0x09` == the BASE failed flag.
    bool HasFinished() const { return HasFailed(); }

    // Whether the fade this take wants is the black-IN variant. X360 `lbz +0x2B4`, which this
    // layout resolves to mbIsColliding.
    // FLAG: the ROLE mapping (a fade-direction selector vs "the fly-by is currently colliding")
    //   is not proven -- only the OFFSET is. The crash-nav state asks for "Black_In_BW" when
    //   the byte is set, and "the fly-by just hit something" is a plausible trigger for that.
    //   DELETE-WHEN: BrnArbStateCrashNav::Update @0x8226DC98 is re-walked against this layout.
    bool ShouldFadeBlackIn() const { return mbIsColliding; }

    // Crash-nav "turnabout" (X360 ACTIVE_TURNABOUT case, asm @0x8226DF7C): drop the prepared
    // gate so Update re-seeds the truck's lane next frame, then reverse the travel direction.
    void ClearSmoothingForNextSample() { SetNotPrepared(); }   // stb 0, +0x08 == mbIsPrepared
    void ReverseTravelDirection()      { Reverse(); }

private:
    void PickNewMode();                               // :1113 (declaration-only)

    // ===== LAYOUT (DWARF :230..:280; console offsets are provenance) =====
    const Parameters*              mpParameters;                    // :230  +0x014
    TrafficLaneTruck               mTrafficLaneTruck;               // :232  +0x020
    LineTestNearestPostBox         mTopLeftBottomRight;             // :234  +0x0B0
    LineTestNearestPostBox         mTopRightBottomLeft;             // :235  +0x100
    LineTestFinePostBox            mLineTestFineA;                  // :237  +0x150
    LineTestFinePostBox            mLineTestFineB;                  // :238  +0x158

    rw::math::vpu::Vector3         mFixationPoint;                  // :240  +0x160
    rw::math::vpu::Vector3         mDebugFixationHalfExtents;       // :241  +0x170
    rw::math::vpu::Matrix44Affine  mDebugFixationTransform;         // :242  +0x180
    rw::math::vpu::Matrix44Affine  mWorldToFixation;                // :243  +0x1C0
    rw::math::vpu::Matrix44Affine  mLastShakeTransform;             // :245  +0x200

    // FLAG (opaque, console-sized): Utils::CameraShake and its Parameters are homed -- but
    //   only inside BehaviourRig.h, which also carries a private CollisionPolicy fork that
    //   collides with BrnBehaviourIceAnim.h's in any TU pulling both (a PRE-EXISTING defect;
    //   BrnBehaviourManager.cpp is exactly such a TU). Including BehaviourRig.h here would
    //   propagate that collision to every road-runner consumer. Modelled as named sub-objects
    //   at their console-pinned sizes (shake 0x10, params 0x10) until CameraShake gets a home
    //   of its own; the .cpp NEVER reaches inside either.
    //   DELETE-WHEN: Utils::CameraShake moves to Camera/Utils/BrnCameraShake.h.
    struct OpaqueShake       { u8 maOpaque[0x10]; };
    struct OpaqueShakeParams { u8 maOpaque[0x10]; };
    OpaqueShake                    mShake;                          // :247  +0x240
    OpaqueShakeParams              mShakeParams;                    // :248  +0x250

    f32                            mfFixationAmount;                // :250  +0x260
    f32                            mfFixationBlendTimeReciprocal;   // :251  +0x264
    f32                            mfFixationStartDistance;         // :252  +0x268
    f32                            mfFixationEndDistance;           // :253  +0x26C
    f32                            mfFixationOccludedTime;          // :254  +0x270

    f32                            mfCurrentModeTime;               // :256  +0x274
    f32                            mfCurrentModeDuration;           // :257  +0x278

    Utils::TransitionSmoother      mHeight;                         // :259  +0x27C

    f32                            mfDesiredSpeed;                  // :261  +0x294
    f32                            mfSpeedBlendAmount;              // :262  +0x298
    f32                            mfDirection;                     // :263  +0x29C

    f32                            mfDesiredBankingScale;           // :265  +0x2A0
    f32                            mfBankingScale;                  // :266  +0x2A4
    f32                            mfBankingScaleBlendAmount;       // :267  +0x2A8

    f32                            mfTimeSinceLastCollisionStarted; // :269  +0x2AC
    f32                            mfTimeSinceLastCollision;        // :270  +0x2B0
    bool                           mbIsColliding;                   // :271  +0x2B4
    bool                           mbWasCollidingLastFrame;         // :272  +0x2B5

    EMode                          meMode;                          // :274  +0x2B8

    bool                           mbFixationsAllowed;              // :276  +0x2BC
    bool                           mbOccluded;                      // :277  +0x2BD
    bool                           mbHasFixation;                   // :278  +0x2BE
    bool                           mbStartingFixation;              // :279  +0x2BF
    bool                           mbFixationIsValid;               // :280  +0x2C0
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ROAD_RUNNER_H
