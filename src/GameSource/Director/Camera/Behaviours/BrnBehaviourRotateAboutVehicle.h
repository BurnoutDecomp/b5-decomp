#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ROTATE_ABOUT_VEHICLE_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ROTATE_ABOUT_VEHICLE_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                    // Vector4 (mOrbitDirection)
#include "GameShared/GameClasses/Core/CgsAssert.h"             // CGS_ASSERT (SetParameters' tripwire)
#include "GameSource/Director/Camera/BrnCollisionPolicy.h"     // CollisionPolicyAttachedToVehicle (embedded @+0x50)
#include "GameSource/Director/Utils/BrnVehicleRef.h"           // BrnDirector::VehicleRef (embedded @+0x2A0)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.h
//
// BrnDirector::Camera::BehaviourRotateAboutVehicle -- the "rotate about vehicle" camera
// behaviour: it sweeps the camera in an arc around the tracked car (used for orbit/reveal
// shots). HOME for the one BehaviourRotateAboutVehicle class slice this TU bodies: a member
// accessor that returns the address of an embedded sub-object at +0x50. The full behaviour
// (Construct/Prepare/Update and the rest of the rig) and its Behaviour base land with their
// own TUs; this header models only the slice this accessor needs, BY NAME.
//
// ----------------------------------------------------------------------------
// The ONLY function homed here is the accessor @0x821FB410:
//     addi r3, r3, 0x50 ; blr
// i.e. it returns `this + 0x50` -- the address of a member sub-object embedded at +0x50. The
// shipped symbol carries no method name and the function has no recorded xrefs, so the member's
// precise type/role is unrecoverable; it is modelled here as a named embedded member at the
// asm-attested offset and returned BY NAME via its address.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
// The per-frame vehicle data the re-seat resolve reads (real home:
// GameSource/Director/Utils/BrnDirectorAllVehicleData.h). Reference-only here.
class AllVehicleData;

namespace Camera
{

struct Camera;   // the per-frame camera the behaviour produces (full type: Camera.h)

class BehaviourRotateAboutVehicle
{
public:

    // ------------------------------------------------------------------------
    // The "rotate about vehicle" parameter block. HOME here because the ledger nests it under this
    // behaviour (BrnDirector::Camera::BehaviourRotateAboutVehicle::Parameters) and the camera-
    // tunings bank saves it through TextFileWriteSerialiser::Serialise<Parameters> @0x821FB410's
    // sibling @0x82214D48.
    //
    // FLAG: the text-serialise field-walk for this block is ATTESTED EMPTY. The X360 instantiation
    //   @0x82214D48 emits only the section-header label line + recursion-depth accounting; it
    //   discards the parameter-block register (mr r5,r4 overwrites the params ptr before FormatName)
    //   and makes NO `bl` to any inner Parameters::Serialise field walker -- the compiler inlined
    //   the inner visitor to nothing because it serialises zero fields to text. The visitor below is
    //   therefore an empty (zero-field) walk, faithful to the attested asm; NO field offsets are
    //   fabricated. The full parameter layout lands with the behaviour rig TU.
    // ------------------------------------------------------------------------
    class Parameters
    {
    public:
        // X360 visitor: `void Serialise<S>(S&)` for the camera-tunings serialiser S. Attested
        // EMPTY for the text writer (see the class FLAG): walks zero fields. Templated inline so
        // TextFileWriteSerialiser::Serialise<Parameters>'s odr-use inlines it away, matching the
        // degenerate instantiation asm (no inner field-walk call).
        template<class TSerialiser> void Serialise(TSerialiser& /*lrSerialiser*/) {}

        // The Behaviour::Parameters head. SetParameters @0x821F55B8 asserts this == 18
        // (eBehaviourRotateAboutVehicle), and Parameters::Construct @0x821FB330 stores 18.
        u32 GetType() const { return mType; }

