#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleNetworkIOInterfaces.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h" // BrnTraffic::KU_MAX_TOTAL_TRAFFIC
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::IsValid (per-row matrix validity)

// BrnWorld::CrashIO::NetworkInputInterface
// Reconstructed from BURNOUT_X360_ARTIST.XEX (Construct @ 0x82592798,
// MarkRaceCarForUpdate @ 0x8254E6F8, operator= @ 0x823C8A78). Per-player network input
// view of crashing traffic: an active-race-car bitset plus one fixed-capacity (24) update
// queue per race car. AddTrafficUpdate and IsRaceCarMarkedForUpdate have out-of-line console
// copies and are bodied here; Clear is header-inline.

namespace BrnWorld
{
namespace CrashIO
{

// Reset the active-car bitset, then construct every per-car queue against its inline
// storage (each EventQueue<T,24>::Construct points the base queue at its maEvents buffer,
// sets miMaxLength = 24, miLength = 0). The X360 body inlines both the per-queue construct
// (writing mpEvents/miMaxLength) and a follow-up miLength clear; modelled as the per-queue
// Construct plus the bitset clear.
void NetworkInputInterface::Construct()
{
    mActiveRaceCars.UnSetAll();

    for (s32 liRaceCar = 0; liRaceCar < KI_MAX_ACTIVE_RACE_CARS; ++liRaceCar)
    {
        maCrashingTrafficUpdateQueues[liRaceCar].Construct();
    }
}

// Mark a race car as having a pending network update by setting its bit. The X360 body
// guards the index (>= 0, < KI_MAX_ACTIVE_RACE_CARS, and the BitArray internal bound) with
// non-gating asserts, then ORs 1 << (liRaceCarId & 63) into the single 64-bit field.
void NetworkInputInterface::MarkRaceCarForUpdate(s32 liRaceCarId)
{
    CGS_ASSERT(liRaceCarId >= 0, "liRaceCarId >= 0");
    CGS_ASSERT(liRaceCarId < KI_MAX_ACTIVE_RACE_CARS, "liRaceCarId < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");

    mActiveRaceCars.SetBit(static_cast<u32>(liRaceCarId));
}

// Queue a crashing traffic vehicle's update under its owning race car. The vehicle index, the
// owner index and the transform are asserted (non-gating), as is the owner having been marked
// for update this frame; the event {u16 vehicle id, transform} then goes onto that car's queue.
void NetworkInputInterface::AddTrafficUpdate(u32 luVehicleIndex, u32 luOwnerRaceCarIndex, Matrix44Affine lTransform)
{
    CGS_ASSERT(luVehicleIndex < BrnTraffic::KU_MAX_TOTAL_TRAFFIC,
               "luVehicleIndex < BrnTraffic::KU_MAX_TOTAL_TRAFFIC");
    CGS_ASSERT(luOwnerRaceCarIndex < static_cast<u32>(KI_MAX_ACTIVE_RACE_CARS),
               "luOwnerRaceCarIndex < (uint32_t) BrnWorld::KI_MAX_ACTIVE_RACE_CARS");

    const bool lbValid = rw::math::vpu::IsValid(lTransform.xAxis)
                      && rw::math::vpu::IsValid(lTransform.yAxis)
                      && rw::math::vpu::IsValid(lTransform.zAxis)
                      && rw::math::vpu::IsValid(lTransform.wAxis);
    CGS_ASSERT(lbValid, "rw::math::IsValid( lTransform )");

    CGS_ASSERT(mActiveRaceCars.IsBitSet(luOwnerRaceCarIndex),
               "You must mark a race car for update before adding vehicles owned by them");

    CrashingTrafficUpdateEvent lEvent;
    lEvent.muVehicleId = static_cast<u16>(luVehicleIndex);
    lEvent.mTransform  = lTransform;

    maCrashingTrafficUpdateQueues[luOwnerRaceCarIndex].AddEvent(lEvent);
}

// Whether the race car has been marked for update (its bit is set). Asserts the index range
// (non-gating) first.
bool NetworkInputInterface::IsRaceCarMarkedForUpdate(s32 liRaceCarId) const
{
    CGS_ASSERT(liRaceCarId >= 0, "liRaceCarId >= 0");
    CGS_ASSERT(liRaceCarId < KI_MAX_ACTIVE_RACE_CARS, "liRaceCarId < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");

    return mActiveRaceCars.IsBitSet(static_cast<u32>(liRaceCarId));
}

// Per-instance copy assignment. The X360 body copies the 8-byte active-car bitset, then for
// each of the 8 per-car queues resets the destination length to 0 and Appends every live
// event from the matching source queue (no buffer re-pointing -- the destination is assumed
// already Constructed). Returns *this.
NetworkInputInterface& NetworkInputInterface::operator=(const NetworkInputInterface& lOther)
{
    mActiveRaceCars = lOther.mActiveRaceCars;

    for (s32 liRaceCar = 0; liRaceCar < KI_MAX_ACTIVE_RACE_CARS; ++liRaceCar)
    {
        maCrashingTrafficUpdateQueues[liRaceCar].Clear();
        maCrashingTrafficUpdateQueues[liRaceCar].Append(lOther.maCrashingTrafficUpdateQueues[liRaceCar]);
    }

    return *this;
}

}
}
