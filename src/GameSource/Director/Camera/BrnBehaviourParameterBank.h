#ifndef GAMESOURCE_DIRECTOR_CAMERA_BRN_BEHAVIOUR_PARAMETER_BANK_H
#define GAMESOURCE_DIRECTOR_CAMERA_BRN_BEHAVIOUR_PARAMETER_BANK_H

#include "types.hpp"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumper.h"    // BehaviourGameplayBumper::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h"  // BehaviourGameplayExternal::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h"           // BehaviourGyroCam::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.h"          // BehaviourFixedCam::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourBystanderCam.h"       // BehaviourBystanderCam::Parameters
#include "GameSource/Director/Camera/Behaviours/BehaviourPassengerCam.h"            // BehaviourPassengerCam::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.h" // BehaviourRotateAboutVehicle::Parameters

// ============================================================================
// GameSource/Director/Camera/BrnBehaviourParameterBank.h
//
// BrnDirector::NamedParameters -- the director's bank of per-named-behaviour camera
// "Parameters" blocks (aftertouch, gyro, bystander, rig, rotate-about-vehicle, ...). The
// arbitrator-state shared context (ArbStateSharedInfo::mpNamedParameters) holds it BY POINTER
// and an arbitrator state reaches one named block out of it to configure a behaviour it has
// just allocated.
//
// FLAG: MINIMAL SLICE. The full bank (every BehaviourXxx::Parameters sub-block, the
//   BehaviourParameterBank wrapper + its serialiser) is a heavy cascade and has no
//   reconstructed home of its own yet. This header models ONLY the one named accessor this
//   build's online-car-select arbitrator state needs -- the "look around car" (rotate-about-
//   vehicle) parameter block -- accessed BY NAME via its address. The DecFIGS DWARF / Feb-2007
//   source name the bank's earlier blocks but NOT this one (the rotate-about-vehicle params
//   were added after the Feb-2007 cut), so the block's precise type is unrecoverable; it is
//   modelled as a named opaque sub-object at the asm-attested offset and only its address is
//   taken (passed to BehaviourRotateAboutVehicle::SetParameters as an opaque parameter block).
//   Replace with the real layout when the BehaviourParameterBank TU lands; the accessor NAME
//   is stable.
//
//   X360 (ArbStateOnlineCarSelect::Prepare @0x82271020): the block sits at
//   mpNamedParameters + 0x2334 (asm `addi r31, r11, 0x2334`); modelled here as the named
//   member maLookAroundCarCamParameters at that offset and returned by address.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    struct NamedParameters
    {
        // The "look around car" / rotate-about-vehicle camera parameter block the online
        // car-select and (offline) car-select states hand to
        // BehaviourRotateAboutVehicle::SetParameters. @+0x2334.
        // ⭐ TYPED 2026-08-01: it is not opaque -- SetParameters @0x821F55B8 asserts
        // `lpParameters->GetType() == eBehaviourRotateAboutVehicle` (tag 18) on whatever the
        // caller hands it, and both call sites hand it exactly this block, so this block IS a
        // BehaviourRotateAboutVehicle::Parameters. (Its interior beyond the shared
        // Behaviour::Parameters head is still unmodelled -- see that class.)
        typedef Camera::BehaviourRotateAboutVehicle::Parameters LookAroundCarCamParameters;

        // Accessor returning the address of the look-around-car parameter block (mpNamedParameters
        // + 0x2334). Returns by const reference; the caller passes &block to SetParameters.
        const LookAroundCarCamParameters& GetLookAroundCarCamParameters() const
        {
            return maLookAroundCarCamParameters;
        }

        // The reserved span places the addressed block at the asm-attested +0x2334. The rest of
        // the bank (the earlier named Parameters blocks) is not modelled here.
        u8                         maReservedHead[0x2334];           // +0x0000 .. +0x2333
        LookAroundCarCamParameters maLookAroundCarCamParameters;     // +0x2334
    };

    namespace Camera
    {
        // BrnDirector::Camera::BehaviourParameterBank (PS3 DWARF: the type of the local
        // `lrBehaviourParameterBank` in SharedCameraContainer::Prepare, and of the
        // BehaviourManager's embedded :325 sub-object at X360 manager +0x12530). MINIMAL
        // SLICE: only the two named fetches SharedCameraContainer::Prepare @0x82263D50 needs.
        // The X360 inlines them to fixed bank offsets -- the external ("chase") block at
        // bank+0x2488 and the bumper block at bank+0x2538 == +0x2488 + 0xB0
        // (sizeof(BehaviourGameplayExternal::Parameters)), so the two blocks are adjacent.
        // Both accessors are DECLARATION-ONLY (their trivial fetch bodies need the bank
        // layout, which is un-homed -- same status as the manager's opaque :325 slot).
        // The PS3 DWARF names the bumper fetch GetGameplayBumperCameraParamsForCar with the
        // X360 ABI showing NO car argument (a fixed-offset fetch): the DWARF name is kept
        // with the X360 arity. FLAG: the external accessor's name is inferred by symmetry
        // (its PS3 hint line is truncated).
        //
        // Relationship to BrnDirector::NamedParameters (above) is NOT pinned: the arbitrator
        // shared context reaches named parameter blocks through mpNamedParameters (+0x2334
        // block) and the manager reaches these two through its own +0x12530 bank; whether
        // those are the same object is left to the bank's own TU.
        class BehaviourParameterBank
        {
        public:
            // X360 bank+0x2538: the bumper-cam ("in car") gameplay parameter block.
            const BehaviourGameplayBumper::Parameters& GetGameplayBumperCameraParamsForCar() const;

            // X360 bank+0x2488: the external ("chase") gameplay parameter block.
            const BehaviourGameplayExternal::Parameters& GetGameplayExternalCameraParamsForCar() const;

            // X360 bank+0xAB4 (manager+0x12FE4): the gyro-cam block the hit-traffic
            // moment binds (MomentHitTraffic::Update @0x82271EB8 hands it to
            // BehaviourGyroCam::SetParameters). FLAG: accessor name inferred by the
            // sibling naming pattern (the X360 inlines the fetch to the fixed offset).
            const BehaviourGyroCam::Parameters& GetGyroCamMomentParams() const;

            // X360 bank+0x2334 (manager+0x14864): the fixed-cam block the static-cam-
            // impact moment binds (MomentStaticCamImpact::Update @0x82266C00 hands it
            // to BehaviourFixedCam::SetParameters). FLAG: accessor name inferred; note
            // +0x2334 coincides with NamedParameters::maLookAroundCarCamParameters
            // (see the relationship note above -- left to the bank's own TU).
            const BehaviourFixedCam::Parameters& GetStaticCamImpactCamParams() const;

            // The two bystander-sees-action camera blocks (MomentBystanderSeesAction::
            // Update @0x82266730 picks by its Parameters::mbCloseCamera and feeds the
            // block to BehaviourBystanderCam::SetParameters). X360 bank +0xEEC (close)
            // / +0x1024 (manager +78876 / +79188). FLAG: getter names inferred from
            // that role. DECLARATION-ONLY (the bank's own TU bodies them).
            const BehaviourBystanderCam::Parameters& GetBystanderCamCloseMomentParams() const;   // +0xEEC
            const BehaviourBystanderCam::Parameters& GetBystanderCamMomentParams() const;        // +0x1024

            // The passenger-sees-action camera block (MomentPassengerSeesAction::
            // Update @0x8225EEB8 hands manager+83732 == bank+0x21E4 to
            // BehaviourPassengerCam::SetParameters). FLAG: getter name inferred.
            // DECLARATION-ONLY (the bank's own TU bodies it).
            const BehaviourPassengerCam::Parameters& GetPassengerCamMomentParams() const;        // +0x21E4

            // The player-jumping moment's shot parameter blocks (MomentPlayerJumping::
            // Prepare @0x82251048 AddShots). The X360 inlines fixed bank offsets on a
            // sizeof grid -- rig blocks at bank + 0x1280 + 0x120*liIndex (Prepare feeds
            // indices {0,1,6,9,8,4} to the attached-rig collection and {10,11,12} to the
            // dropped-rig collection), bystander blocks at bank + 0xD18 + 0x138*liIndex
            // (indices {0,1}). FLAG: accessor names + the index grids inferred (the
            // 0x120/0x138 strides match the blocks' spacings == the Parameters sizes;
            // the individual offsets are asm-attested). DECLARATION-ONLY (the bank's
            // own TU bodies them). BehaviourRig::Parameters comes in through the
            // BehaviourPassengerCam.h include's BehaviourRig.h base slice.
            const BehaviourRig::Parameters&          GetPlayerJumpingRigShotParams(s32 liIndex) const;
            const BehaviourBystanderCam::Parameters& GetPlayerJumpingBystanderShotParams(s32 liIndex) const;

            // X360 0x822732D0. Dumps the whole parameter bank to the debug text file
            // "d:\\camera.txt". The X360 compiler inlines TextFileWriteSerialiser::
            // Construct("d:\\camera.txt") (fopen "w", muRecursionDepth = 0) and Destruct()
            // (CGS_ASSERT muRecursionDepth == 0; fclose) into this body; reconstructed as the
            // three calls the source made. Lives in BrnBehaviourParameterBank.cpp.
            void SaveParameters();

            // The bank's serialiser-visitor template: walks every named Parameters sub-block,
            // handing each field to the supplied serialiser. Attested by the X360 mangled call
            // in SaveParameters (`public: void Serialise<TextFileWriteSerialiser>(
            // TextFileWriteSerialiser&)`). The per-instantiation bodies are separate (still-todo)
            // TUs; declared here so SaveParameters can call it. T is deduced from the argument.
            template<class T> void Serialise(T& lrSerialiser);
        };
    }
}

#endif // GAMESOURCE_DIRECTOR_CAMERA_BRN_BEHAVIOUR_PARAMETER_BANK_H
