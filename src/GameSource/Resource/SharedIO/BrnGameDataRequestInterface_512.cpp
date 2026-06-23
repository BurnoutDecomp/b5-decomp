#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueueImpl.h"

// ============================================================================
// BrnResource::GameDataIO::RequestInterface<512> -- instance TU.
//
// The generic RequestInterface<N> request-builder bodies live ONCE in
// BrnGameDataRequestQueueImpl.h. This TU owns the <512> instance and forces the
// out-of-line emission the X360 ARTIST build produced for it:
//   GetAILanes        @ 0x822563C0 : GetGameDataEvent,       type 49, meType=DATA
//   GetICEList        @ 0x82256358 : GetICEListRequest,      type 49, meType=DATA, poolId 5
//   GetVehicleList    @ 0x822562F0 : GetVehicleListRequest,  type 49, meType=DATA, poolId 5
//   LoadPVS           @ 0x822FD250 : LoadGameDataEvent,      type 26, meType=DATA
//   LoadTrafficLanes  @ 0x82256288 : LoadGameDataEvent,      type 26, meType=DATA
// (Callers: BrnDirector::WorldMap::LoadData, BrnDirector::DirectorResourceManager::
//  Prepare, BrnWorld::PVSModule::Prepare.)
// ============================================================================

namespace BrnResource
{
namespace GameDataIO
{
    template struct RequestInterface<512>;
}
}
