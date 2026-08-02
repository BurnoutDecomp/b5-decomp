#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GAMEPLAY_BUMPER_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GAMEPLAY_BUMPER_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                            // Vector3
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"           // THE Behaviour base
#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"           // Utils::CameraShake(+Params)
                                                                       //   + CameraShakeICEController

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumper.h
//
// BrnDirector::Camera::BehaviourGameplayBumper -- the in-car "bumper cam" gameplay behaviour:
// the low, fast camera mounted at the front of the player car, with acceleration dampening,
// pitch/yaw/roll springs, body roll/pitch scaling, its own FOV / boost-FOV and an impact
// shake. It is one of the TWO SHARED gameplay cameras SharedCameraContainer::Prepare
// @0x82263D50 allocates, and it is the one selected by default
// (GetGameplayCameraHelperIndex picks the bumper unless mbUseGameplayExternal is set).
//
// ⭐ RE-BASED (2026-07-29). This class used to be a raw-offset SLICE: a `void* mpVTable` head,
// two reserved byte spans, an invented `mpcCachedName` member, and a local
// `EBehaviourTypeGameplayBumper` enum. It now derives the canonical
// BrnDirector::Camera::Behaviour and carries the DWARF member list by name.
//
// WHY IT HAD TO BE RE-BASED: BehaviourManager::AllocateBehaviour<BehaviourGameplayBumper>
// @0x82253250 hands a raw pool slot to AbstractPool::AllocateVoid<T> @0x8224BAB8, which
// placement-news the behaviour in it; BehaviourHelper::Prepare @0x82255F48 then dispatches
// vtable slot 0 (Construct). For a NON-POLYMORPHIC class placement-new installs no vtable, so
// that dispatch read a null vptr -- an access violation in BehaviourHelper::Prepare
// (`call [rax+8]` with rax = 0) the moment Arbitrator::Update entered E_STATE_PREPARE. This
// was the FIRST thing on the path to a moving director camera.
//
// LAYOUT AUTHORITY: the DECFIGS DWARF (BrnBehaviourGameplayBumper.h:53, members :88..:100;
// Parameters :106, members :109..:129) -- and EVERY member offset below is independently
// re-derived from the X360 asm of Construct @0x82242418 and Prepare @0x821F9640:
//
//   Construct: *(this+8..12)=0, *(this+4)=0, *(this+16)=0        <- the inlined base Construct
//              *(this+2088)=0                                    <- mpParameters      @0x828
//              CameraShakeICEController::Construct(this+48)      <- mBoostShake       @0x030
//              *(this+32/36/40/44)=0.0f                          <- mImpactShake      @0x020
//              *(this+20)=0.0f                                   <- mfImpactShakeFactor @0x014
//   Prepare:   stvx128 v0(=0), this+2064                         <- mLastCameraAngles @0x810
//              *(this+2080)=0.0f, *(this+2084)=0.0f              <- mfLastSpeed       @0x820
//                                                                   mfDampenedAcceleration @0x824
//              *(this+8)=1                                       <- mbIsPrepared
//              return 1
//   SetParameters @0x821F39C0: stw params,0x828 / stw params->+4,0x10
//
// ⭐ Those two anchors (mBoostShake at +0x30, mLastCameraAngles at +0x810) PIN
// Utils::CameraShakeICEController's console size at 0x7E0 -- independently re-confirmed by
// BehaviourGameplayExternal (its own Prepare Constructs one at +0x2C0 and its
// mLastPlayerTransform sits at +0xAA0). See BrnCameraShake.h.
//
// x64: parity is BY NAMED MEMBER (pointers widen); the console offsets above are provenance.
// ============================================================================

namespace BrnDirector
{
namespace Camera
{

// RETIRED (2026-07-29): the local `enum EBehaviourTypeGameplayBumper { eBehaviourGameplayBumper
// = 1 }` that used to sit here was a minimal slice of the shared camera-behaviour type tag,
// declared "replace with the real EBehaviourType enum when the Behaviour base TU lands". The
// base landed; the tag now lives on Behaviour::Parameters::mType (the word SetParameters
// @0x821F39C0 compares against 1). The enumerator is kept, with its X360-pinned VALUE, as the
// bumper's own tag constant so no call site changes meaning -- there is still no single homed
// EBehaviourType enum (each behaviour's tag is only observable in its own asserts:
// external 0, bumper 1, rig 2, helicam 6, passenger 7).
enum EBehaviourTypeGameplayBumper
{
    eBehaviourGameplayBumper = 1
};

class BehaviourGameplayBumper : public Behaviour
{
public:

