// =================================================================================================
// GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_wG12_ArticulatedJoints.cpp
//
// The articulated (cab + trailer) tail of UpdateTrafficPhysicsPostSimulation, which runs these two
// back to back as the last thing it does.
//
// ResolveArticulatedJoints -- the per-frame positional fix-up of the hitch. For each live traffic
// slot that is a CAB with a non-broken joint, find the trailer and move each half of the pair half
// the separation between the two articulation points toward the other. Pose only: it writes the
// two bodies' transform translation rows through ExternallySimulatedBody::Translate and touches no
// velocity. Dormant by data until something puts a traffic vehicle into
// E_ARTICULATE_JOINT_ATTACHED -- the joint-creation chain (CreateJoint and friends) is not
// reconstructed.
//
// ProcessJointSpys -- walks the OutJointSpy queue on the READ-locked simulation output buffer.
// THE LOOP BODY IS EMPTY ON PURPOSE: the console's only instruction in it is the checked
// GetEvent(i) call with its result discarded. The walk still runs the queue's three tripwires.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"  // the traffic body (Translate / GetTransform via its ExternallySimulatedBody base)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"               // PhysicsSimulationIO::OutputBuffer + OutJointSpyQueue
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"              // OutJointSpy
#include "GameShared/GameClasses/Core/CgsAssert.h"                                     // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                                             // Subtract / Mult / Negate
#include "rw/math/vpu/matrix44affine_operation.h"                                      // IsValid(Matrix44Affine)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                             // the witness below

#include <cstdlib>   // getenv (the BRN_TRAFFIC_DIAG witness below)

namespace
{
    // DIAG, off unless BRN_TRAFFIC_DIAG is set. DELETE-WHEN-STABLE.
    bool TrafficDiagEnabled()
    {
        static const bool sbEnabled = (getenv("BRN_TRAFFIC_DIAG") != 0);
        return sbEnabled;
    }

    // Budget: without it this fires once per resolved pair per frame.
    const s32 KI_JOINT_WITNESS_BUDGET = 24;
    s32 giJointWitnessLinesLeft = KI_JOINT_WITNESS_BUDGET;

    // Witness for "a joint was resolved". It cannot appear until the joint-creation chain lands.
    // DELETE-WHEN-STABLE.
    void JointResolveWitness(s32 liCab, s32 liTrailer, s32 liJointIndex)
    {
        if (!TrafficDiagEnabled() || CgsDev::Log::gpDebugPrint == 0
            || giJointWitnessLinesLeft <= 0)
        {
            return;
        }
        --giJointWitnessLinesLeft;
        *CgsDev::Log::gpDebugPrint
            << "[T-joint] resolved cab=" << liCab
            << " trailer=" << liTrailer
            << " joint=" << liJointIndex
            << "\n";
    }
}

namespace BrnPhysics
{
namespace Vehicle
{

void PhysicalTrafficManager::ResolveArticulatedJoints()
{
    for (s32 liTraffic = mUsedTrafficVehicles.GetFirstNonZeroBit();
         liTraffic >= 0;
         liTraffic = mUsedTrafficVehicles.GetNextNonZeroBit(liTraffic))
    {
        PhysicalTrafficVehicle* lpTrafficVehicle = GetTrafficVehicle(liTraffic);

        // Raw field read, no range assert: this one test does not go through
        // GetArticulatedVehicleType(), while every later read of the field below does.
        if (lpTrafficVehicle->meArticulatedVehicleType
                != PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_CAB)
        {
            continue;
        }
        if (!lpTrafficVehicle->HasNonBrokenJoint())
        {
            continue;
        }

        CGS_ASSERT(rw::math::vpu::IsValid(lpTrafficVehicle->mpVehicleBody->GetTransform()),
                   "Cab has invalid transform, ID ");                                   // :2570

        const s32 liJointIndex = lpTrafficVehicle->miJointIndex;
        const s32 liTrailerIndex = mArticulatedJointPool.GetIndexOfOtherHalf(
            liJointIndex,
            static_cast<s32>(lpTrafficVehicle->GetArticulatedVehicleType()));

        CGS_ASSERT(static_cast<u32>(liTrailerIndex) < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC,
                   "invalid index : ");
        if (!mUsedTrafficVehicles.IsBitSet(static_cast<u32>(liTrailerIndex)))
        {
            continue;
        }

        PhysicalTrafficVehicle* lpCab     = lpTrafficVehicle;
        PhysicalTrafficVehicle* lpTrailer = GetTrafficVehicle(liTrailerIndex);

        CGS_ASSERT(rw::math::vpu::IsValid(lpTrailer->mpVehicleBody->GetTransform()),
                   "Trailer has invalid transform, ID ");                                // :2596
        CGS_ASSERT(lpCab->GetArticulatedVehicleType()
                       == PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_CAB,
                   "lpCab->GetArticulatedVehicleType() == PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_CAB");      // :2598
        CGS_ASSERT(lpTrailer->GetArticulatedVehicleType()
                       == PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_TRAILER,
                   "lpTrailer->GetArticulatedVehicleType() == PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_TRAILER"); // :2599
        CGS_ASSERT(mArticulatedJointPool.GetIndexOfOtherHalf(
                       liJointIndex,
                       static_cast<s32>(lpTrailer->GetArticulatedVehicleType())) == liTraffic,
                   "mArticulatedJointPool.GetIndexOfOtherHalf( liJointIndex, lpTrailer->GetArticulatedVehicleType() ) == liTraffic"); // :2600

        const Vector3 lCabArticulationPoint     = lpCab->GetArticulationPointWorldSpace();
        const Vector3 lTrailerArticulationPoint = lpTrailer->GetArticulationPointWorldSpace();

        // (cab - trailer) / 2 -- the console reaches 0.5f by refining a reciprocal of 2.0f.
        const Vector3 lSeperationVectorWorld = rw::math::vpu::Mult(
            rw::math::vpu::Subtract(lCabArticulationPoint, lTrailerArticulationPoint), 0.5f);

        lpCab->mpVehicleBody->Translate(rw::math::vpu::Negate(lSeperationVectorWorld));
        lpTrailer->mpVehicleBody->Translate(lSeperationVectorWorld);

        JointResolveWitness(liTraffic, liTrailerIndex, liJointIndex);   // DIAG, not in the binary
    }
}

void PhysicalTrafficManager::ProcessJointSpys(
        const CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpSimModuleOutputBuffer)
{
    const CgsPhysics::PhysicsSimulationIO::OutputBuffer::OutJointSpyQueue* lpJointSpies =
        lpSimModuleOutputBuffer->GetJointSpyQueue();

    const s32 liNumJointSpies = lpJointSpies->GetLength();
    for (s32 liJointSpy = 0; liJointSpy < liNumJointSpies; ++liJointSpy)
    {
        // The checked accessor is the whole loop body -- see the banner.
        (void)lpJointSpies->GetEvent(liJointSpy);
    }
}

}
}
