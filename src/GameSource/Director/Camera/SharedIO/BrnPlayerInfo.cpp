// ============================================================================
// GameSource/Director/Camera/SharedIO/BrnPlayerInfo.cpp
//
// BrnDirector::Camera::VehicleInfo ledger functions:
//   VehicleInfo(const VehicleInfo&) @0x8221CDC8
//   operator=(const VehicleInfo&)   @0x821F49C8
//
// Both are member-wise copies of a trivially-copyable snapshot. The X360 bodies inline
// the copy: the copy ctor runs the RaceCarState copy ctor for the base member then
// VMX/word copies the director-camera fields; operator= XMemCpy's the 1120-byte
// RaceCarState then copies the same trailing fields. The reconstruction performs the
// same member-by-member copy (by name); the whole mCrashingCentreOfMass matrix
// is copied (all four 16-byte lanes 0x460/0x470/0x480/0x490) just as the X360 does.
// ============================================================================

#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"

namespace BrnDirector
{
namespace Camera
{
    // @0x8221CDC8 -- copy ctor.
    //   bl  RaceCarState::RaceCarState        (base/member copy of mRaceCarState)
    //   VMX-copy mCrashingCentreOfMass all 4 lanes (0x460/0x470/0x480/0x490), 8-dword loop = mAABB
    //   (0x4A0..0x4BF), VMX-copy mHardestNormalStress{Normal,} (0x4C0/0x4D0), then the
    //   f32 + 3 bools (0x4E0/0x4E4/0x4E5/0x4E6).
    VehicleInfo::VehicleInfo(const VehicleInfo& rhs)
        : mRaceCarState(rhs.mRaceCarState)
        , mCrashingCentreOfMass(rhs.mCrashingCentreOfMass)
        , mAABB(rhs.mAABB)
        , mHardestNormalStressNormal(rhs.mHardestNormalStressNormal)
        , mHardestNormalStress(rhs.mHardestNormalStress)
        , mfHardestImpact(rhs.mfHardestImpact)
        , mbHardestImpactIsAgainstWorld(rhs.mbHardestImpactIsAgainstWorld)
        , mbHasCrashingCenterOfMass(rhs.mbHasCrashingCenterOfMass)
        , mbEngineOn(rhs.mbEngineOn)
    {
    }

    // @0x821F49C8 -- copy assignment.
    //   XMemCpy(this, rhs, 0x460)              (mRaceCarState whole-object copy)
    //   then the same trailing-field copies as the ctor. Returns this (r3 = r31).
    VehicleInfo& VehicleInfo::operator=(const VehicleInfo& rhs)
    {
        mRaceCarState                 = rhs.mRaceCarState;
        mCrashingCentreOfMass         = rhs.mCrashingCentreOfMass;
        mAABB                         = rhs.mAABB;
        mHardestNormalStressNormal    = rhs.mHardestNormalStressNormal;
        mHardestNormalStress          = rhs.mHardestNormalStress;
        mfHardestImpact               = rhs.mfHardestImpact;
        mbHardestImpactIsAgainstWorld = rhs.mbHardestImpactIsAgainstWorld;
        mbHasCrashingCenterOfMass     = rhs.mbHasCrashingCenterOfMass;
        mbEngineOn                    = rhs.mbEngineOn;
        return *this;
    }
}
}
