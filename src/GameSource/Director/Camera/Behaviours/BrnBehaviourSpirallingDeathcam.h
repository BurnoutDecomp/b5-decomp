#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_SPIRALLING_DEATHCAM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_SPIRALLING_DEATHCAM_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                         // Vector3 / Vector4
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT (the Start !mbStarted tripwire)
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"        // THE canonical Camera::Behaviour base
#include "GameSource/Director/Camera/BrnCollisionPolicy.h"          // CollisionPolicyAttachedToVehicle
#include "GameSource/Director/Camera/Utils/BrnLooker.h"             // Utils::Looker (+ ::Parameters, by value)
#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"        // Utils::CameraShake (+ ::Parameters, by value)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourSpirallingDeathcam.h
//
// BrnDirector::Camera::BehaviourSpirallingDeathcam -- the post-wreck "spiralling deathcam":
// once the player's car is totalled (Road Rage / Marked Man: the last crash) the crashing
// arbitrator state swaps its crash camera for this one, which parks an attachment point on
// the car, orbits it about the world Y axis at a radius/height that grow over time, looks at
// the car through a Looker, shakes, blurs, and asks for the "Player_Shutdown" screen hook.
//
// Owner: ArbStateCrashing (NewBehaviour<BehaviourSpirallingDeathcam> @0x822655E8, Start from
// its Update, Release at teardown) and ArbStateTestbed.
//
// ⭐ REBUILT 2026-09-17. Until then this class had NO Behaviour base: it was a hollow shell
//   modelling only the two members Prepare()/Start() wrote, at console offsets. Nothing
//   reached it because the director's action-205 arm (the road-rage damage action) was
//   missing, so mbRoadRageTotalled never went true. The moment that arm landed, the fourth
//   crash of a Road Rage allocated this behaviour and BehaviourHelper::Prepare dispatched
//   Behaviour::Construct through a vtable the object did not have -- the
//   EXCEPTION_ACCESS_VIOLATION reading 0x0 the owner reported. This is an instance of the
//   "hollow-shell class" defect class: the DecFIGS DWARF for this header (:52) says
//   `struct BehaviourSpirallingDeathcam : public Behaviour` with the member list below.
//
// LAYOUT (X360, from Construct @0x8222C088 / Update @0x8224A528, member NAMES from the DWARF
// :135..:149; host offsets differ by the widened pointers, so nothing here is reached by
// displacement):
//   +0x000 Behaviour base (vtable, meTimestepType, the five flags, mpcDebugParametersName)
//   +0x020 mCollisionPolicy   CollisionPolicyAttachedToVehicle (Construct(this+0x20, 0))
//   +0x270 mVectorData        two Vector4 lanes-as-fields (see VectorData)
//   +0x290 mAttachmentPos     Vector3
//   +0x2A0 mLooker            Utils::Looker (0x20)
//   +0x2C0 mShake             Utils::CameraShake (0x10)
//   +0x2D0 mpParameters       const Parameters*
//   +0x2D4 mAttachment        Behaviour::VehicleRef (0x10)
//   +0x2E4 mfBlurAmount       f32
//   +0x2E8 mfShakeAmount      f32
//   +0x2EC mfLookOffsetFactor f32
//   +0x2F0 mfRunningTime      f32
//   +0x2F4 mbStarted          bool
// (760 bytes on the console; the manager's small pool (1600-byte buckets) holds it.)
//
// VTABLE (DWARF): Construct @0x8222C088, Prepare(info) @0x821FB3F8, Update @0x8224A528,
// GetCollisionPolicy (cpp:225, ICF-folded: `addi r3, r3, 0x20`), GetName @0x821FB600. No
// X360 export exists for SetupTweaker (cpp:266) -- the base default stands and it is flagged
// in the .cpp. The DWARF also lists virtual Get/SetParameters(Behaviour::Parameters*); the PC
// base holds NO slot for that pair (see Behaviour.h), so the typed SetParameters below hides
// the base's non-virtual one, exactly as the sibling behaviours do.
// ----------------------------------------------------------------------------
namespace BrnDirector
{
namespace Camera
{

class BehaviourSpirallingDeathcam : public Behaviour
{
public:
    // ------------------------------------------------------------------------
    // The deathcam parameter block. HOME here because the ledger nests it under this behaviour
    // (BrnDirector::Camera::BehaviourSpirallingDeathcam::Parameters) and the camera-tunings bank
    // saves it through TextFileWriteSerialiser::Serialise<Parameters> @0x82214DE0.
    //
    // FLAG: the text-serialise field-walk for this block is ATTESTED EMPTY. The X360 instantiation
    //   @0x82214DE0 emits only the section-header label line + recursion-depth accounting; it
    //   discards the parameter-block register (mr r5,r4 overwrites the params ptr before FormatName)
    //   and makes NO `bl` to any inner Parameters::Serialise field walker -- i.e. the compiler
    //   inlined the inner visitor to nothing because it serialises zero fields to text. The
    //   visitor below is therefore an empty (zero-field) walk, faithful to the attested asm; NO
    //   field offsets are fabricated.
    //
    // MEMBER NAMES AND ORDER are the DecFIGS DWARF for this file (:158..:176), verbatim;
    // DEFAULTS are Parameters::Construct @0x821FB498, store for store. The pin is that the
    // thirteen trailing floats the asm writes and the DWARF's thirteen trailing names line up
    // one-to-one in ascending offset:
    //   +0x7C 60.0  mfRotationSpeedDegs            +0x80 0.5  mfHeightIncreaseSpeed
    //   +0x84 0.0   mfRadiusIncreaseSpeed          +0x88 0.5  mfInitialHeight
    //   +0x8C 1.5   mfInitialRadius                +0x90 4.0  mfTimeBeforeFullLookOffsetSpeed
    //   +0x94 1.0   mfLookOffsetSpeed              +0x98 15.0 mfHeightIncreaseDuration
    //   +0x9C 5.0   mfRotationSpeedDecreaseRate    +0xA0 20.0 mfMinRotationSpeed
    //   +0xA4 0.8   mfMinAttachAmount              +0xA8 7.0  mfBlurTime
    //   +0xAC 7.0   mfShakeTime
    // ------------------------------------------------------------------------
    class Parameters
    {
    public:
        // The behaviour type tag SetParameters asserts on (asm: `cmplwi r11, 0x13` == 19).
        s32 GetType() const { return meType; }

