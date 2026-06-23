#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleNetworkIOInterfaces.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::IsValid (per-row matrix validity)

// BrnWorld::CrashIO::NetworkOutputInterface
// Reconstructed from BURNOUT_X360_ARTIST.XEX (AddOwnedTrafficUpdate @ 0x827C3528). Network
// OUTPUT view of locally-owned crashing traffic: a single fixed-capacity (24) crashing-traffic
// update queue (the lone member at offset 0). Construct / Clear / GetCrashingTrafficUpdateQueue
// follow the queue-forwarding convention the X360 build uses for these one-member interfaces.

namespace BrnWorld
{
namespace CrashIO
{

// Total traffic-vehicle index bound. The X360 AddOwnedTrafficUpdate asserts
// "luVehicleIndex < BrnTraffic::KU_MAX_TOTAL_TRAFFIC" (the immediate is 0x258 == 600).
// Modelled as a file-visible constant rather than pulling in a shared BrnTraffic header for a
// single bound (the sibling KI_MAX_ACTIVE_RACE_CARS precedent in the network IO header).
const u32 KU_MAX_TOTAL_TRAFFIC = 600;

// (Re)construct the queue against its inline storage (EventQueue<T,24>::Construct points the
// base queue at its maEvents buffer, sets miMaxLength = 24, miLength = 0).
void NetworkOutputInterface::Construct()
{
    mCrashingTrafficUpdateQueue.Construct();
}

// Drop all queued updates (queue length reset; the backing buffer is untouched).
void NetworkOutputInterface::Clear()
{
    mCrashingTrafficUpdateQueue.Clear();
}

// Build a CrashingTrafficUpdateEvent{ (u16)luVehicleIndex, lTransform } and append it to the
// queue. The X360 body (1) asserts luVehicleIndex < KU_MAX_TOTAL_TRAFFIC, (2) asserts the
// transform is finite via the inlined per-row rw::math::IsValid( lTransform ) (each of the 4
// rows is NaN-checked lane-by-lane and AND-combined -- modelled here as the per-row IsValid
// AND-chain, the same construction CgsMicrophone uses), then (3) stores the u16 vehicle id and
// the 4 matrix rows into a stack CrashingTrafficUpdateEvent and forwards to the queue's AddEvent
// (the queue is at offset 0, so the asm passes `this` straight through). Both asserts are
// non-gating tripwires.
void NetworkOutputInterface::AddOwnedTrafficUpdate(u32 luVehicleIndex, Matrix44Affine lTransform)
{
    CGS_ASSERT(luVehicleIndex < KU_MAX_TOTAL_TRAFFIC,
               "luVehicleIndex < BrnTraffic::KU_MAX_TOTAL_TRAFFIC");

    const bool lbValid = rw::math::vpu::IsValid(lTransform.xAxis)
                      && rw::math::vpu::IsValid(lTransform.yAxis)
                      && rw::math::vpu::IsValid(lTransform.zAxis)
                      && rw::math::vpu::IsValid(lTransform.wAxis);
    CGS_ASSERT(lbValid, "rw::math::IsValid( lTransform )");

    CrashingTrafficUpdateEvent lEvent;
    lEvent.muVehicleId = static_cast<u16>(luVehicleIndex);
    lEvent.mTransform  = lTransform;

    mCrashingTrafficUpdateQueue.AddEvent(lEvent);
}

// Read-only access to the queued updates.
const NetworkOutputInterface::CrashingTrafficUpdateQueue*
NetworkOutputInterface::GetCrashingTrafficUpdateQueue() const
{
    return &mCrashingTrafficUpdateQueue;
}

}
}
