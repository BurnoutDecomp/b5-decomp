#ifndef GAMESOURCE_DIRECTOR_CAMERA_BRN_BEHAVIOUR_PARAMETER_BANK_H
#define GAMESOURCE_DIRECTOR_CAMERA_BRN_BEHAVIOUR_PARAMETER_BANK_H

#include "types.hpp"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumper.h"    // BehaviourGameplayBumper::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h"  // BehaviourGameplayExternal::Parameters

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
        // car-select state hands to BehaviourRotateAboutVehicle::SetParameters. @+0x2334.
        // FLAG: opaque -- only its address is taken; type/role of the interior fields is not
        // modelled here (the real Parameters sub-block lands with the parameter-bank TU).
        struct LookAroundCarCamParameters
        {
            u8 maOpaque[1];   // opaque head; only the block's address is used
        };

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
        };
    }
}

#endif // GAMESOURCE_DIRECTOR_CAMERA_BRN_BEHAVIOUR_PARAMETER_BANK_H
