#pragma once

// BrnPhysics::Vehicle::E_DRIVER_TYPE -- which controller drives a vehicle.
// Recovered from references/DecFIGS/dwarfdump/.../SharedIO/BrnVehicleDriverControls.h
// (enum at line :40). The BrnNetworkDriverControls / BrnAIDriverControls /
// BrnTrafficDriverControls classes that also live in this header are separate future TUs.
//
// ADDITIVE GROW (BrnPhysics-vehicle group): this header originally homed ONLY the enum (the
// member RaceCarState::meDriverType needs). It now also homes the BrnPlayerDriverControls
// base class -- the player-input controls payload -- because the BrnPlayerDriverControls
// ledger TU (GetAftertouchValues @0x825B2E88) needs an owning home. Layout/order/types are
// verbatim from the DWARF (BrnVehicleDriverControls.h:60..113). Nothing existing is
// reordered or retyped; the enum above is untouched.
#include "types.hpp"   // s32, f32, u8, bool
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::Event (empty base)

namespace BrnPhysics
{
namespace Vehicle
{
    enum E_DRIVER_TYPE : s32
    {
        E_DRIVER_TYPE_PLAYER       = 0,
        E_DRIVER_TYPE_AI           = 1,
        E_DRIVER_TYPE_NETWORK      = 2,
        E_DRIVER_TYPE_TRAFFIC      = 3,
        E_NUM_E_DRIVER_TYPE_EVENTS = 4
    };

    // The player-input control payload published into the vehicle sim (the base of the
    // Network / AI / Traffic driver-control variants). Derives from the empty CgsModule::Event
    // queue-payload base, so the first data member sits at offset 0. Layout/order/types are
    // verbatim from the DWARF (BrnVehicleDriverControls.h:84..113); these member offsets are
    // exactly the ones the X360 GetAftertouchValues leaf reads (mfSteering @+0x10,
    // mfForwardSteering @+0x14, mfRequestedGas @+0x1C, mfBrake @+0x08).
    struct BrnPlayerDriverControls : public CgsModule::Event
    {
        // Quarter-scale modifier applied to the stick/steering inputs when deriving aftertouch
        // (named in the DWARF as KF_STICK_AFTERTOUCH_MODIFIER, BrnVehicleDriverControls.h:34).
        // The X360 GetAftertouchValues leaf multiplies by +/-0.25 (this magnitude).
        static const f32 KF_STICK_AFTERTOUCH_MODIFIER;

        s32  miVehicleID;                 // @0x00
        f32  mfGas;                       // @0x04
        f32  mfBrake;                     // @0x08
        f32  mfHandBrake;                 // @0x0C
        f32  mfSteering;                  // @0x10
        f32  mfForwardSteering;           // @0x14
        f32  mfSpin;                      // @0x18
        f32  mfRequestedGas;              // @0x1C
        f32  mfAftertouchLevel;           // @0x20
        f32  mfXSensor;                   // @0x24
        f32  mfYSensor;                   // @0x28
        f32  mfZSensor;                   // @0x2C
        f32  mfGSensor;                   // @0x30
        s8   miVehicleIDToMerge;          // @0x34
        bool mbReset;                     // @0x35
        bool mbToggle;                    // @0x36
        bool mbBoost;                     // @0x37
        bool mbIsInvulnerableToVehicles;  // @0x38
        bool mbIsInvulnerableToWorld;     // @0x39
        bool mbForceDrift;                // @0x3A
        bool mbBoostBounce;               // @0x3B
        bool mbIsOnStartLine;             // @0x3C
        bool mbIsSteeringWheel;           // @0x3D
        bool mbHorn;                      // @0x3E

    protected:
        E_DRIVER_TYPE meDriverType;       // @0x40

    public:
        // GetAftertouchValues @0x825B2E88 -- bodied in BrnPlayerDriverControls.cpp.
        // The DWARF signature is GetAftertouchValues(f32&, f32&, f32&, bool) const; the trailing
        // bool selects how the second output is derived. (The standalone X360 leaf reads that
        // selector from a spilled arg slot rendered as +0x41 by the decompiler.)
        void GetAftertouchValues(f32& lrfOutX, f32& lrfOutY, f32& lrfOutZ, bool lbUseRequestedGas) const;

        // The remaining members are owned by future TUs (no X360 address in this ledger):
        // ctor @ BrnVehicleDriverControls.h:63, Clear :67, ResetType :70, GetType :74.
        E_DRIVER_TYPE GetType() const { return meDriverType; }

        // ----- ADDITIVE GROW (physics-implementation campaign): the typed control accessors the
        //       VehiclePhysics / RaceCarPhysics bodies call. DECLARE-ONLY (bodied by the
        //       BrnPlayerDriverControls TU) so this header stays the SINGLE definition of the class
        //       (RaceCarPhysics.h previously carried a conflicting duplicate -> ODR; removed). The
        //       trailing-byte accessors (GetMode @+0x44, GetFlag78 @+0x4E, GetCarType @+0x41,
        //       GetButton63 @+0x3F) read members past this minimal DWARF slice and are left
        //       declare-only so no offset is fabricated here. Comments give the console byte each
        //       reads. -----
        f32  GetSteer() const;             // *(controls + 0x10)  steering axis [-1,1] (== mfSteering)
        f32  GetAftertouchEnable() const;  // *(controls + 0x20)  aftertouch-enable scalar (>0 active)
        f32  GetSixaxisTilt() const;       // *(controls + 0x18)  SIXAXIS tilt axis (aftertouch pitch)
        s8   GetCarType() const;           // *(controls + 0x41)  car-type byte
        bool GetButton63() const;          // *(controls + 0x3F)  per-frame button/bounce request
        s32  GetMode() const;              // *(controls + 0x44)  control mode (==1 -> slam-steer add)
        bool GetFlag78() const;            // *(controls + 0x4E)  slam-steer enable flag
        // overload the X360 standalone leaf takes; reads the aftertouch stick deflection.
        bool GetAftertouchValues(f32* lpYaw, f32* lpPitch, f32* lpScalar) const;
    };
}
}
