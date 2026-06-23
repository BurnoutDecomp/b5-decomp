#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueueImpl.h"

// ============================================================================
// BrnResource::GameDataIO::RequestInterface<32768> -- instance TU.
//
// The generic RequestInterface<N> request-builder bodies live ONCE in
// BrnGameDataRequestQueueImpl.h. This TU owns the <32768> instance and forces the
// out-of-line emission the X360 ARTIST build produced for it:
//   GetFreeburnChallengeList @ 0x8250BBE0 : GetFreeburnChallengeListRequest,
//                                            type 49, meType=DATA, poolId 5
//   GetVehicleList           @ 0x82512CF0 : GetVehicleListRequest, type 49, meType=DATA, poolId 5
//   LoadBundle               @ 0x823CE558 : LoadBundleRequest,     type  2 (plain AddEvent, size 148)
// (Callers: BrnGui::WorldDataController::Prepare, BrnGame::BrnGameModule::GamePrepare.)
// ============================================================================

namespace BrnResource
{
namespace GameDataIO
{
    template struct RequestInterface<32768>;
}
}
