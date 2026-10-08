// Execute the production car-cache and offline minimap paths. Neighboring
// resource lookups are fixtures; CgsIDs deliberately share their low word.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using u8=uint8_t; using u16=uint16_t; using u32=uint32_t;
using u64=uint64_t; using s32=int32_t; using CgsID=u64;
static unsigned assertions=0, checks=0, failures=0;
#define CGS_ASSERT(c, ...) do { if (!(c)) ++assertions; } while(0)
namespace CgsDev {
namespace Assert { void BeginAssert(){} void EndAssert(){} void FireAssert(const char*,const char*,s32){++assertions;} }
namespace Log { struct Stream { template<class T> Stream& operator<<(T){return *this;} }; Stream* gpDebugPrint=nullptr; }
}
namespace CgsModule { struct Event {}; }
struct Vector3 { float x=0,y=0,z=0; void SetZero(){x=y=z=0;} };
namespace BrnResource {
struct VehicleListEntry { CgsID id,parent; CgsID GetParentId()const{return parent;} };
struct VehicleList {
    std::vector<VehicleListEntry> entries;
    s32 GetVehicleIndex(CgsID id)const { for(s32 i=0;i<s32(entries.size());++i)if(entries[i].id==id)return i; return -1; }
    const VehicleListEntry* GetVehicleData(s32 i)const{return i>=0&&i<s32(entries.size())?&entries[i]:nullptr;}
    const VehicleListEntry* GetVehicleData(CgsID id)const{return GetVehicleData(GetVehicleIndex(id));}
};
}
namespace BrnProgression {
struct ProfileEvent {
    enum {E_FLAG_RANK_WIN=2,E_FLAG_NON_RANK_WIN=4,E_FLAG_WON_SPECIAL_EVENT_BEFORE=8};
    u32 id=37; u16 flags=0; u32 GetID()const{return id;} u16 GetFlags()const{return flags;}
};
struct RaceEventData { u8 type=5; CgsID car=0; u8 GetEventTypeByte()const{return type;} CgsID GetSpecialEventCarId()const{return car;} };
}
namespace BrnGui {
struct SatNavEventDisplayInfo { Vector3 mv3Position; };
struct WorldDataController {
    BrnResource::VehicleList vehicles;
    BrnProgression::RaceEventData event;
    const BrnResource::VehicleList* GetVehicleList()const{return &vehicles;}
    const BrnProgression::RaceEventData* GetEventInfoFromEventId(u32)const{return &event;}
};
struct GuiCache {
    WorldDataController* mpWorldDataController=nullptr;
    CgsID mLocalPlayerCarId=0,mLocalPlayerOriginalCarId=0;
    BrnProgression::ProfileEvent event; SatNavEventDisplayInfo display;
    CgsID GetOriginalCarId(CgsID);
    CgsID GetLocalPlayerOriginalCarId()const{return mLocalPlayerOriginalCarId;}
    WorldDataController* GetWorldDataController()const{return mpWorldDataController;}
    const BrnProgression::ProfileEvent* GetProfileEvent(u32)const{return &event;}
    const SatNavEventDisplayInfo* GetProfileEventDisplayInfo(u32)const{return &display;}
    void RecEvent(const CgsModule::Event* lpEvent,s32 liEventId) { switch(liEventId) {
#include "route_cache_arm.inc"
    default:break; } }
};
struct GuiEventEnableSatNavIcons { enum {E_ICON_DISPLAY_TYPE_OFFLINE_EVENTS=0}; };
enum IconType {E_SATNAVICON_EVENT_NOTATTEMPTED=0,E_SATNAVICON_EVENT_COMPLETED=1};
struct IconRendererSatNavIconInfo { Vector3 mv3Position; s32 miEventId=0; u32 muEventTypeIndex=99; IconType meSatNavIconType=E_SATNAVICON_EVENT_NOTATTEMPTED; };
static const u32 KU_MAX_SATNAV_ICONS=150;
static const u32 KAU_EVENTTYPE_TO_ICONROW[]={1,1,1,1,1,2};
void FireSatNavAssert(const char*,int){++assertions;}
struct SatNavRenderer {
    GuiCache* mpGuiCache=nullptr; int meIconDisplayType=0; u32 muNumberOfSatNavIcons=0;
    IconRendererSatNavIconInfo maCachedSatNavIcons[150];
    void GetIconInformation(u32 luIndex,IconRendererSatNavIconInfo* lpInfo)const { switch(meIconDisplayType) {
#include "route_icon_arm.inc"
    default:break; } }
    void RefreshSatNavIconInfo(s32);
};
#include "route_bodies.inc"
}
static void expect(bool ok,const char* label){++checks;if(!ok){++failures;std::printf("FAIL %s\n",label);}}
namespace BrnGameState {namespace GameStateModuleIO {
enum {E_ACTION_RESET_PLAYER_CAR=0};
struct ResetPlayerCarAction {u8 positionAndDirection[32];CgsID mCarModelId;};
}}
struct GuiInput {
    int type=-1;u32 size=0;CgsID value=0;
    GuiInput* GetGuiEvents(){return this;}
    void AddEvent(const CgsModule::Event* payload,int id,u32 bytes){type=id;size=bytes;std::memcpy(&value,payload,bytes);}
};
static void translate(GuiInput* lpGuiInput,const CgsModule::Event* lpAction,int liActionType){
    switch(liActionType){
#include "route_bridge_arms.inc"
    default:break;
    }
}
int main(){
    constexpr CgsID base=0x1234567800000037ull,other=0xabcdef0100000037ull;
    constexpr CgsID paint=0x1111222200000037ull,livery=0x3333444400000037ull;
    BrnGui::WorldDataController world;
    world.vehicles.entries={{base,0},{other,0},{paint,base},{livery,paint}};
    BrnGui::GuiCache cache; cache.mpWorldDataController=&world;
    expect(cache.GetOriginalCarId(base)==base,"base stays base");
    expect(cache.GetOriginalCarId(paint)==base,"one parent");
    expect(cache.GetOriginalCarId(livery)==base,"two parents");
    expect(cache.GetOriginalCarId(99)==99,"unknown id preserved");
    cache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&livery),415);
    expect(cache.mLocalPlayerCarId==livery&&cache.GetLocalPlayerOriginalCarId()==base,"whole IDs cached");
    BrnGui::SatNavRenderer map; map.mpGuiCache=&cache; world.event.car=base;
    BrnGui::IconRendererSatNavIconInfo icon;
    map.GetIconInformation(0,&icon);
    expect(icon.muEventTypeIndex==0,"matching route distinguished");
    world.event.car=other; map.GetIconInformation(0,&icon);
    expect(icon.muEventTypeIndex==2,"same low word is a different car");
    world.event.car=base; cache.event.flags=BrnProgression::ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE;
    map.GetIconInformation(0,&icon);
    expect(icon.muEventTypeIndex==0&&icon.meSatNavIconType==BrnGui::E_SATNAVICON_EVENT_COMPLETED,"completion retained");
    map.RefreshSatNavIconInfo(37);
    expect(map.muNumberOfSatNavIcons==1&&map.maCachedSatNavIcons[0].muEventTypeIndex==0,"newly revealed matching route");
    map.RefreshSatNavIconInfo(37); expect(map.muNumberOfSatNavIcons==1,"repeat does not duplicate");
    world.event.car=other;map.RefreshSatNavIconInfo(38);
    expect(map.maCachedSatNavIcons[1].muEventTypeIndex==2,"new other-car route stays ordinary");
    GuiInput input;
    translate(&input,reinterpret_cast<const CgsModule::Event*>(&base),1);
    expect(input.type==415&&input.size==8&&input.value==base,"car change publishes whole ID as event 415");
    cache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&input.value),input.type);
    expect(cache.mLocalPlayerCarId==base,"change reaches cache");
    translate(&input,reinterpret_cast<const CgsModule::Event*>(&other),2);
    expect(input.type==76&&input.size==8&&input.value==other,"unlock publishes GuiCarUnlockEvent 76");
    cache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&input.value),input.type);
    expect(cache.mLocalPlayerCarId==base,"unlock preserves current car");
    BrnGameState::GameStateModuleIO::ResetPlayerCarAction reset={};reset.mCarModelId=livery;
    translate(&input,reinterpret_cast<const CgsModule::Event*>(&reset),0);
    expect(input.type==415&&input.size==8&&input.value==livery,"reset reads model after transform");
    cache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&input.value),input.type);
    expect(cache.mLocalPlayerCarId==livery&&cache.mLocalPlayerOriginalCarId==base,"reset normalizes selected model");
    expect(assertions==0,"valid inputs do not assert");
    std::printf("BurningRouteGui: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