        // @0x821FB498 -- seed the block to its authored defaults. Body in the .cpp.
        void Construct();

        // X360 visitor: `void Serialise<S>(S&)` for the camera-tunings serialiser S. Attested
        // EMPTY for the text writer (see the class FLAG): walks zero fields. Templated inline so
        // TextFileWriteSerialiser::Serialise<Parameters>'s odr-use inlines it away, matching the
        // degenerate instantiation asm (no inner field-walk call).
        template<class TSerialiser> void Serialise(TSerialiser& /*lrSerialiser*/) {}

        // ---- Behaviour::Parameters base head ----
        s32 meType;                                // +0x00  = eBehaviourSpirallingDeathcam (19)
        s32 miBaseField04;                         // +0x04  base param word (cleared to 0)

        // ---- the embedded sub-blocks (DWARF :158 / :159) ----
        Utils::Looker::Parameters      mLookerParams;  // +0x08  (seeded by its own Construct)
        Utils::CameraShake::Parameters mShakeParams;   // +0x6C  (0.06 / 0.0 / 1.15 / 0.11)

        // ---- the deathcam's own tunables (DWARF :161..:176) ----
        f32 mfRotationSpeedDegs;                   // +0x7C
        f32 mfHeightIncreaseSpeed;                 // +0x80
        f32 mfRadiusIncreaseSpeed;                 // +0x84
        f32 mfInitialHeight;                       // +0x88
        f32 mfInitialRadius;                       // +0x8C
        f32 mfTimeBeforeFullLookOffsetSpeed;       // +0x90
        f32 mfLookOffsetSpeed;                     // +0x94
        f32 mfHeightIncreaseDuration;              // +0x98
        f32 mfRotationSpeedDecreaseRate;           // +0x9C
        f32 mfMinRotationSpeed;                    // +0xA0
        f32 mfMinAttachAmount;                     // +0xA4
        f32 mfBlurTime;                            // +0xA8
        f32 mfShakeTime;                           // +0xAC
    };

