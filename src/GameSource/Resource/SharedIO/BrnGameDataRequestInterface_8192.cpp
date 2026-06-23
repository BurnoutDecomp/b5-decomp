#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueueImpl.h"

// ============================================================================
// BrnResource::GameDataIO::RequestInterface<8192> -- instance TU.
//
// The generic RequestInterface<N> request-builder bodies live ONCE in
// BrnGameDataRequestQueueImpl.h. This TU owns the <8192> instance and forces the
// out-of-line emission the X360 ARTIST build produced for it:
//   GetVehicleList @ 0x822FD318 : GetVehicleListRequest, type 49, meType=DATA, poolId 5
//   GetWheelList   @ 0x822FD380 : GetWheelListRequest,   type 49, meType=DATA, poolId 5
//   LoadBundle     @ 0x822FD2B8 : LoadBundleRequest,     type  2 (plain AddEvent, size 148)
// (Caller: BrnWorld::RaceCarEntityModule::LoadGlobalResources.)
// ============================================================================

namespace BrnResource
{
namespace GameDataIO
{
    template struct RequestInterface<8192>;
}
}