    // ------------------------------------------------------------------------
    // The bumper-cam parameter block (DWARF BrnBehaviourGameplayBumper.h:106, deriving
    // Behaviour::Parameters).
    //
    // ⭐ EVERY FIELD NAME BELOW IS DOUBLY ATTESTED: the DWARF member order, and this block's
    // own serialiser labels (the write instance @0x82230B68 / read @0x82214C70 walk the
    // fields in ascending offset order with the labels quoted per field). The retired slice
    // called these mfField08..mfField2C.
    // ------------------------------------------------------------------------
    class Parameters : public Behaviour::Parameters
    {
    public:
        // X360 visitor: `void Serialise<S>(S&)` -- walks this block into the camera-tunings
        // serialiser S (TextFile{Read,Write}Serialiser / DebugMenuSerialiser). Bodied in
        // BrnBehaviourGameplayBumper.cpp.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        EBehaviourTypeGameplayBumper GetType() const
        {
            return static_cast<EBehaviourTypeGameplayBumper>(mType);
        }

        // The source block this seeds from: a small object whose +0x04 holds a pointer to
        // the f32 source array Parameters::Set copies out of (lwz r11,4(r4); lfs ...(r11)).
        struct Source
        {
            const f32* mpfValues;   // +0x04 of the source object: the f32 source array
        };

        // Seed this block from an attribute-system source object, applying the default
        // bumper-cam tunables. @0x821F94C8.
        void Set(const Source* lpSource);

        // DWARF h:132 -- declaration-only (its own ledger function; nothing on the live
        // director path calls it, Set is what the attribute pump uses).
        void Construct();

        // ---- layout (DWARF h:109..:129; console offsets after the 8-byte base) ----------
        f32 mfYOffset;                 // :109  +0x08  "Y Offset"          <- source[0x04]
        f32 mfZOffset;                 // :110  +0x0C  "Z Offset"          <- source[0x00]
        f32 mfAccelerationDampening;   // :112  +0x10  "Accel. Dampening"  <- source[0x28]
        f32 mfAccelerationResponse;    // :114  +0x14  "Accel. Response"   <- source[0x24]
        f32 mfPitchSpring;             // :116  +0x18  "Pitch Spring"      <- source[0x10]
        f32 mfYawSpring;               // :117  +0x1C  "Yaw Spring"        <- source[0x08]
        f32 mfRollSpring;              // :118  +0x20  "Roll Spring"       <- source[0x0C]
        f32 mfFOV;                     // :120  +0x24  (label @0x820051C0) <- source[0x14]  (>0)
        f32 mfBodyRollScale;           // :122  +0x28  "Body Roll Scale"   <- source[0x1C]
        f32 mfBodyPitchScale;          // :123  +0x2C  "Body Pitch Scale"  <- source[0x20]
        f32 mfBoostFOV;                // :125  +0x30  "FOV during boost"  <- source[0x18]  (>0)
        bool mbIsValid;                // :127  +0x34  (Set stores 1)

        // ⭐ :129 +0x38 -- the four f32 the retired slice called mfField38..mfField44 ARE one
        // CameraShake::Parameters (0.0f / 0.0f / 3.0f / 1.0f). Confirmed by the identical
        // quadruple at BehaviourGameplayExternal::Parameters +0x1C, which that block's own
        // serialiser labels "Impact Shake Params" and drives as a nested
        // CameraShake::Parameters section.
        Utils::CameraShake::Parameters mImpactShakeParams;   // :129  +0x38 .. +0x47
    };

    // ---- the virtual interface (DWARF h:53) --------------------------------------------

    // @0x82242418 -- zero the base, the impact-shake state and the parameter pointer, then
    // Construct the boost shake.
    virtual void Construct();

    // @0x821F9640 -- clear the last-frame tracking and report ready (cannot fail).
    virtual bool Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo);

    // @0x821F9670.
    virtual const char* GetName() const;