    // ------------------------------------------------------------------------
    // The orbit state (DWARF :105..:132): two Vector4s whose LANES are the fields. The console
    // keeps them as VMX registers and reaches every field through a VecFloatRef accessor
    // (`vrlimi128` lane inserts in Construct / Update); the accessors below name the same
    // lanes. mVector1 = { YRotationDegs, YRotationSpeedDegsPS, Height, HeightIncreaseSpeed },
    // mVector2 = { Radius, RadiusIncreaseSpeed, LookOffsetAmount, (unused) }.
    // ------------------------------------------------------------------------
    struct VectorData
    {
        f32&       YRotationDegs()             { return mVector1.x; }   // :109  lane X of mVector1
        f32&       YRotationSpeedDegsPS()      { return mVector1.y; }   // :112  lane Y
        f32&       Height()                    { return mVector1.z; }   // :115  lane Z
        f32&       HeightIncreaseSpeed()       { return mVector1.w; }   // :118  lane W
        f32&       Radius()                    { return mVector2.x; }   // :121  lane X of mVector2
        f32&       RadiusIncreaseSpeed()       { return mVector2.y; }   // :124  lane Y
        f32&       LookOffsetAmount()          { return mVector2.z; }   // :127  lane Z
        const f32& YRotationDegs()       const { return mVector1.x; }
        const f32& YRotationSpeedDegsPS() const { return mVector1.y; }
        const f32& Height()              const { return mVector1.z; }
        const f32& HeightIncreaseSpeed() const { return mVector1.w; }
        const f32& Radius()              const { return mVector2.x; }
        const f32& RadiusIncreaseSpeed() const { return mVector2.y; }
        const f32& LookOffsetAmount()    const { return mVector2.z; }

        Vector4 mVector1;   // :131  console +0x270
        Vector4 mVector2;   // :132  console +0x280
    };

    // The behaviour type tag this class's parameter blocks carry. The VALUE is asm
    // (SetParameters @0x821F5680 compares the block's first word against 0x13).
    enum EBehaviourTypeSpirallingDeathcam
    {
        eBehaviourSpirallingDeathcam = 19
    };

    // ---- the Behaviour virtual interface (see the vtable note in the banner) ----
    void Construct() override;                                                      // @0x8222C088
    bool Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo) override;         // @0x821FB3F8
    bool Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo) override;      // @0x8224A528
    CollisionPolicy* GetCollisionPolicy() override;                                 // cpp:225
    const char* GetName() const override;                                           // @0x821FB600

    // Adopt an authored parameter block. @0x821F5680: assert the block's type tag, then store
    // the pointer (console +0x2D0). Hides Behaviour::SetParameters (no base slot; see banner).
    void SetParameters(const Parameters* lpParameters);
    const Parameters* GetParameters() const { return mpParameters; }                // cpp:238

    // Has this activation already been started (DWARF h:96). ArbStateCrashing::Update gates its
    // Start() call on exactly this byte (`lbz r11, 0x2F4(behaviour)`).
    bool HasStarted() const { return mbStarted; }

    // Latch the "started" flag for this activation. @0x821F5620: asserts !mbStarted (the behaviour
    // must not be double-started), then stores mbStarted = 1.
    void Start();

private:
    CollisionPolicyAttachedToVehicle mCollisionPolicy;   // :135  console +0x020
    VectorData                       mVectorData;        // :137  console +0x270
    Vector3                          mAttachmentPos;     // :138  console +0x290
    Utils::Looker                    mLooker;            // :139  console +0x2A0
    Utils::CameraShake               mShake;             // :140  console +0x2C0
    const Parameters*                mpParameters;       // :141  console +0x2D0
    Behaviour::VehicleRef            mAttachment;        // :142  console +0x2D4
    f32                              mfBlurAmount;       // :144  console +0x2E4
    f32                              mfShakeAmount;      // :145  console +0x2E8
    f32                              mfLookOffsetFactor; // :146  console +0x2EC
    f32                              mfRunningTime;      // :147  console +0x2F0
    bool                             mbStarted;          // :149  console +0x2F4
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_SPIRALLING_DEATHCAM_H
