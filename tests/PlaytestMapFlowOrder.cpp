#include "types.hpp"
#include <cstdio>
static unsigned guChecks,guFailures;
static int gaOrder[3],giOrder;
namespace CgsModule {struct Event {};}
#undef CGS_ASSERT
#define CGS_ASSERT(test,msg) do {if(!(test)) ++guFailures;}while(0)
namespace BrnGui {
class MapIconManager {
public:
    enum {E_SATNAV_MAP=1};
    bool mbIconsVisible=true;
    s32 miNumUsedIcons=9,mOwnerId=1,miHiddenPasses=0;
    void SetIconsVisible(bool);
    void UpdateSatNavIcons() {++miHiddenPasses;}
    void UpdateCrashNavIcons() {++miHiddenPasses;}
};
struct SatNavShowHidePayload {s32 miOne;f32 mfDelay;u8 mu8Show;u8 pad[3];};
struct SatNavComponent {
    enum {KI_EVENT_SHOW_HIDE=213};
    MapIconManager* mpIconManager;
    void RecvEvent(const CgsModule::Event*,s32);
    void Destruct() {if(mpIconManager->mOwnerId==1) mpIconManager->mOwnerId=0;}
};
struct FBurnMainHudState {
    bool mbSatNavEnabled;
    SatNavComponent mSatNavComponent;
    void OnLeave();
};
struct FixtureCache {};
struct GuiCachePayload {FixtureCache* mpGuiCache;};
struct CrashNavMap {
    FixtureCache* mpGuiCache=nullptr;
    MapIconManager* mpIconManager;
    void ResetIconManager(const CgsModule::Event*) {
        mpIconManager->mOwnerId=2;
        mpIconManager->miNumUsedIcons=5;
    }
    void Bind(const GuiCachePayload*,const CgsModule::Event*);
};
struct FixtureHudFlow {
    FBurnMainHudState* mpHud;
    bool mbLeaving;
    void Update() {gaOrder[giOrder++]=1;if(mbLeaving){mbLeaving=false;mpHud->OnLeave();}}
};
struct FixtureScreenFlow {
    CrashNavMap* mpMap;
    FixtureCache mCache;
    bool mbBinding;
    void Update() {
        gaOrder[giOrder++]=2;
        if(mbBinding){mbBinding=false;GuiCachePayload cache{&mCache};CgsModule::Event event;mpMap->Bind(&cache,&event);}
    }
};
struct FixtureOverlayFlow {void Update() {gaOrder[giOrder++]=3;}};
struct FixtureModule {
    FixtureHudFlow mHudFlow;
    FixtureScreenFlow mScreenFlow;
    FixtureOverlayFlow mOverlayFlow;
    void Tick();
};
}
#include "playtest_map_flow_order_bodies.inc"
static void Check(bool b,const char* label) {
    ++guChecks;if(!b){++guFailures;std::printf("FAIL %s\n",label);}
}
int main() {
    // Fresh direct entry, direct reentry, tab entry after HUD pause, race entry.
    // All consumers below are extracted from production, including the original
    // hide that clears the bank. No owner guard substitutes for its semantics.
    for(int scenario=0;scenario<4;++scenario) {
        BrnGui::MapIconManager bank;
        bank.mbIconsVisible=scenario!=1;
        BrnGui::FBurnMainHudState hud{true,{&bank}};
        BrnGui::CrashNavMap map;map.mpIconManager=&bank;
        BrnGui::FixtureModule module{{&hud,scenario!=2},{&map,{},true},{}};
        giOrder=0;module.Tick();
        Check(gaOrder[0]==1&&gaOrder[1]==2&&gaOrder[2]==3,"original HUD/SCREEN/OVERLAY observer order");
        Check(bank.mbIconsVisible,"map enable follows departing minimap hide");
        Check(bank.mOwnerId==2&&bank.miNumUsedIcons==5,"map bank is not cleared by a later HUD hide");
        giOrder=0;module.Tick();
        Check(bank.mbIconsVisible&&bank.miNumUsedIcons==5,"render-only following frame retains map visibility");
    }
    std::printf("PlaytestMapFlowOrder: %u checks, %u failures\n",guChecks,guFailures);
    return guFailures?1:0;
}