    // Adopt a bumper-cam parameter block. @0x821F39C0. NOT a virtual override: the DWARF
    // declares it (h:144) taking the DERIVED Parameters type, so it does not share the base's
    // slot-7 signature -- it HIDES the base name rather than overriding it, which is exactly
    // what the console does (the bumper has a standalone symbol; the external's SetParameters
    // @0x821F91A8 takes Behaviour::Parameters and IS the override).
    void SetParameters(const Parameters* lpParameters);

    // FLAG (not transcribed): the DWARF also declares `virtual bool Update(Camera&, const
    //   BehaviourSharedInfo&)` (BrnBehaviourGameplayBumper.cpp:85, X360 @0x82226778, ~230
    //   lines: the acceleration-dampened spring rig that rides the player car) and
    //   `virtual void SetupTweaker(Tweaker&)` (.cpp:314). Neither is declared here, so both
    //   vtable slots keep the base's defaults (Update returns true and leaves the camera
    //   untouched; SetupTweaker does nothing). That is a DOCUMENTED GAP, not a fabrication --
    //   the alternative would be inventing a camera rig.
    //
    // ⚠️ THE CLOSING CLAUSE OF THIS FLAG WAS WRONG (retired 2026-08-02). It read: "Nothing
    //   dispatches slot 2 today anyway: MainDirector::UpdateCameraBehavioursPostScene
    //   @0x8224FD30 (the only caller of UpdateAllBehaviours) is itself still gated." Both
    //   clauses are false since the 2026-08-01 PreScene/PostScene split -- `BrnMainDirector.cpp
    //   :1153` calls UpdateAllBehaviours from UpdateCameraBehavioursPreScene @0x82255318,
    //   un-gated. SLOT 2 IS DISPATCHED. This behaviour is inert for the same reason its
    //   external sibling is: its parameter block is never validated. The full seven-link chain
    //   (shared by both gameplay cameras, since one SharedCameraContainer::Prepare binds them
    //   together and one MainDirector::ProcessNewVehicleEvents seeds them together) is written
    //   out in BrnBehaviourGameplayExternal.h's matching FLAG -- read it there.
    //   DELETE-WHEN: @0x82226778 is transcribed AND the parameter chain reaches
    //   Parameters::Set.

private:
    // ---- layout (DWARF h:88..:100; every offset asm-pinned -- see the file banner) -------
    f32                            mfImpactShakeFactor;  // :88   +0x014
    f32                            mfTimeInJump;         // :89   +0x018
    bool                           mbJumping;            // :90   +0x01C
    Utils::CameraShake             mImpactShake;         // :91   +0x020
    Utils::CameraShakeICEController mBoostShake;         // :94   +0x030 (0x7E0)
    Vector3                        mLastCameraAngles;    // :95   +0x810
    f32                            mfLastSpeed;          // :97   +0x820
    f32                            mfDampenedAcceleration; // :98 +0x824
    const Parameters*              mpParameters;         // :100  +0x828
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourGameplayBumper::SetParameters @0x821F39C0
//   lwz  r11, 0(r4)         ; lpParameters->mType
//   cmplwi r11, 1           ; == eBehaviourGameplayBumper
//   ... assert on mismatch ...
//   lwz  r11, 4(r4)         ; lpParameters->mpcDebugName  (Behaviour::Parameters +0x04)
//   stw  r4,  0x828(r3)     ; mpParameters = lpParameters
//   stw  r11, 0x10(r3)      ; the BASE's mpcDebugParametersName (Behaviour +0x10)
//
// ⭐ The +0x10 store used to be modelled as an invented member `mpcCachedName`. With the real
// base in place it resolves to Behaviour::mpcDebugParametersName, and the whole function is
// the base's own protected SetDebugParametersName(lpParameters->GetDebugName()) -- both
// offsets (+0x04 on the param block, +0x10 on the behaviour) fall out of Behaviour::Parameters
// and Behaviour exactly. No invented member remains.
// ----------------------------------------------------------------------------
inline void
BehaviourGameplayBumper::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourGameplayBumper,
               "lpParameters->GetType() == eBehaviourGameplayBumper");
    mpParameters = lpParameters;                                  // stw r4,  0x828(this)
    SetDebugParametersName(lpParameters->GetDebugName());         // stw r11, 0x10(this)
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GAMEPLAY_BUMPER_H
