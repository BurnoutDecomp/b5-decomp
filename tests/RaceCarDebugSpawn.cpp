#include <cstdio>
#include <cstring>
#include "types.hpp"
#include "rw/math/vpu/types.h"
// The diagnostic witness is inactive in this production-body fixture.
#include <cstdlib>
namespace CgsDev::Log {
struct DebugPrint { void AppendFormat(const char*,...) {} };
static DebugPrint* gpDebugPrint=nullptr;
}
using rw::math::vpu::Vector3;
using rw::math::vpu::Matrix44Affine;
using CgsID = u64;
using EActiveRaceCarIndex = s32;
constexpr s32 E_ACTIVE_RACE_CAR_INDEX_COUNT=8;
struct GameModeParams {
    s32 miNumNetworkPlayers=1;
    s32 maNetworkPlayerID[8]={11,22};
    s32 mLocalNetworkPlayerID=11;
    CgsID maModelIds[8]={100,200};
    Vector3 GetStartPosition(s32 i) const { return {float(i),20,30,1}; }
    Vector3 GetStartDirection(s32) const { return {1,0,0,0}; }
};
namespace BrnWorld {
struct RaceCarEntityModuleDebugComponent {
    bool mbAirRam=false, mbForceOnlineFreeburnSpawnPosition=false;
    float mMag=17.5f;
    static void AirRamAddNew(void*);
};
struct RaceCarEntityModule {
    RaceCarEntityModuleDebugComponent mRCEntityModuleDebugComponent;
    bool Respawn(bool lbIsFreeburnLobby,const GameModeParams* lpGameModeParams,CgsID lLocalCarModelId,s32& liFoundGrid);
    Vector3 Position(bool lbIsFreeburnLobby,bool lbIsLocal,const GameModeParams* lpGameModeParams,
                     s32 liGridPosition,Vector3 lLocalPosition,Vector3 lLocalDirection,Vector3& lChosenDirection);
};
#include "race_car_debug_spawn.inc"
}
static unsigned suChecks,suFailures;
static void Check(bool good,const char* label) { ++suChecks; if(!good) { ++suFailures; std::printf("FAIL %s\n",label); } }
static bool Same(Vector3 a,Vector3 b) { return a.x==b.x && a.y==b.y && a.z==b.z && a.w==b.w; }
int main() {
    BrnWorld::RaceCarEntityModule module;
    GameModeParams params;
    s32 grid=-1;
    Check(!module.Respawn(true,&params,100,grid) && grid==0,"default same-model local lobby car is retained");
    module.mRCEntityModuleDebugComponent.mbForceOnlineFreeburnSpawnPosition=true;
    Check(module.Respawn(true,&params,100,grid),"live debug toggle forces a same-model local respawn");
    module.mRCEntityModuleDebugComponent.mbForceOnlineFreeburnSpawnPosition=false;
    Check(module.Respawn(true,&params,101,grid),"model changes still respawn with the toggle off");
    Check(module.Respawn(false,&params,100,grid),"non-lobby grid still respawns with the toggle off");
    params.mLocalNetworkPlayerID=22;
    Check(!module.Respawn(true,&params,200,grid) && grid==1,"roster identity selects the local player's actual grid row");
    Vector3 local={700,8,-900,1},direction={0,0,-1,0},chosen={};
    Check(Same(module.Position(true,true,&params,1,local,direction,chosen),local) && Same(chosen,direction),
          "default lobby keeps local position and direction");
    module.mRCEntityModuleDebugComponent.mbForceOnlineFreeburnSpawnPosition=true;
    Check(Same(module.Position(true,true,&params,0,local,direction,chosen),{3008.169921875f,-1.159999966621399f,-1874.300048828125f,0}) && Same(chosen,direction),
          "forced local lobby position uses original grid-row-zero table entry");
    Check(Same(module.Position(true,true,&params,1,local,direction,chosen),{3172.3798828125f,-3.359999895095825f,-2006.1700439453125f,0}),
          "forced position follows the grid row rather than the active slot");
    Check(Same(module.Position(true,false,&params,1,local,direction,chosen),{0,0,0,0}) && Same(chosen,direction),
          "remote lobby car still starts at origin pending its network pose");
    Check(Same(module.Position(false,true,&params,1,local,direction,chosen),params.GetStartPosition(1)) && Same(chosen,params.GetStartDirection(1)),
          "non-lobby start-grid transform ignores the debug lobby toggle");
    BrnWorld::RaceCarEntityModuleDebugComponent other;
    module.mRCEntityModuleDebugComponent.mbAirRam=false;
    BrnWorld::RaceCarEntityModuleDebugComponent::AirRamAddNew(&module.mRCEntityModuleDebugComponent);
    Check(module.mRCEntityModuleDebugComponent.mbAirRam && !other.mbAirRam,"original Apply Ram latches only its own diagnostic flag");
    Check(module.mRCEntityModuleDebugComponent.mMag==17.5f,"Apply Ram preserves the otherwise unused Magnitude value");
    BrnWorld::RaceCarEntityModuleDebugComponent::AirRamAddNew(&module.mRCEntityModuleDebugComponent);
    Check(module.mRCEntityModuleDebugComponent.mbAirRam && module.mRCEntityModuleDebugComponent.mMag==17.5f,"repeated Apply Ram remains a latched flag");
    std::printf("RaceCarDebugSpawn: %u checks, %u failures\n",suChecks,suFailures);
    return suFailures ? 1 : 0;
}
