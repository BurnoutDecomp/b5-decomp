#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueueImpl.h"

// ============================================================================
// BrnResource::GameDataIO::RequestInterface<1024> -- instance TU.
//
// The generic RequestInterface<N> request-builder bodies live ONCE in
// BrnGameDataRequestQueueImpl.h. This TU owns the <1024> instance and forces the
// out-of-line emission the X360 ARTIST build produced for it:
//   GetPropInstances @ 0x82303E08 : GetGameDataEvent,  type 49, meType=PHYSICS
//                                    (id = CgsIDCompress(SPrintf("PRP_INST_%d", idx)))
//   LoadPropPhysics  @ 0x82303DA0 : LoadGameDataEvent, type 26, meType=PHYSICS
// (Callers: BrnWorld::PropEntityModule::UpdateInstanceStreaming / ::Prepare.)
// ============================================================================

namespace BrnResource
{
namespace GameDataIO
{
    template struct RequestInterface<1024>;
}
}
