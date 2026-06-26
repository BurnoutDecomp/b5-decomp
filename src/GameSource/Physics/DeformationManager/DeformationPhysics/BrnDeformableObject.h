#pragma once

// BrnPhysics::Deformation::DeformableObject -- MINIMAL OWNING SLICE.
//
// The full DeformableObject is a ~26 KB class (24 sensor spheres, swept spheres, 128 tag points,
// 128 driven points, 50 IK parts, the embedded VehicleRigidBody, 20 deformation sensors, ...; see
// references/DecFIGS/dwarfdump/.../BrnDeformableObject.h). Reconstructing it whole is many separate
// TUs. THIS slice reconstructs only what DeformableObject::ApplyCarCarImpulse @0x82624C08 reaches
// BY NAME:
//   * the attached physics body (GetVehicleBody -> ExternalPhysicsBody&; the asm's repeated
//     `*(this+6476)`), and its RaceCarPhysics view (the inlined downcast the bounce path uses);
//   * the global entity id (GetGlobalEntityId);
//   * the other-car game-mode word + the two per-frame bounce flag bytes the cross-car bounce
//     logic reads/writes (the asm's *(...+26392) / +26413 / +26414);
//   * the three ledger callees the body invokes (ApplyCarCarImpulse itself, plus the declare-only
//     ApplySensorImpulse / GetVehicleWorldRestitution whose bodies are separate TUs).
//
// LAYOUT NOTE (project rule -- LAYOUT RECOVERY, members BY NAME, no raw offsets): the named members
// below carry their console byte offset in a comment for provenance, but the ~26 KB of preceding
// state is NOT reproduced as padding (same convention as the RaceCarPhysics / VehiclePhysics minimal
// slices). The class is reached only through these accessors here, so the absolute host size is not
// load-bearing for the per-TU `cl /c` gate. GROW the slice into the full member sequence as the
// DeformableObject TUs land -- do not fork.

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, VecFloat, EntityId
#include "GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.h"                         // ExternalPhysicsBody
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"                 // RaceCarPhysics
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformationSensor.h"   // StoredImpulseContact, DeformationSensor
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnCollidableBody.h"      // ImpulseParams

namespace CgsNumeric { class Random; }

namespace BrnPhysics
{
namespace Vehicle { class VehiclePhysics; }

namespace Deformation
{
    class DeformableObject
    {
    public:
        // ---- the car-car-impulse ledger func owned by THIS TU (bodied in BrnDeformableObject.cpp) ----

        // @0x82624C08. The two-body car-on-car shunt impulse orchestrator. Computes the closing
        // velocity at the contact, early-outs if the cars are separating, solves the two-body
        // collision impulse (ExternalPhysicsBody::CalculateCollisionImpulseWithBody), applies the
        // showtime/bounce-boost shaping, then applies the equal-and-opposite impulse to both cars
        // via ApplySensorImpulse (this car forward, the other car with the reversed contact).
        // Returns true if an impulse was applied, false if the contact was separating.
        bool ApplyCarCarImpulse(const StoredImpulseContact& lContact, VecFloat lvfTimeStep,
                                VecFloat lvfIteration, s32 liSensorIndex, const CgsNumeric::Random& lRandom);

        // ---- declare-only callees (their bodies are separate TUs; the per-TU gate needs only decls) ----

        // @0x826078B0. Apply one impulse to a sensor of THIS car (the heavy per-sensor apply that
        // forwards the built ImpulseParams to the body's ApplyLocalImpulse and updates the spy).
        void ApplySensorImpulse(VecFloat lvfTimeStep, const StoredImpulseContact& lContact,
                                const ImpulseParams& lImpulseParams, Vector3 lRelativeMotion,
                                Vector3 lImpulseDir, VecFloat lvfImpulseMagnitude,
                                DeformationSensor* lpSensor, bool lbAddToSpy, bool lbUseNormalScaledFriction);

        // @ DWARF BrnDeformableObject.h:459. The combined restitution for a vehicle-vs-vehicle
        // contact (consults the two cars' absorption sets / materials). Declare-only.
        VecFloat GetVehicleWorldRestitution(const StoredImpulseContact& lContact) const;

