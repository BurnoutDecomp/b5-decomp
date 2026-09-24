// FX-BRIDGES (crash parity 2026-09-24): the PRODUCTION per-car VehicleInfo build of
// BrnGame::BrnGameModule::BridgeWorldToDirector @0x823E3AB0 -- the region of
// src/GameSource/Game/GameBridgeWorldToX.cpp from `BrnDirector::Camera::VehicleInfo lVehicleInfo;`
// up to the `lpDirectorInput->SetRaceCarInfo(` publish, extracted verbatim by
// run_fxbridges_vehicle_info.py -- compiled against the real VehicleInfo / RaceCarState /
// ContactSpyInterface / ContactSpyData types and run on fixtures.
//
// CC-5, the hardest-impact leg, checked against the ARTIST asm:
//   seed    0x823E4884 lvx128 unk_82181510 = (0,1,0,0) -> mHardestNormalStressNormal (var_2F0 = +0x4C0)
//           0x823E4890 vspltisw128 v127,0              -> mHardestNormalStress       (var_2E0 = +0x4D0)
//           0x823E488C stfs f31 (flt_82001CC0 = 0.0)   -> mfHardestImpact            (var_2D0 = +0x4E0)
//   gate    0x823E4A1C..0x823E4A28 `lwz r11, 0(GetContactSpy()) ; beq 0x823E4B28` (spy unbound: seed stays)
//   run     GetRaceCarContactRunList @0x82355BF0 -> GetRunDataWithEntityID @0x82373C38 (mEntityId, lwz 0x3C8);
//           NULL run: seed stays (beq 0x823E4B28)
//   loop    contact = queue + 0x10 + 0x60 * (start + i); m2 = vmsum3fp128(stress, stress) (xyz, NO sqrt);
//           vcmpgtfp128 m2 > best (STRICT; NaN never replaces); vsel normal (+0x30) / best / stress (+0x20)
//   store   0x823E4B1C lfs best lane 0 -> mfHardestImpact (an empty run publishes 0 and keeps the seed normal)
//
// CC-7, the deformed AABB arm (0x823E4924..0x823E49B4):
//   if (deformationOutput->mpDeformationState (+0x70) && GetCarStateF(state, mEntityId) @0x822CC340)
//       mAABB = { CarState +0x640 (mDeformedBBoxMin), +0x650 (mDeformedBBoxMax) }   (the lookup is re-called)
//   else  max = mHalfExtent (+0x350), min = max XOR 0x80000000 in all four lanes (vspltisw -1 ; vslw ; vxor)
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationState.h"
#include "rw/math/vpu/vector3_operation.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
}

using BrnPhysics::ContactSpy::ContactSpyInterface;
using BrnPhysics::ContactSpy::ContactSpyData;
using BrnPhysics::ContactSpy::RaceCarContact;
using BrnPhysics::ContactSpy::ContactSpyRunData;
using BrnPhysics::Vehicle::RaceCarState;

// The engine predicates the region ORs into mbEngineOn (not under test here; the real ones are
// header inlines of RCEntityActiveRaceCarOutputInterface).
struct ActiveRaceCarsFixture
{
    bool IsRaceCarEngineOn(EActiveRaceCarIndex) const       { return true; }
    bool IsRaceCarEngineStarting(EActiveRaceCarIndex) const { return false; }
};

// What the tests read back out of the staged VehicleInfo (VehicleInfo's copy operations are
// out-of-line, so the fixture copies the fields it checks).
struct Published
{
    Vector3 mMin, mMax, mNormal, mStress;
    f32     mfImpact;
};

