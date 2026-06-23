#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueueImpl.h"

// ============================================================================
// BrnResource::GameDataIO::RequestInterface<4096> -- instance TU.
//
// The generic RequestInterface<N> request-builder bodies live ONCE in
// BrnGameDataRequestQueueImpl.h (included above). This TU owns the <4096> instance:
// the methods the X360 emits out-of-line for <4096> are:
//   GetSurfaceList     @ 0x822F1EF0 : LoadGameDataEvent,     type 26, meType=ATTRIBS
//   GetVehicleList     @ 0x82746928 : GetVehicleListRequest, type 49, meType=DATA, poolId 5
//   LoadBundle         @ 0x8229B500 : LoadBundleRequest,     type  2 (plain AddEvent)
//   LoadTrafficLanes   @ 0x827468C0 : LoadGameDataEvent,     type 26, meType=DATA
//   LoadWorldCollision @ 0x822F1E88 : LoadGameDataEvent,     type 26, meType=PHYSICS
// ============================================================================

namespace BrnResource
{
namespace GameDataIO
{
    // Explicit instantiation of the <4096> request interface (this TU's instance).
    template struct RequestInterface<4096>;
}
}
