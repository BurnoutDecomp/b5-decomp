#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BEHAVIOUR_BYSTANDER_CAM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BEHAVIOUR_BYSTANDER_CAM_H

#include "types.hpp"
#include "BrnCommonTypes.h"                            // Vector3 / Matrix44Affine / VecFloat aliases
#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT (the mpParameters / mbSetup asserts)
#include "GameShared/GameClasses/Numeric/CgsRandom.h"  // CgsNumeric::Random (Camera::Utils::Random typedef)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BehaviourBystanderCam.h
//
// BrnDirector::Camera::BehaviourBystanderCam -- the "bystander cam" camera behaviour: the
// dramatic third-party camera that frames a chosen race car from a roadside vantage. This
// header is the HOME for the full-behaviour slice of this class (its construction, the per-frame
// Update, the dev-tools tweaker hookup, the Prepare gate, the name accessor) AND for the two
// impact-effect controllers declared alongside it in the original .cpp:
//   - BehaviourBystanderCam::Construct           @0x822438E8  (virtual; seed the rig + RNG + sub-objects)
//   - BehaviourBystanderCam::Prepare             @0x821F9AD0  (virtual; assert mbSetup, mark prepared)
//   - BehaviourBystanderCam::Update              @0x82243C80  (virtual; the per-frame camera solve)
//   - BehaviourBystanderCam::GetName             @0x821F9C78  (virtual; "BehaviourBystanderCam")
//   - BehaviourBystanderCam::SetupTweaker        @0x821F9B30  (virtual; wire Rig X/Y/Z to the tweaker)
//   - BehaviourBystanderCam::Parameters::Construct @0x821F9A00 (seed the param block defaults)
//   - ImpactSlomoController::Update              @0x82227230  (drive the crash slow-mo state machine)
//   - ImpactShakeController::Update              @0x82243720  (register a velocity-scaled impact shake)
//
// The trivial in-header slices (GetCol / SetParameters / SetTarget) and the virtual
// GetCollisionPolicy are owned by the sibling TU BrnBehaviourBystanderCam.h, which models the
// SAME class as a partial layout. To avoid forking that committed type, THIS header is a
// SELF-CONTAINED layout used only by this compile unit (the two reconstructions agree
// store-for-store on every pinned offset: mpParameters @+0x350, mCollisionPolicy @+0xD0,
// mTarget @+0x340, mbSetup @+0x35C, mbGotPosition @+0x35D, the RNG @+0x310).
//
// Heavy embedded sub-objects (PositionFinder, CameraShake, Looker, the collision policy, the
// CgsNumeric::Random RNG, mTransform) are modelled as opaque reserved byte spans at their
// asm-attested offsets; the cross-TU helpers that operate on them (Looker::Update,
// CameraShake::Update, PositionFinder::FindPosition/Update, VehicleRef::Get/IsValid,
// Timestep::Get, Camera::ValidateTransformWithDebugInfo, the impact-effect helpers) are declared
// as free functions on opaque handles -- their real homes land with their own TUs; under the
// per-TU `cl /c` gate only the declaration is load-bearing.
//
// SIZE-STABLE PIN: the X360 is a 4-byte-pointer build; this PC reconstruction is 64-bit, so a
// real 8-byte pointer mid-struct would shift every later offset. mpParameters is therefore kept
// as a size-stable 32-bit slot at its pinned +0x350 and re-exposed by name through an accessor
// that round-trips the opaque handle; the embedded reserved spans reproduce every offset exactly.
// ----------------------------------------------------------------------------