// The region under test, wrapped with the locals it reads under their production names.
static Published BuildVehicleInfo(const ActiveRaceCarsFixture* lpActiveRaceCars,
                                  EActiveRaceCarIndex leSlot,
                                  const RaceCarState* lpState,
                                  const ContactSpyInterface* lpContactSpy,
                                  const ContactSpyData::RaceCarContactQueue* lpCarContacts,
                                  const BrnPhysics::Deformation::DeformationOutputInterface* lpDeformationOutputInterface)
{
    (void)lpContactSpy; (void)lpCarContacts; (void)lpDeformationOutputInterface;
#include "fxbridges_vehicle_info_region.inc"
    Published lOut;
    lOut.mMin     = lVehicleInfo.mAABB.mMin;
    lOut.mMax     = lVehicleInfo.mAABB.mMax;
    lOut.mNormal  = lVehicleInfo.mHardestNormalStressNormal;
    lOut.mStress  = lVehicleInfo.mHardestNormalStress;
    lOut.mfImpact = lVehicleInfo.mfHardestImpact;
    return lOut;
}

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("  FAIL %s\n", lpcName);
    }
    else
    {
        std::printf("  ok   %s\n", lpcName);
    }
}

static bool Same(const Vector3& a, const Vector3& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

static bool SameXYZ(const Vector3& a, const Vector3& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

static const u32 KU_CAR_ENTITY   = 0x01000400u;   // owner 1 (race car), index 1
static const u32 KU_OTHER_ENTITY = 0x01000800u;   // owner 1, index 2

static ContactSpyData      gSpyData;              // static storage: ~115 KB
static ContactSpyInterface gSpy;
static RaceCarState        gState;
static BrnPhysics::Deformation::DeformationOutputInterface gDeformationOutput;   // mpDeformationState NULL
static BrnPhysics::Deformation::DeformationState           gDeformationState;    // ~48 KB

static RaceCarContact MakeContact(u32 luEntity, Vector3 lStress, Vector3 lNormal)
{
    RaceCarContact lContact;
    std::memset(&lContact, 0, sizeof(lContact));
    lContact.mEntityIdA.muValue = luEntity;
    lContact.mNormalStress      = lStress;
    lContact.mNormal            = lNormal;
    return lContact;
}

// Rebuild the spy with the given contacts (all in queue order) and ONE run for luRunEntity
// covering [liStart, liStart + liLength).
static void ResetSpy(const RaceCarContact* lpContacts, s32 liCount, u32 luRunEntity, s32 liStart, s32 liLength)
{
    ContactSpyData::RaceCarContactQueue*   lpQueue = &gSpyData.mRaceCarContactQueue;
    ContactSpyData::RaceCarContactRunList* lpRuns  = &gSpyData.mRaceCarContactRunList;
    lpQueue->Construct(BrnWorld::E_ENTITYTYPE_RACECAR);
    lpRuns->Construct(BrnWorld::E_ENTITYTYPE_RACECAR);
    for (s32 i = 0; i < liCount; ++i)
        lpQueue->AddEvent(lpContacts[i]);
    if (liLength >= 0)
    {
        ContactSpyRunData lRun;
        std::memset(&lRun, 0, sizeof(lRun));
        lRun.mEntityId.muValue = luRunEntity;
        lRun.miStartIndex      = liStart;
        lRun.miRunLength       = liLength;
        lpRuns->AddEvent(lRun);
    }
    gSpy.mpData = &gSpyData;
}

static Published Run(bool lbBound)
{
    static const ActiveRaceCarsFixture klCars = {};
    if (!lbBound)
        gSpy.mpData = nullptr;
    // lpCarContacts as the production caller computes it before the loop (DWARF :87): NULL
    // unless the spy is bound.
    const ContactSpyData::RaceCarContactQueue* lpCarContacts =
        gSpy.mpData != nullptr ? gSpyData.GetRaceCarContacts() : nullptr;
    return BuildVehicleInfo(&klCars, E_ACTIVE_RACE_CAR_INDEX_1, &gState, &gSpy, lpCarContacts,
                            &gDeformationOutput);
}

int main()
{
    gState.Clear();
    gState.mEntityId.muValue = KU_CAR_ENTITY;
    gState.mHalfExtent       = Vector3{ 1.0f, 0.75f, 2.5f, 0.0f };

    const Vector3 kSeedNormal = { 0.0f, 1.0f, 0.0f, 0.0f };   // 0x82181510 (x360rd)
    const Vector3 kZero       = { 0.0f, 0.0f, 0.0f, 0.0f };

    const Vector3 kN1 = { 1.0f, 0.0f, 0.0f, 0.0f };
    const Vector3 kN2 = { 0.0f, 0.0f, 1.0f, 0.0f };
    const Vector3 kN3 = { 0.0f, -1.0f, 0.0f, 0.0f };

    // (1) three contacts |stress| 3, 5, 4: the squared 25 wins with its own normal/stress.
    {
        const RaceCarContact laContacts[3] = {
            MakeContact(KU_CAR_ENTITY, Vector3{ 0.0f, 3.0f, 0.0f, 0.0f }, kN1),
            MakeContact(KU_CAR_ENTITY, Vector3{ 3.0f, 0.0f, 4.0f, 9.0f }, kN2),   // w lane is ignored
            MakeContact(KU_CAR_ENTITY, Vector3{ 0.0f, 0.0f, -4.0f, 0.0f }, kN3),
        };
        ResetSpy(laContacts, 3, KU_CAR_ENTITY, 0, 3);
        const Published lOut = Run(true);
        Check(lOut.mfImpact == 25.0f, "(1) mfHardestImpact is the SQUARED magnitude of the hardest contact (25, not 5)");
        Check(Same(lOut.mNormal, kN2), "(1) mHardestNormalStressNormal is the hardest contact's mNormal (+0x30)");
        Check(Same(lOut.mStress, laContacts[1].mNormalStress), "(1) mHardestNormalStress is the hardest contact's mNormalStress (+0x20)");
    }

    // (2) equal magnitudes: the FIRST to reach the maximum is kept (vcmpgtfp128 is strict).
    {
        const RaceCarContact laContacts[2] = {
            MakeContact(KU_CAR_ENTITY, Vector3{ 4.0f, 0.0f, 0.0f, 0.0f }, kN1),
            MakeContact(KU_CAR_ENTITY, Vector3{ 0.0f, 4.0f, 0.0f, 0.0f }, kN2),
        };
        ResetSpy(laContacts, 2, KU_CAR_ENTITY, 0, 2);
        const Published lOut = Run(true);
        Check(lOut.mfImpact == 16.0f, "(2) equal magnitudes: mfHardestImpact 16");
        Check(Same(lOut.mNormal, kN1) && Same(lOut.mStress, laContacts[0].mNormalStress),
              "(2) equal magnitudes keep the FIRST contact (strict greater-than)");
    }

    // (3) the run starts after other cars' contacts: GetStartIndex offsets into the queue.
    {
        const RaceCarContact laContacts[4] = {
            MakeContact(KU_OTHER_ENTITY, Vector3{ 100.0f, 0.0f, 0.0f, 0.0f }, kN3),
            MakeContact(KU_OTHER_ENTITY, Vector3{ 0.0f, 100.0f, 0.0f, 0.0f }, kN3),
            MakeContact(KU_CAR_ENTITY,   Vector3{ 0.0f, 0.0f, 2.0f, 0.0f }, kN1),
            MakeContact(KU_CAR_ENTITY,   Vector3{ 1.0f, 1.0f, 1.0f, 0.0f }, kN2),
        };
        ResetSpy(laContacts, 4, KU_CAR_ENTITY, 2, 2);
        const Published lOut = Run(true);
        Check(lOut.mfImpact == 4.0f && Same(lOut.mNormal, kN1),
              "(3) only this car's run [start, start+length) is walked (4, not the other car's 10000)");
    }

    // (4) a NaN stress never replaces the running best (vcmpgtfp128 is false on NaN).
    {
        const f32 kfNaN = std::numeric_limits<f32>::quiet_NaN();
        const RaceCarContact laContacts[2] = {
            MakeContact(KU_CAR_ENTITY, Vector3{ kfNaN, 0.0f, 0.0f, 0.0f }, kN3),
            MakeContact(KU_CAR_ENTITY, Vector3{ 0.0f, 3.0f, 0.0f, 0.0f }, kN1),
        };
        ResetSpy(laContacts, 2, KU_CAR_ENTITY, 0, 2);
        const Published lOut = Run(true);
        Check(lOut.mfImpact == 9.0f && Same(lOut.mNormal, kN1), "(4) a NaN contact is skipped, the next one (9) wins");
    }

    // (5) no run for this car: the seed is published -- normal (0,1,0,0), stress 0, impact 0.
    {
        const RaceCarContact laContacts[1] = {
            MakeContact(KU_OTHER_ENTITY, Vector3{ 5.0f, 0.0f, 0.0f, 0.0f }, kN1),
        };
        ResetSpy(laContacts, 1, KU_OTHER_ENTITY, 0, 1);
        const Published lOut = Run(true);
        Check(lOut.mfImpact == 0.0f, "(5) no run for this car: mfHardestImpact 0");
        Check(Same(lOut.mNormal, kSeedNormal), "(5) no run: the seed normal is 0x82181510 = (0, 1, 0, 0)");
        Check(Same(lOut.mStress, kZero), "(5) no run: the seed stress is zero");
    }

    // (6) a zero-length run: the zero accumulator is stored, the seed normal stays.
    {
        ResetSpy(nullptr, 0, KU_CAR_ENTITY, 0, 0);
        const Published lOut = Run(true);
        Check(lOut.mfImpact == 0.0f && Same(lOut.mNormal, kSeedNormal), "(6) empty run: impact 0, seed normal (0, 1, 0, 0)");
    }

    // (7) the spy is not bound: the whole leg is skipped (seed).
    {
        const Published lOut = Run(false);
        Check(lOut.mfImpact == 0.0f && Same(lOut.mNormal, kSeedNormal) && Same(lOut.mStress, kZero),
              "(7) unbound spy: the seed is published");
    }

    // (8) the undeformed AABB arm is unchanged: min = -mHalfExtent, max = mHalfExtent.
    {
        const Published lOut = Run(false);
        Check(SameXYZ(lOut.mMax, gState.mHalfExtent)
              && lOut.mMin.x == -1.0f && lOut.mMin.y == -0.75f && lOut.mMin.z == -2.5f,
              "(8) half-extent AABB arm (no deformation state)");
    }

    // (9) the deformation state owns this car: the camera box is the CRUSHED box, as published.
    const Vector3 kDeformedMin = { -0.875f, -0.5f, -1.5f, 0.0f };   // a crushed nose: min.z moved in
    const Vector3 kDeformedMax = {  1.0f,   0.625f, 2.5f, 0.0f };
    gDeformationState.mxLiveSlots.SetBit(4);
    gDeformationState.maCarIds[4] = KU_CAR_ENTITY;
    gDeformationState.maCarStates[4].mDeformedBBoxMin = kDeformedMin;
    gDeformationState.maCarStates[4].mDeformedBBoxMax = kDeformedMax;
    gDeformationOutput.mpDeformationState = &gDeformationState;
    {
        const Published lOut = Run(false);
        Check(Same(lOut.mMin, kDeformedMin) && Same(lOut.mMax, kDeformedMax),
              "(9) live deformation state owning the car: mAABB = CarState mDeformedBBoxMin / Max (+0x640 / +0x650)");
    }

    // (10) the state is live but owns only another car: the pristine half-extent box.
    gDeformationState.maCarIds[4] = KU_OTHER_ENTITY;
    {
        const Published lOut = Run(false);
        Check(SameXYZ(lOut.mMax, gState.mHalfExtent) && lOut.mMin.z == -2.5f,
              "(10) GetCarStateF finds no record for this car: half-extent arm");
    }

    // (11) no deformation state at all: the half-extent arm, and its min is the vxor of ALL FOUR
    //      lanes (w too) -- a w of 0.5 publishes -0.5, a w of 0 publishes -0.0.
    gDeformationOutput.mpDeformationState = nullptr;
    gState.mHalfExtent.w = 0.5f;
    {
        const Published lOut = Run(false);
        Check(lOut.mMin.w == -0.5f && lOut.mMax.w == 0.5f, "(11) half-extent arm: min.w = -max.w (vxor sign mask on w)");
    }
    gState.mHalfExtent.w = 0.0f;
    {
        const Published lOut = Run(false);
        Check(lOut.mMin.w == 0.0f && std::signbit(lOut.mMin.w), "(12) half-extent arm: a zero w publishes -0.0 (sign flipped)");
    }

    std::printf("FxBridgesVehicleInfo: %u checks, %u failures (%u asserts fired)\n", gChecks, gFailures, gAsserts);
    return gFailures == 0 ? 0 : 1;
}
