#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cstdarg>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace CgsDev {
namespace Assert {int BeginAssert(){return 0;}int FireAssert(const char*,const char*,int){return 0;}void* EndAssert(){return nullptr;}}
namespace DebugUI {struct Metrics {float mfScreenWidth=1280;};}
struct DebugRender {
    enum Justify {E_JUSTIFY_RIGHT};
    void Draw2DTextJustified(const char* text,Vector2,Justify,float,u32);
};
struct UI {DebugUI::Metrics metrics;const DebugUI::Metrics& GetMetrics(){return metrics;}};
static DebugRender renderer;static UI ui;static std::vector<std::string> pending;
void DebugRender::Draw2DTextJustified(const char* text,Vector2,Justify,float,u32){pending.emplace_back(text);}
struct DebugInterface {DebugRender& Get2dRender(){return renderer;}UI& GetUI(){return ui;}};
}
namespace CgsCore {
int SPrintf(char* dst,size_t size,const char* fmt,...){va_list args;va_start(args,fmt);int n=vsnprintf(dst,size,fmt,args);va_end(args);return n;}
}
namespace BrnWorld {
enum ERaceCarType {E_RACE_CAR_TYPE_INACTIVE,E_RACE_CAR_TYPE_AI,E_RACE_CAR_TYPE_COUNT};
struct PhysicsState {Matrix44Affine mTransform{};Vector3 mLinearVelocity{},mAngularVelocity{};};
struct ActiveRaceCar {PhysicsState state;const PhysicsState* GetPhysicsState()const{return &state;}};
struct RaceCar {
    bool active=false;ERaceCarType type=E_RACE_CAR_TYPE_INACTIVE;Vector3 position{};
    bool HasActiveRaceCar()const{return active;}
    bool IsInWorld()const{return active;}
    bool IsAIDriven()const{return type==E_RACE_CAR_TYPE_AI;}
    bool IsInRangeRival()const{return type==E_RACE_CAR_TYPE_AI&&active;}
    ERaceCarType GetType()const{return type;}
    Vector3 GetPosition()const{return position;}
};
struct RaceCarEntityModule {
    RaceCar maRaceCars[E_GLOBAL_RACE_CAR_INDEX_COUNT]{};
    ActiveRaceCar player;
    bool mbDisplayPlayerCarPosition=true,mbRivalDebug=false;
    EActiveRaceCarIndex index=E_ACTIVE_RACE_CAR_INDEX_0;
    EActiveRaceCarIndex GetPlayerActiveRaceCarIndex()const{return index;}
    const ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex)const{return &player;}
    void DebugRenderPosition()const;
};
}
#include "player_position_overlay.inc"
static int checks,failures;
static void Check(bool ok,const char* text){++checks;if(!ok){++failures;printf("FAIL %s\n",text);}}
static void RunFrame(BrnWorld::RaceCarEntityModule& car,int ticks){
    for(int i=0;i<ticks;++i){
        car.player.state.mTransform.wAxis.x+=1;
        if(kPhysicsSubmits&&car.mbDisplayPlayerCarPosition)car.DebugRenderPosition();
    }
    if(kRenderSubmits)car.DebugRenderPosition();
}
int main(){
    const int rates[]={30,60,120,144,240};
    for(int fps:rates){
        BrnWorld::RaceCarEntityModule car;car.maRaceCars[0].active=true;
        car.player.state.mTransform.zAxis={0,0,1,0};
        int accumulator=0,totalTicks=0,missing=0,duplicates=0;
        for(int frame=0;frame<fps;++frame){
            CgsDev::pending.clear();accumulator+=60;int ticks=accumulator/fps;accumulator%=fps;totalTicks+=ticks;
            RunFrame(car,ticks);
            if(CgsDev::pending.size()<4)++missing;
            if(CgsDev::pending.size()>4)++duplicates;
            Check(CgsDev::pending.size()==4,"exactly four rows on every presented frame");
            if(CgsDev::pending.size()==4){
                char expected[64];snprintf(expected,sizeof(expected),"Pos: %5.1f, %5.1f, %5.1f",float(totalTicks),0.0,0.0);
                Check(CgsDev::pending[0]==expected,"zero-step frames retain latest completed physics values");
            }
        }
        Check(totalTicks==60,"simulation remains 60 Hz at every presentation rate");
        printf("cadence %d FPS: missing=%d duplicate=%d physicsTicks=%d\n",fps,missing,duplicates,totalTicks);
        car.mbDisplayPlayerCarPosition=false;CgsDev::pending.clear();RunFrame(car,0);
        Check(CgsDev::pending.empty(),"display toggle hides the overlay immediately without a physics tick");
    }
    BrnWorld::RaceCarEntityModule car;CgsDev::pending.clear();car.DebugRenderPosition();
    Check(CgsDev::pending.empty(),"no active car produces no overlay");
    car.index=E_ACTIVE_RACE_CAR_INDEX_INVALID;car.DebugRenderPosition();
    Check(CgsDev::pending.empty(),"unassigned player is safe before preparation");
    car.index=E_ACTIVE_RACE_CAR_INDEX_0;car.maRaceCars[0].active=true;car.mbRivalDebug=true;
    car.maRaceCars[1].active=true;car.maRaceCars[1].type=BrnWorld::E_RACE_CAR_TYPE_AI;
    car.maRaceCars[1].position={3,0,4,0};car.DebugRenderPosition();
    Check(CgsDev::pending.size()==6,"optional rival rows retained");
    printf("PlayerPositionOverlay: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