// ---- cross-TU types this behaviour threads through. Their real homes land with their own TUs;
//   under the per-TU `cl /c` gate only these opaque forward-declarations are load-bearing. They
//   appear only by reference/pointer here, so an incomplete type suffices and nothing is forked. ----
namespace BrnDirector
{

// ⚠️ CLASS-KEY: `struct`, matching the real home (BrnDirectorModuleDebugPrinter.h:40) and
// every other forward declaration (Behaviour.h:92, BrnBehaviourManager.h:71,
// BrnCameraTweaker.h:38). It used to be `class` here, and MSVC mangles the class-key into the
// symbol: any TU that saw THIS header first emitted `AEAVDebugPrinter` while the
// BehaviourManager TU emitted `AEAUDebugPrinter`, so BehaviourManager::UpdateAllBehaviours
// came up unresolved at link (the same fork class the renderer wave hit on rw::IResourceAllocator).
struct DebugPrinter;                // dev print sink (threaded only)
class AllVehicleData;               // the per-frame all-vehicle data (GetPlayer source)
class VehicleTracker;               // a vehicle's per-frame tracker (velocity journal owner)
class VehicleRef;                   // a resolved race-car handle (Get/IsValid source)
struct BehaviourSharedInfo;         // per-frame shared info passed to Update
struct BehaviourSharedPrepareReleaseInfo; // shared info passed to Prepare/Release

namespace Camera
{

class Camera;                 // the camera being driven (its flags/transform live in the shared info)

namespace Utils
{
    struct Tweaker;           // dev-tools tweaker SetupTweaker wires the rig members into
                              // (`struct` per its real home Camera/Utils/BrnCameraTweaker.h:45 --
                              //  the class-key is part of the MSVC mangled name)
    // The randomised-offset RNG. DE-FORKED 2026-07-30: this used to be `class Random;`, a
    // forward declaration of a class that does not exist -- Camera::Utils::Random is a TYPEDEF
    // of CgsNumeric::Random (see Utils/BrnCameraShake.h:51 and Utils/BrnLooker.h:31, which both
    // spell it that way). The two forms are not interchangeable: a TU that saw both was C2371
    // ("redefinition; different basic types"), which is what BrnArbStateTakedown.cpp hit the
    // moment the ICE-anim behaviour started including the real CameraShake/Looker homes. A
    // repeated typedef to the same type is well-formed, so this now matches them exactly.
    typedef CgsNumeric::Random Random;
}

// FLAG: the camera-behaviour type tag. Each behaviour carries a type id in the leading word of
//   its Parameters block; the bystander-cam enumerator's console VALUE is 5 (asm: the sibling
//   SetParameters @0x821F3F30 compares the block's first word against 5; Parameters::Construct
//   here stores 5 into the leading word). Replace with the real EBehaviourType enum when the
//   Behaviour base TU lands.
enum EBehaviourTypeBystanderCam
{
    eBehaviourBystanderCam = 5
};
// ----------------------------------------------------------------------------
// ⭐ THE TWO IMPACT CONTROLLERS MOVED OUT (2026-08-29, crash-camera wave), to
//     Behaviours/BehaviourBystanderCamImpactControllers.h
// They are still DWARF-homed in this file (:44 and :75) and nothing about them changed
// conceptually -- but ArbStateCrashing embeds BOTH by value, its header is #included by the
// arbitrator state CONTAINER, and the container is reached by TUs that already reach the OTHER
// reconstruction of this same source file (BrnBehaviourBystanderCam.h, via
// BrnBehaviourParameterBank.h). Two definitions of BrnDirector::Camera::BehaviourBystanderCam
// in one TU is a hard C2011, so dragging this whole header in behind the container would have
// detonated that fork across the build. The controllers share no member with the behaviour, so
// carving just them out costs nothing and leaves both reconstructions untouched.
// ⚠️ ImpactShakeController is 20 BYTES there, not the 68 this file used to model
// (`f32 mfImpactScalar + u8 maImpactEffect[0x40]`, itself FLAGged as a guess): the DWARF gives
// the class exactly one member, a CameraImpactEffect, and ArbStateCrashing::ApplySlomoAndShake
// clears exactly five floats over it.
// The two Update BODIES moved with them, to the matching .cpp partfile -- see the banner in
// BehaviourBystanderCam.cpp for why they could not stay here.
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// The bystander-cam behaviour. DWARF BehaviourBystanderCam.h:107 (public Behaviour base).
// ----------------------------------------------------------------------------
class BehaviourBystanderCam
{
public:

    // The bystander-cam parameter block (DWARF :181, public Behaviour::Parameters). Layout +
    // defaults pinned store-for-store from Parameters::Construct @0x821F9A00.
    class Parameters
    {
    public:
        EBehaviourTypeBystanderCam GetType() const
        {
            return static_cast<EBehaviourTypeBystanderCam>(meType);
        }

        // Reset every field to its construction default (asm @0x821F9A00).
        void Construct();

        // ---- Behaviour::Parameters base head (DWARF :181) ----
        s32 meType;                                  // +0x00  type tag (= eBehaviourBystanderCam, 5)
        s32 miBaseField04;                           // +0x04  base param word (cleared to 0)

        // ---- mShakeParams (CameraShake::Parameters, DWARF :184), 4 floats @+0x08..+0x17 ----
        f32 mfShakeParam08;                          // +0x08  stfs 0.0 (was 0.06)
        f32 mfShakeParam0C;                          // +0x0C  stfs 0.0
        f32 mfShakeParam10;                          // +0x10  stfs 1.0 (was 1.15)
        f32 mfShakeParam14;                          // +0x14  stfs 0.25 (was 0.11)

        // ---- mLookerParams (Looker::Parameters, DWARF :185), 0x64 bytes @+0x18..+0x7B.
        //   Seeded by Looker::Parameters::Construct(this+0x18); only the two fields this TU
        //   over-writes after that call are named, the rest is the looker's own default block. ----
        u8  maLookerParams[0x7C - 0x18];             // +0x18  Looker::Parameters (opaque sub-block)

        // ---- bystander-specific scalars (DWARF :187..:198) ----
        f32 mfVelocityInfluenceOnPosition;           // +0x7C  stfs 0.5
        f32 mfMaxInitialDistanceKM;                  // +0x80  stfs 40.0
        f32 mfDistanceForFailKM;                     // +0x84  stfs 80.0
        f32 mfTargetSpaceX;                          // +0x88  stfs 2.0
        f32 mfTargetSpaceY;                          // +0x8C  stfs 0.0
        f32 mfTargetSpaceZ;                          // +0x90  stfs 0.0
        f32 mfHeight;                                // +0x94  stfs 1.0
        bool mbUseTargetSpaceInsteadOfPositionFinder;// +0x98  stb 0
        bool mbUseRangeTesting;                      // +0x99  stb 1
    };

    // The embedded collision-policy sub-object GetCollisionPolicy returns (DWARF :162). Concrete
    // type lands with the collision-policy TU; opaque here so the accessor types the +0xD0 address.
    class CollisionPolicy;

    BehaviourBystanderCam() {}

    // ---- the virtual behaviour API this TU bodies (declared virtual to match the vtable) ----
    virtual void Construct();                                   // @0x822438E8
    virtual bool Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo); // @0x821F9AD0
    virtual bool Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo); // @0x82243C80
    virtual CollisionPolicy* GetCollisionPolicy();             // owned by sibling TU (decl only)
    virtual void SetupTweaker(Utils::Tweaker& lrTweaker);      // @0x821F9B30
    virtual const char* GetName() const;                      // @0x821F9C78

    // The adopted parameter block, typed by name (round-trips the size-stable +0x350 slot).
    const Parameters* GetParameters() const
    {
        return static_cast<const Parameters*>(mpParametersSlot);
    }

private:

    // FLAG: full layout pinned from the Construct/Update/Prepare/SetupTweaker asm. Heavy embedded
    //   sub-objects are opaque reserved spans at their pinned offsets. The vtable pointer at +0x000
    //   is the compiler's own (these methods are declared `virtual` per the DWARF), so no explicit
    //   vtable member is declared -- the named members below begin at the X360 +0x004 base word.
    //   (PC is a 64-bit build: the implicit vtable pointer is 8 bytes here, so the X360 byte offsets
    //   in the comments are documentation of the console layout, not the PC struct's literal offsets.)
    u8    mbBaseField04;                             // +0x004  base flag (stw 0 covers +0x04..+0x07)
    u8    maBaseHead005[0x08 - 0x05];                // +0x005..+0x007
    u8    mbBaseFlag08;                              // +0x008  base flag (Prepare: stb 1; Construct: 0)
    u8    mbBaseFlag09;                              // +0x009  base flag (Construct 0; Update writes 1)
    u8    mbBaseFlag0A;                              // +0x00A  base flag (Construct 0; Update reads)
    u8    mbBaseFlag0B;                              // +0x00B  base flag (Update clears)
    u8    mbBaseFlag0C;                              // +0x00C  base flag (Construct 0; Update sets 1)
    u8    maBaseHead00D[0x10 - 0x0D];                // +0x00D..+0x00F
    s32   miParamWord1;                              // +0x010  cached parameter word (sibling: mParamWord1)
    u8    maBaseHead014[0x20 - 0x14];                // +0x014..+0x01F (base rig, incl. 4B param slot)

    Matrix44Affine mTransform;                       // +0x020  rig transform (DWARF :156, 64 bytes)

    // mPositionFinder (DWARF :158). Mostly opaque; the three latch bytes Update gates on are
    // surfaced by name at their pinned offsets (Construct seeds them 0/0/1).
    u8    maPositionFinder060[0x90 - 0x60];          // +0x060  PositionFinder head (opaque)
    bool  mbPositionFinderSeeded;                    // +0x090  set once FindPosition has seeded it
    bool  mbPositionFinderValid;                     // +0x091  set by Update when a vantage was found
    bool  mbPositionFinderField92;                   // +0x092  third latch (Construct = 1)
    u8    maPositionFinder093[0xD0 - 0x93];          // +0x093  PositionFinder tail (opaque)
    u8    mCollisionPolicy[0x150 - 0xD0];            // +0x0D0  VisibilityCollisionPolicy (DWARF :162, &-of)
    u8    maMid150[0x270 - 0x150];                   // +0x150  mShake/mLooker interior (DWARF :159/:160)

    // --- rig fade-out flags Update reads at +0x270..+0x272 (inside the mShake/mLooker rig). The
    //     concrete sub-object owns them; surfaced here by name so Update's fade gate is offset-clean.
    bool  mbRigFadeFlag270;                          // +0x270  fade-out latch A
    bool  mbRigFadeFlag271;                          // +0x271  fade-out latch B (forces the camera flag)
    bool  mbRigFadeFlag272;                          // +0x272  fade-out latch C (gates latch A)
    u8    maMid273[0x310 - 0x273];                   // +0x273  rest of the mShake/mLooker interior

    u8    mRandom[0x340 - 0x310];                    // +0x310  CgsNumeric::Random (DWARF :164, LCG state)

    // --- mTarget (Behaviour::VehicleRef, DWARF :166), +0x340..+0x34F ---
    s32   miTargetSet;                               // +0x340  target-set flag (Construct 0)
    s32   meTargetRaceCarIndex;                      // +0x344  target race-car index (Construct -1)
    s32   miTargetField348;                          // +0x348  cleared to 0
    u8    mbTargetField34C;                          // +0x34C  flag (Construct toggles 0 then 1)
    u8    maReserved34D[0x350 - 0x34D];              // +0x34D..+0x34F (VehicleRef tail)

    void* mpParametersSlot;                          // +0x350  adopted Parameters* (DWARF :168)
    f32   mfSquaredDistanceToTarget;                 // +0x354  cached squared distance (DWARF :170)
    f32   mfPerceivedDistanceModificationFactor;     // +0x358  perceived-distance factor (DWARF :172, =1.0)
    bool  mbSetup;                                   // +0x35C  setup-complete flag (DWARF :174)
    bool  mbGotPosition;                             // +0x35D  position-acquired flag (DWARF :175)
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourBystanderCam::GetName @0x821F9C78
//   lis r11, aBehaviourbysta@ha ; addi r3, ... ; blr   -> the literal class name
// ----------------------------------------------------------------------------
inline const char*
BehaviourBystanderCam::GetName() const
{
    return "BehaviourBystanderCam";
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BEHAVIOUR_BYSTANDER_CAM_H
