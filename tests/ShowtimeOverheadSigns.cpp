#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityInstance.h"
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "rw/math/vpu/vector3_operation.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdlib>
#include <cstdio>

using namespace BrnWorld;
using namespace BrnGameState::GameStateModuleIO;
using namespace rw::math::vpu;
namespace CgsDev { namespace Assert {
static int assertions;
int BeginAssert(){return 0;}
int FireAssert(const char*,const char*,int){++assertions;return 0;}
void* EndAssert(){return nullptr;}
} namespace Log { decltype(gpDebugPrint) gpDebugPrint=nullptr; } }
u64 CgsDev::Message::gxMessageFilterFlags=0;
namespace BrnWorld {
u32 PropEntityInstance::GetTypeId() const {return muTypeId;}
const Matrix44Affine& PropEntityInstance::GetWorldTransform() const {return mWorldTransform;}
Matrix44Affine& PropEntityInstance::GetWorldTransform() {return mWorldTransform;}
PropEntityID::PropEntityID(u32 id) {mEntityId.muValue=id;}
u32 PropEntityID::GetPartIndex() const {return mEntityId.muValue & 1023;}
u32 PropEntityID::GetEntityIndex() const {return (mEntityId.muValue >> 10) & 16383;}
}
struct PropOutput {
    struct Physical {Vector3 mPosition;};
    struct PhysicalQueue {
        Physical event; int count = 0;
        Physical* AllocateEventSafe(){++count;return &event;}
    } physical;
    using PropBecamePhysicalEventQueue=PhysicalQueue;
    CgsModule::EventQueue<HitOverheadSignEvent,100> signs;
    PhysicalQueue* GetPropBecamePhysicalEventQueue(){return &physical;}
    CgsModule::EventQueue<HitOverheadSignEvent,100>* GetHitOverheadSignQueue(){return &signs;}
    const CgsModule::EventQueue<HitOverheadSignEvent,100>* GetHitOverheadSignQueue() const{return &signs;}
};
namespace BrnWorld { namespace PropEntityIO {
using OutputBuffer_PostPhysics=::PropOutput;
using PropBecamePhysicalEvent=::PropOutput::Physical;
} }
struct WorldOutput {
    CgsModule::VariableEventQueue<4096,16> events;
    CgsModule::VariableEventQueue<4096,16>* GetGameEventQueue(){return &events;}
};
struct VisibleIds {
    CgsSceneManager::EntityId ids[64]{}; u32 count=0;
    u32 GetLength() const{return count;}
    const CgsSceneManager::EntityId& Get(u32 i) const{return ids[i];}
    const CgsSceneManager::EntityId& GetItem(u32 i) const{return ids[i];}
} ;
struct Zone {
    PropEntityInstance props[64]{};
    PropEntityInstance* GetProp(PropEntityID id){return &props[id.GetEntityIndex()];}
};
struct Types {
    BrnPhysics::Props::PropTypeData types[8]{};
    const BrnPhysics::Props::PropTypeData* GetType(u32 i) const{return &types[i];}
};
static const u8 KU8_ENTITYTYPE_REPLAY_PROP=34;
struct Picker {
    VisibleIds lrVisibleEntities;
    Zone mZoneManager;
    Types data;
    Array<BrnGui::OverheadSignScore,32> mVisibleOverheadSigns;
    s32 miFramesUntilUpdateVisibleSigns=1;
    Vector3 mPlayerPosition{};
    const Types* GetPropPhysicsDataHeader(){return &data;}
    void Tick();
    void Discover(bool lbRenderCoronas);
};
struct CrashModeScoring {
    static constexpr s32 KI_SCORE_BONUS_PER_OVERHEAD_SIGN=10000;
    s32 miBaseScore=0;
    f32 mfTimeSinceLastEvent=3, mfTimeSinceLastHitOverheadSign=2;
    void DealWithHitOverheadSign();
};
#include "showtime_overhead_signs.inc"
static int checks,failures;
static void Check(bool ok,const char* message){++checks;if(!ok){++failures;std::printf("FAIL %s\n",message);}}
int main(){
    static Picker picker;
    picker.mVisibleOverheadSigns.Construct();
    const u32 ids[]={500950,500930,506050,428180,428152,12345};
    for(u32 i=0;i<6;++i)picker.data.types[i].muSceneUriId=ids[i];
    for(u32 i=0;i<40;++i){
        picker.lrVisibleEntities.ids[i].mId=(u32(E_ENTITYTYPE_PROP)<<24)|(i<<10);
        picker.mZoneManager.props[i].muTypeId=u16(i%5);
        picker.mZoneManager.props[i].mWorldTransform.Pos()=Vector3{float(i),0,0,0};
    }
    picker.lrVisibleEntities.count=5;
    picker.Tick();picker.Discover(true);
    Check(picker.mVisibleOverheadSigns.GetLength()==5,"all three sign types and both bonus props discovered");
    for(u32 i=0;i<picker.mVisibleOverheadSigns.GetLength();++i){
        const auto& s=picker.mVisibleOverheadSigns.GetItem(i);
        Check(s.muEntityIndex==i&&s.mWorldSpacePosition.x==i&&s.mWorldSpacePosition.y==(i<3?3:5),"entity id and recovered label offsets");
    }
    picker.mZoneManager.props[0].mu8Flags=KU_MOVED_BIT;
    picker.mZoneManager.props[1].mWorldTransform.Pos().x=100.01f;
    picker.mZoneManager.props[2].mWorldTransform.Pos().x=100.0f;
    picker.mZoneManager.props[3].muTypeId=5;
    picker.lrVisibleEntities.ids[4].mId=(34u<<24)|(4<<10);
    picker.miFramesUntilUpdateVisibleSigns=1;picker.Tick();picker.Discover(true);
    Check(picker.mVisibleOverheadSigns.GetLength()==1,"moved, distant, ordinary and replay props excluded; radius inclusive");
    if(picker.mVisibleOverheadSigns.GetLength())Check(picker.mVisibleOverheadSigns.GetItem(0).muEntityIndex==2,"100 metre boundary retained");
    picker.lrVisibleEntities.count=0;
    picker.Discover(false);
    Check(picker.miFramesUntilUpdateVisibleSigns==60&&picker.mVisibleOverheadSigns.GetLength()==1,"reflection passes leave timer and markers alone");
    for(int i=0;i<59;++i){picker.Tick();picker.Discover(true);}
    Check(picker.mVisibleOverheadSigns.GetLength()==1,"markers cached for 59 fixed simulation ticks");
    picker.Tick();picker.Discover(true);
    Check(picker.mVisibleOverheadSigns.GetLength()==0,"empty visible frame clears expired markers");
    for(u32 i=0;i<40;++i){picker.mZoneManager.props[i].mu8Flags=0;picker.mZoneManager.props[i].muTypeId=0;picker.mZoneManager.props[i].mWorldTransform.Pos().x=0;picker.lrVisibleEntities.ids[i].mId=(u32(E_ENTITYTYPE_PROP)<<24)|(i<<10);}
    picker.lrVisibleEntities.count=40;picker.miFramesUntilUpdateVisibleSigns=1;picker.Tick();picker.Discover(true);
    Check(picker.mVisibleOverheadSigns.GetLength()==32,"marker capacity bounded at 32");
    static PropOutput output;static WorldOutput world;
    output.signs.Construct();world.events.Construct();
    PropEntityInstance prop{};CrashModeScoring scorer;
    Move(false,&prop,&picker.data.types[0],513,&output);
    Check(output.signs.GetLength()==0&&prop.mu8Flags==0,"stationary sign does not score");
    Move(true,&prop,&picker.data.types[0],513,&output);
    Check(output.signs.GetLength()==1&&prop.mu8Flags==KU_MOVED_BIT,"first physical motion posts sign hit");
    Move(true,&prop,&picker.data.types[0],513,&output);
    Check(output.signs.GetLength()==1,"subsequent updates do not repost");
    Bridge(&output,&world);
    const CgsModule::Event* event=nullptr;s32 bytes=0;
    s32 id=world.events.GetFirstEvent(&event,&bytes);
    Check(id==118&&bytes==1&&event,"hit travels through the original world game-event queue");
    if(event){Check(reinterpret_cast<const HitOverheadSignEvent*>(event)->muRaceCarId==1,"original entity index truncates to one byte");scorer.DealWithHitOverheadSign();}
    Check(scorer.miBaseScore==10000,"eligible hit awards 10000");
    scorer.DealWithHitOverheadSign();Check(scorer.miBaseScore==10000,"one-second cooldown suppresses duplicate score");
    scorer.mfTimeSinceLastHitOverheadSign=1;scorer.DealWithHitOverheadSign();Check(scorer.miBaseScore==20000,"cooldown boundary awards next hit");
    output.signs.Clear();prop.mu8Flags=0;Move(true,&prop,&picker.data.types[5],0,&output);
    Check(output.signs.GetLength()==0,"ordinary props do not emit overhead-sign events");
    const int rates[]={30,60,120,144,240};
    for(int fps:rates){
        Picker cadence;cadence.mVisibleOverheadSigns.Construct();cadence.lrVisibleEntities.count=1;
        cadence.lrVisibleEntities.ids[0].Set(E_ENTITYTYPE_PROP,0,0);
        cadence.data.types[0].muSceneUriId=500950;
        int accumulator=0,ticks=0,lastRefresh=-1,refreshes=0;float last=-1;
        for(int frame=0;frame<fps*3;++frame){
            accumulator+=60;int count=accumulator/fps;accumulator%=fps;
            for(int i=0;i<count;++i){cadence.Tick();++ticks;}
            cadence.mZoneManager.props[0].mWorldTransform.Pos().x=float(ticks)*.001f;
            cadence.Discover(true);
            const int beforeReflections=cadence.miFramesUntilUpdateVisibleSigns;
            cadence.Discover(false);cadence.Discover(false);
            Check(cadence.miFramesUntilUpdateVisibleSigns==beforeReflections,"reflection passes do not consume sign cadence");
            if(cadence.mVisibleOverheadSigns.GetLength()){
                float position=cadence.mVisibleOverheadSigns.GetItem(0).mWorldSpacePosition.x;
                if(position!=last){
                    if(lastRefresh>=0)Check(ticks-lastRefresh==60,"marker refresh stays at one second of fixed ticks across presentation rates");
                    lastRefresh=ticks;last=position;++refreshes;
                }
            }
        }
        Check(refreshes==3,"three-second window has exactly three sign refreshes at every FPS");
        const int before=cadence.miFramesUntilUpdateVisibleSigns;
        for(int i=0;i<120;++i)cadence.Discover(true);
        Check(cadence.miFramesUntilUpdateVisibleSigns==before,"extra presentations without simulation do not expire markers");
        for(int i=0;i<120;++i)cadence.Tick();
        Check(cadence.miFramesUntilUpdateVisibleSigns==0,"countdown saturates while world rendering is skipped");
        cadence.Discover(true);
        Check(cadence.miFramesUntilUpdateVisibleSigns==60,"main-view rendering resumes with an immediate refresh");
    }
    Check(CgsDev::Assert::assertions==0,"no assertions");
    std::printf("ShowtimeOverheadSigns: %d checks, %d failures\n",checks,failures);return failures?1:0;
}