        // ---- accessors the body reaches BY NAME ----

        // The attached externally-simulated physics body (the asm's `*(this+6476)`), used as the
        // "A" body of the two-body impulse solve. The full class embeds a VehicleRigidBody whose
        // GetVehiclePhysics() yields this body; the slice pins the resolved pointer BY NAME.
        ExternalPhysicsBody&       GetVehicleBody()       { return *mpVehicleBody; }
        const ExternalPhysicsBody& GetVehicleBody() const { return *mpVehicleBody; }

        // The attached vehicle physics, viewed as a RaceCarPhysics (the inlined downcast the bounce
        // path uses). Null when the attached vehicle is not a race car. DWARF BrnDeformableObject.h:361
        // exposes GetVehiclePhysics(); the race-car view is the body's bounce-boost entry point.
        Vehicle::RaceCarPhysics*       AsRaceCarPhysics()       { return mpAttachedRaceCar; }
        const Vehicle::RaceCarPhysics* AsRaceCarPhysics() const { return mpAttachedRaceCar; }

        // DWARF BrnDeformableObject.h:407. The deformation sensor at the given index. Declare-only.
        DeformationSensor* GetDeformationSensor(s32 liSensorIndex);

        // DWARF BrnDeformableObject.h:347. This car's global scene-entity id (passed to the bounce
        // call). Declare-only (its body reads mGlobalEntityId).
        EntityId GetGlobalEntityId();

        // The other car's packed game-mode/state word (the asm's `*(...+26392)`); the bounce logic
        // tests its HIGH byte == 2 (the "in a takedown/showtime game mode" selector). Pinned BY
        // NAME at console +26392. FLAG: the exact high-byte enum is not separately homed -- it is
        // the EGameModeType selector the cross-car bounce gate compares against 2.
        u8 GetGameModeByte() const { return static_cast<u8>(mu32GameModeState >> 24); }

    private:
        // The attached physics body pointer (console +6476). The full class embeds VehicleRigidBody
        // by value; here the resolved attached ExternalPhysicsBody is pinned BY NAME (the asm loads
        // it as a pointer at +6476 and uses it as both the impulse-solver "A" body and -- downcast
        // -- the bounce-boost RaceCarPhysics).
        ExternalPhysicsBody* mpVehicleBody;        // +6476 (pinned BY NAME)

        // The same attached vehicle re-typed as a RaceCarPhysics for the bounce path (the inlined
        // downcast). Pinned BY NAME alongside mpVehicleBody; null for non-race-car bodies.
        Vehicle::RaceCarPhysics* mpAttachedRaceCar;

        // The car game-mode/state word whose HIGH byte selects the game mode (console +26392). Read
        // on both this car and the other car (lContact.mpOtherVehicle) by the bounce gate.
        u32 mu32GameModeState;          // +26392 (pinned BY NAME)

        // Per-frame cross-car bounce flag bytes (console +26413 / +26414). mbBounceRandomParity is
        // set from a random parity in the bounce-boost branch; mbHasBouncedThisFrame latches the
        // double-bounce damp path. Pinned BY NAME. FLAG: names inferred from role (the asm only
        // shows a parity store at +26413 and a set-to-1 / read at +26414).
        u8 mbBounceRandomParity;        // +26413 (pinned BY NAME; role-inferred)
        u8 mbHasBouncedThisFrame;       // +26414 (pinned BY NAME; role-inferred)

        // The global scene-entity id (console +645 region per DWARF mGlobalEntityId); read by
        // GetGlobalEntityId. Pinned BY NAME.
        EntityId mGlobalEntityId;

    public:
        // Mutators the body uses for the two cross-car flag bytes (named-member writes -- no raw
        // offset pokes). mbHasBouncedThisFrame is set on both this car and the other car.
        void SetHasBouncedThisFrame(bool lb) { mbHasBouncedThisFrame = lb ? 1u : 0u; }
        bool HasBouncedThisFrame() const     { return mbHasBouncedThisFrame != 0u; }
        void SetBounceRandomParity(bool lb)  { mbBounceRandomParity = lb ? 1u : 0u; }
    };
}
}
