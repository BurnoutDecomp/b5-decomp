// =================================================================================================
// GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_wG_CrashingIds.cpp
//
// PhysicalTrafficManager::PassNearbyCrashingTrafficIdsToRaceCarModule -- the per-frame traffic
// pass-by stage of VehicleManager::UpdateVehiclePhysics. Every live traffic slot that is CRASHING
// and within the pass-by radius of the player car posts its GLOBAL traffic entity id into the
// manager-output interface's 10-slot mFineTrafficCrashedEventQueue (distinct from the 20-slot
// mCrashedTrafficEventQueue the crash-response arms fill). Its reader,
// RaceCarEntityModule::UpdateNearMisses, is not reconstructed yet.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h" // VehicleManagerOutputInterface + the fine queue
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"          // TrafficCrashedEvent
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h" // GetPosition()
#include "rw/math/vpu/vector3_operation.h"                                    // vpu::Subtract / Dot
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdlib>   // getenv (the BRN_TRAFFIC_DIAG witness below)

namespace BrnPhysics
{
namespace Vehicle
{
    namespace
    {
        // The squared pass-by radius (a plain float literal in the console's constant pool,
        // shared with several other bodies that also splat it into a vector lane).
        const f32 KF_NEARBY_CRASHING_TRAFFIC_RANGE_SQUARED = 60.0f;

        // DIAG, off unless BRN_TRAFFIC_DIAG is set. DELETE-WHEN-STABLE.
        bool TrafficDiagEnabled()
        {
            static const bool sbEnabled = (getenv("BRN_TRAFFIC_DIAG") != 0);
            return sbEnabled;
        }
    }

    // lPlayerPosition is the player race car's transform translation row, passed by value by the
    // sole caller. As shipped there is no "is there a player?" guard on the caller side.
    void PhysicalTrafficManager::PassNearbyCrashingTrafficIdsToRaceCarModule(
        VehicleManagerOutputInterface* lpVehicleManagerOutputInterface, Vector3 lPlayerPosition)
    {
        // DIAG only. Counts what this frame actually forwarded.
        s32 liDiagPosted = 0;

        for (s32 liVehicle = mUsedTrafficVehicles.GetFirstNonZeroBit();
             liVehicle != TotalPhysicalTrafficBitArray::KI_INVALID_BITINDEX;
             liVehicle = mUsedTrafficVehicles.GetNextNonZeroBit(liVehicle))
        {
            // Unconditional, and BEFORE the crashing test -- see the faithfulness notes.
            const Vector3 lvTrafficPosition =
                GetTrafficVehicle(liVehicle)->mpVehicleBody->GetPosition();

            // The console narrows the slot index here before the second accessor call.
            const u16 lu16Vehicle = static_cast<u16>(liVehicle);
            if (GetTrafficVehicle(static_cast<s32>(lu16Vehicle))->mePhysicalTrafficState
                    != static_cast<u32>(E_TRAFFIC_TYPE_CRASHING))
            {
                continue;
            }

            const Vector3 lvDelta =
                rw::math::vpu::Subtract(lPlayerPosition, lvTrafficPosition);
            const f32 lfSquareDist = rw::math::vpu::Dot(lvDelta, lvDelta);

            // Written as a negated `<` so a NaN distance is skipped, as the console's compare
            // does; do not rewrite this as `>=`.
            if (!(lfSquareDist < KF_NEARBY_CRASHING_TRAFFIC_RANGE_SQUARED))
            {
                continue;
            }

            // Zeroed volume-instance slot + the GLOBAL traffic entity id in the second field,
            // read straight out of the id table (no accessor -- see the notes).
            TrafficCrashedEvent lEvent;
            lEvent.mTrafficVolumeInstanceID.muId = 0;
            lEvent.mCrasherEntityID              = maTrafficEntityIDs[liVehicle];

            VehicleManagerOutputInterface::FineTrafficCrashedEventQueue& lrQueue =
                lpVehicleManagerOutputInterface->GetFineTrafficCrashedEventQueue();

            // Caller-side capacity gate: a full queue drops the id, it does not assert.
            if (lrQueue.GetMaxLength() > lrQueue.GetLength())
            {
                lrQueue.AddEvent(lEvent);
                ++liDiagPosted;
            }
        }

        // [T-pass] DIAG, not in the shipped binary. DELETE-WHEN-STABLE.
        if (liDiagPosted > 0 && TrafficDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[T-pass] fineCrashedIdsPosted=" << liDiagPosted
                << " queueLen="
                << lpVehicleManagerOutputInterface->GetFineTrafficCrashedEventQueue().GetLength()
                << "\n";
        }
    }
}   // namespace Vehicle
}   // namespace BrnPhysics
