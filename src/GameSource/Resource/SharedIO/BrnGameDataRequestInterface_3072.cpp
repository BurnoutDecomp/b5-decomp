#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueueImpl.h"

// ============================================================================
// BrnResource::GameDataIO::RequestInterface<3072> -- instance TU.
//
// The generic RequestInterface<N> request-builder bodies live ONCE in
// BrnGameDataRequestQueueImpl.h. This TU owns the <3072> instance and forces the
// out-of-line emission the X360 ARTIST build produced for it:
//   GetAILanes               @ 0x8234A978 : GetGameDataEvent,               type 49, meType=DATA
//   GetFreeburnChallengeList @ 0x82396AB8 : GetFreeburnChallengeListRequest, type 49, meType=DATA, poolId 5
//   GetVehicleList           @ 0x82396A50 : GetVehicleListRequest,          type 49, meType=DATA, poolId 5
//   GetWheelList             @ 0x82396B20 : GetWheelListRequest,            type 49, meType=DATA, poolId 5
//   LoadAILanes              @ 0x82396980 : LoadGameDataEvent,             type 26, meType=DATA
//   LoadBundle               @ 0x8234A918 : LoadBundleRequest,             type  2 (plain AddEvent, size 148)
//   LoadTrafficLanes         @ 0x823969E8 : LoadGameDataEvent,             type 26, meType=DATA
// (Callers: BrnGameState::StreetManager::LoadAIData/LoadStreetData,
//  BrnProgression::ProgressionManager::LoadAIData/LoadProgressionData,
//  BrnGameState::GameStateModule::Prepare, BrnGameState::TriggerQueryManager::Prepare,
//  BrnGameState::StuntManager::LoadDistrictMap. The <3072> instance is the one embedded in
//  BrnGameStateModuleIO -- the 3088-byte (3072+16) reservation in BrnGameDataRequestQueue.h.)
// ============================================================================

namespace BrnResource
{
namespace GameDataIO
{
    template struct RequestInterface<3072>;
}
}
