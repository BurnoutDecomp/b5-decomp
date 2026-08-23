#pragma once

// BrnPhysics::Vehicle::VehicleDriver -- the per-car driver: one control payload plus the
// network catch-up interpolation state. Eight of these are the FIRST array in VehicleManager
// (maRaceCarDrivers @ +64), and a ninth (mPlayerAiDriver @ +171968) is the manager's spare.
//
// DWARF home: references/DecFIGS/dwarfdump/GameSource/Physics/VehicleManager/VehiclePhysics/
// BrnVehicleDriver.h (struct @ line 32; the member run is :116..:123).
//
// LAYOUT -- every offset below is asm-literal in VehicleDriver::Construct @0x825B83C8:
//   +0x00  BrnAIDriverControls mControls           (80 bytes; ends 0x50)
//   +0x50  Matrix44Affine      mCatchupTargetTransform   (four stvx128 lanes at +0x50..+0x80)
//   +0x90  Matrix44Affine      mSlerpTransform           (four stvx128 lanes at +0x90..+0xC0)
//   +0xD0  E_DRIVER_TYPE       meDriverType        (`stw r28, 0xD0(r3)` with r28 == 4)
//   +0xD4  s8                  mi8NumOfInterpSteps (`stb r11, 0xD4(r3)`)
//   +0xD5  bool                mbSnappedThisFrame  (`stb r11, 0xD5(r3)`)
//   sizeof == 224 (0xE0) -- independently confirmed by VehicleManager::Construct's per-car
//   cursor advance `addi r25, r25, 0xE0`, eight times, from this+64 to this+1856.
//
// This type is what retires VehicleManager's `RaceCarDriverRecord` stand-in. That stand-in
// carried three role-named bytes at in-record 59/60/61 behind an explicit HYPOTHESIS flag; all
// three land inside mControls and are now real members, and the DWARF names match the recovered
// roles exactly:
//   in-record 59 (0x3B) "mbBoostImpactEligible" -> mControls.mbBoost
//         (the takedown classifier promotes SLAM->BOOST_SLAM / SHUNT->BOOST_SHUNT when the
//          aggressor was HOLDING BOOST -- that is the boost button, not a derived flag)
//   in-record 60 (0x3C) "mbTakenDown"           -> mControls.mbIsInvulnerableToVehicles
//         (SetRaceCarCrashing consults it to SUPPRESS a vehicle-caused crash)
//   in-record 61 (0x3D) "mbSuppressByCause"     -> mControls.mbIsInvulnerableToWorld
//         (the same suppression, gated on the WORLD-collision cause sub-codes)
// Three role-derived guesses, three DWARF members, three exact semantic matches.

#include "types.hpp"
#include <cstddef>                                                  // offsetof (layout gate)
#include "BrnCommonTypes.h"                                         // Matrix44Affine
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h" // BrnAIDriverControls, E_DRIVER_TYPE

namespace BrnPhysics
{
namespace Vehicle
{
    class VehiclePhysics;   // pointer-only collaborator on the declare-only surface below

    struct VehicleDriver
    {
        // DWARF BrnVehicleDriver.h:114. The fixed slerp step count a network snap interpolates over.
        static const s8 ki8NumNetworkSlerpSteps = 10;

        // @0x825B83C8 -- the console constructor. Called from VehicleManager::Construct (the
        // eight-car array and the spare mPlayerAiDriver) and from PhysicalTrafficManager::Prepare.
        void Construct();

        // ---- declare-only DWARF surface (bodies belong to this class's own future slices) ------
        // Left DECLARED-ONLY on purpose: a declared non-virtual with no definition costs nothing
        // until something calls it, and none of these has a recovered body yet.
        void Destruct();
        bool Prepare();
        bool Release();
        void Update(const BrnPlayerDriverControls* lpControls);
        void Update(const BrnNetworkDriverControls* lpControls, VehiclePhysics* lpVehicle);
        void Update(const BrnAIDriverControls* lpControls);
        void Update(const BrnTrafficDriverControls* lpControls);
        void UpdateVehicle(VehiclePhysics* lpVehicle);
        void StartCatchupInterpolation(VehiclePhysics* lpVehicle, Matrix44Affine lTargetTransform,
                                       Vector3 lLinearVelocity, Vector3 lAngularVelocity, bool lbSnap);
        void ClearControls();