        // ⚠️ The record is AT LEAST 0x80 bytes and embeds a Utils::Looker::Parameters (0x64
        // bytes) at +0x08 -- Parameters::Construct @0x821FB300 runs
        // Utils::Looker::Parameters::Construct(this + 8) @0x821F8D80 and then re-tunes twelve
        // of its fields. The full field-by-field seed is recovered (offsets and literal values
        // both) but NOT transcribed here: none of the field NAMES is recovered, and committing
        // twenty unnamed floats at console displacements is exactly the offset-poking the x64
        // rule forbids. Only the head that SetParameters actually reads is modelled.
        // DELETE-WHEN: Utils::Looker::Parameters is homed and the DWARF field names land.
        u32 mType;                                 // +0x00
        const char* mpcDebugName;                  // +0x04
        u8 maUnrecoveredFields[0x80 - 0x08 - sizeof(const char*) + 4];  // +0x08 .. +0x7F
    };

    // ⭐⭐ IDENTIFIED 2026-08-01 -- the "unrecoverable +0x50 sub-object" IS THE EMBEDDED
    //   COLLISION POLICY, and @0x821FB410 is this class's GetCollisionPolicy().
    //   BehaviourRotateAboutVehicle::Construct @0x8222BEDC/@0x8222BEFC does
    //     addi r3, r31, 0x50 ; li r4, 0 ; bl CollisionPolicyAttachedToVehicle::Construct
    //   so the member at +0x50 is a CollisionPolicyAttachedToVehicle (0x250 bytes), and the
    //   accessor is the exact analogue of BehaviourIceAnim::GetCollisionPolicy @0x821FACE8.
    //   Corroborating: the IDB's own symbol for @0x821FB410 is the truncated
    //   "…BehaviourRotateAboutVehicle::" (50 chars) and @0x821FACE8's is
    //   "…BehaviourIceAnim::GetCollisio" -- the same 50-char truncation.
    //   The BODY is verified; the NAME GetCollisionPolicy is inferred (high confidence).
    CollisionPolicyAttachedToVehicle* GetCollisionPolicy() { return &mCollisionPolicy; }

    // ⭐ SetParameters @0x821F55B8 -- BODIED 2026-08-01 (below), from the full 24-line asm.
    //
    // ⚠️ THE PARAMETER WAS TYPED `const void*` AND IS NOT: the console asserts
    //   `lpParameters->GetType() == eBehaviourRotateAboutVehicle` against tag 18 (0x12) before
    //   the store, so it is a real BehaviourRotateAboutVehicle::Parameters*.
    // ⚠️ AND IT IS *ONE* STORE, NOT TWO. The sibling BehaviourGameplayExternal::SetParameters
    //   @0x821F91A8 does two (mpParameters AND the debug-name word into base +0x10); this one
    //   writes only mpParameters @+0x374. That asymmetry is real, not a transcription gap --
    //   all three call sites (ArbStateTestbed::Update @0x8226B638,
    //   ArbStateCarSelect::Prepare @0x8226EFA0, ArbStateOnlineCarSelect::Prepare @0x82271020)
    //   show the same single store.
    void SetParameters(const Parameters* lpParameters);

    // The camera this behaviour produced this frame. The arbitrator states copy it into their
    // own mCamera while this behaviour is driving them (the X360 reaches it via the manager pool
    // slot the BehaviourHandle resolves to -- sub_821FDF38 reads slot+0x10, the same role the
    // ICE-anim behaviour exposes as GetProducedCamera). DECLARATION-ONLY: the body (and the
    // produced-camera member it returns) land with this behaviour's own TU; modelled here BY
    // NAME so consumers never reach the camera by offset.
    const Camera& GetProducedCamera() const;

    // ADDITIVE GROW (BrnArbStateCarSelect::Update @0x8226F5D0, several arms): re-seat this
    // orbit behaviour so it starts from the camera another behaviour is currently producing,
    // resolved against the frame's vehicle data (the X360 call is
    // `BehaviourRotateAboutVehicle::BecomeSimilarTo(behaviour, &sourceCamera,
    // info.mpAllVehicleData)` -- the junkyard states keep the look-around-car cam aligned with
    // the ICE movie so the later interpolation onto the car has no discontinuity).
    // DECLARATION-ONLY: the body lands with this behaviour's own TU.
    void BecomeSimilarTo(const Camera& lrSourceCamera, const AllVehicleData& lrAllVehicleData);

private:

    // ⛔ GROWN 2026-08-01. This class used to end at +0x51 -- it modelled the vtable head, a
    // reserved span and a one-byte "opaque sub-object", total sizeof 0x51. The real object is
    // at least 0x378: the collision policy alone is 0x250 bytes and mpParameters is at +0x374.
    // ⚠️ WHY THAT MATTERED: BehaviourManager::AllocateBehaviour<TBehaviour> picks its pool
    // bucket from sizeof(TBehaviour), and BrnBehaviourManager.cpp explicitly instantiates it
    // for this type. At sizeof 0x51 it booked a bucket for 81 bytes and would have
    // placement-new'd an 888-byte object into it -- the same class of defect as the retired
    // BehaviourInterpolate slice, and this one WOULD have overrun (the interpolate slice
    // under-used its bucket; this one is the other direction). Both sizes land in the same
    // 1600-byte small bucket, so the bucket CHOICE is unchanged and no allocation moves.
    //
    // Offsets, all asm-attested:
    //   +0x000  vtable / Behaviour base head
    //   +0x020  a 0x30-byte sub-object Construct @0x8222BF68 and BecomeSimilarTo @0x8224A4E4
    //           reset with the SAME ten stores (Vector4 + 4 f32 + 3 u8 + 2 f32). Utils::Looker
    //           is the strong candidate -- Parameters embeds a Utils::Looker::Parameters --
    //           but that is INFERRED, so it stays a span.
    //   +0x050  CollisionPolicyAttachedToVehicle mCollisionPolicy (0x250; Construct @0x8222BEDC)
    //   +0x2A0  BrnDirector::VehicleRef mVehicleRef (Construct seeds it to the player car
    //           @0x8222BF5C..64; BecomeSimilarTo resolves through it @0x8224A370)
    //   +0x2B0..+0x35F  rig members not modelled here
    //   +0x360  Vector4 mOrbitDirection (BecomeSimilarTo's only output; Construct seeds it
    //           from XMMatrixRotationY(-pi/2 * 0.25f)'s "at" row @0x8222C04C)
    //   +0x374  const Parameters* mpParameters (SetParameters' only store)
    void* mpVTable;                                        // +0x000 behaviour vtable (opaque base head)
    u8    maReserved008[0x20 - sizeof(void*)];             // +0x008 .. +0x01F
    u8    maReserved020[0x50 - 0x20];                      // +0x020 .. +0x04F  the reset sub-object
    CollisionPolicyAttachedToVehicle mCollisionPolicy;     // +0x050 (0x250 on the console, so
                                                           //  mVehicleRef follows immediately)
    BrnDirector::VehicleRef mVehicleRef;                   // +0x2A0
    u8    maReserved2B0[0x360 - 0x2B0];                    // +0x2B0 .. +0x35F
    Vector4 mOrbitDirection;                               // +0x360
    u8    maReserved370[0x374 - 0x370];                    // +0x370 .. +0x373
    const Parameters* mpParameters;                        // +0x374
};

// ----------------------------------------------------------------------------
// BehaviourRotateAboutVehicle::SetParameters @0x821F55B8 -- the whole function.
//   0x821F55D4  lwz    r11, 0(r31)        ; lpParameters->GetType()
//   0x821F55D8  cmplwi cr6, r11, 0x12     ; == 18 == eBehaviourRotateAboutVehicle
//   0x821F55E8  assert "lpParameters->GetType() == eBehaviourRotateAboutVehicle"
//               file ..\..\..\GameSource\Director/Camera/Behaviours/
//                    BrnBehaviourRotateAboutVehicle.h, line 150 (the HEADER, unlike the
//               GameplayExternal sibling whose tripwire quotes a .cpp)
//   0x821F5600  stw    r31, 0x374(r30)    ; mpParameters = lpParameters
// The tag 18 matches Parameters::Construct @0x821FB330's own `stw 0x12, 0(r8)`.
// ----------------------------------------------------------------------------
inline void BehaviourRotateAboutVehicle::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == 18u,
               "lpParameters->GetType() == eBehaviourRotateAboutVehicle");   // .h:150

    mpParameters = lpParameters;
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ROTATE_ABOUT_VEHICLE_H