        const BrnPlayerDriverControls* GetControls() const { return &mControls; }
        E_DRIVER_TYPE  GetDriverType() const               { return meDriverType; }
        bool           IsInvulnerableToVehicles() const    { return mControls.mbIsInvulnerableToVehicles; }
        bool           IsInvulnerableToWorld() const       { return mControls.mbIsInvulnerableToWorld; }
        void           SetInvulnerableToVehicles(bool lbOn){ mControls.mbIsInvulnerableToVehicles = lbOn; }
        void           SetInvulnerableToWorld(bool lbOn)   { mControls.mbIsInvulnerableToWorld = lbOn; }
        void           ClearSnappedThisFrame()             { mbSnappedThisFrame = false; }
        Matrix44Affine GetCatchupTargetTransform() const   { return mCatchupTargetTransform; }
        s8             GetNumInterpSteps() const           { return mi8NumOfInterpSteps; }
        bool           SnappedThisFrame() const            { return mbSnappedThisFrame; }

        // Members are public: VehicleManager's takedown chain reads mControls.mbBoost and the two
        // invulnerability flags directly (the X360 reaches them by absolute offset off the manager),
        // and the console's own accessor surface exposes them anyway.
        BrnAIDriverControls mControls;                 // +0x00   (80)
        Matrix44Affine      mCatchupTargetTransform;   // +0x50   (64)
        Matrix44Affine      mSlerpTransform;           // +0x90   (64)
        E_DRIVER_TYPE       meDriverType;              // +0xD0
        s8                  mi8NumOfInterpSteps;       // +0xD4
        bool                mbSnappedThisFrame;        // +0xD5
    };

    // The layout gate. Every number is an asm literal from VehicleDriver::Construct @0x825B83C8,
    // and the 224 is independently attested by VehicleManager::Construct's `addi r25, r25, 0xE0`.
    static_assert(sizeof(VehicleDriver) == 224,
                  "VehicleDriver is 224 bytes (VehicleManager::Construct `addi r25, r25, 0xE0`)");
    static_assert(offsetof(VehicleDriver, mControls) == 0x00,   "mControls @+0");
    static_assert(offsetof(VehicleDriver, mCatchupTargetTransform) == 0x50,
                  "mCatchupTargetTransform (Construct: stvx128 lanes off r3+0x50)");
    static_assert(offsetof(VehicleDriver, mSlerpTransform) == 0x90,
                  "mSlerpTransform (Construct: stvx128 lanes off r3+0x90)");
    static_assert(offsetof(VehicleDriver, meDriverType) == 0xD0,
                  "meDriverType (Construct: stw r28(4), 0xD0(r3))");
    static_assert(offsetof(VehicleDriver, mi8NumOfInterpSteps) == 0xD4,
                  "mi8NumOfInterpSteps (Construct: stb r11(0), 0xD4(r3))");
    static_assert(offsetof(VehicleDriver, mbSnappedThisFrame) == 0xD5,
                  "mbSnappedThisFrame (Construct: stb r11(0), 0xD5(r3))");
    // The three bytes the VehicleManager takedown chain reaches at in-record 59/60/61.
    static_assert(offsetof(VehicleDriver, mControls) + 0x3B == 59, "mControls.mbBoost is in-record 59");
    static_assert(offsetof(VehicleDriver, mControls) + 0x3C == 60, "mControls.mbIsInvulnerableToVehicles is in-record 60");
    static_assert(offsetof(VehicleDriver, mControls) + 0x3D == 61, "mControls.mbIsInvulnerableToWorld is in-record 61");
}
}
