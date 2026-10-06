#include "BrnCommonTypes.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameSource/Gui/SatNav/BrnSatNavIcon.h"
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cmath>
#include <initializer_list>
static unsigned guChecks,guFailures;
#undef CGS_ASSERT
#define CGS_ASSERT(test,msg) do {if(!(test)) ++guFailures;}while(0)
static CgsID gConvertedId;
void CgsIDConvertToString(CgsID id,char* p) {gConvertedId=id;std::strcpy(p,"EXAMPLE");}
namespace CgsCore {
int SPrintf(char* p,s32 n,const char* fmt,...) {
    va_list args;va_start(args,fmt);const int r=std::vsnprintf(p,n,fmt,args);va_end(args);return r;
}
}
#include "playtest_full_map_rivals_deps.inc"
namespace BrnGui {
struct FixtureCache {
    BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData maPlayers[8];
    s32 miMode,miTeam,miColour;
    const auto* GetOnlinePlayerInfo(s32 i) const {return &maPlayers[i];}
    s32 GetCurrentOnlinePlayerTeam(EActiveRaceCarIndex) const {return miTeam;}
    s32 GetGameMode() const {return miMode;}
    s32 GetOnlinePlayerColourFromARCI(EActiveRaceCarIndex) const {return miColour;}
};
struct FixtureDrawIcon {
    char macText[128];bool mbLocalised;f32 mfRotation;
    void SetIconText(const char* p,bool b) {std::strcpy(macText,p);mbLocalised=b;}
    void SetRotation(f32 f) {mfRotation=f;}
};
class MapIconManager {
public:
    using SatNavIconInfo=GuiEventUpdateSatNav::SatNavIconInfo;
    enum {E_ICONSIZE_SMALL=0};
    FixtureCache* mpGuiCache;
    MapIconBrnBase::IconState GetCrashNavIconStateForRival(SatNavIconInfo*);
    s32 CallerCrashNav(SatNavIconInfo&,FixtureDrawIcon&);
    s32 CallerSatNav(SatNavIconInfo&,FixtureDrawIcon&);
};
#include "playtest_full_map_rivals_bodies.inc"
}
static void Check(bool b,const char* s) {++guChecks;if(!b){++guFailures;std::printf("FAIL %s\n",s);}}
int main() {
    BrnGui::MapIconManager manager={};
    BrnGui::MapIconManager::SatNavIconInfo info={};
    info.SetIconType(BrnGui::MapIconManager::SatNavIconInfo::E_SATNAVICON_RIVAL);
    for(int mode:{-1,0,3,10,12,15,17}) {
        (void)mode;
        Check(manager.GetCrashNavIconStateForRival(&info)==14,"offline rival remains state14 without cache access");
    }
    BrnGui::FixtureCache cache={};manager.mpGuiCache=&cache;
    info.SetIconType(BrnGui::MapIconManager::SatNavIconInfo::E_SATNAVICON_NETWORKRIVAL);
    info.SetActiveRaceCarIndex(E_ACTIVE_RACE_CAR_INDEX_7);
    Check(manager.GetCrashNavIconStateForRival(&info)==0,"unknown network car is invisible");
    cache.maPlayers[7].meActiveRaceCarIndex=E_ACTIVE_RACE_CAR_INDEX_7;
    Check(manager.GetCrashNavIconStateForRival(&info)==0,"network car outside local world is invisible");
    cache.maPlayers[7].mbIsInLocalGameWorld=true;
    // Golden truth table from ARTIST824F47F0..824F487C, not the minimap's
    // wider colour-mode set. Team gates precede marked-man/lobby gates.
    const int modes[8]={-1,0,3,10,12,14,15,17};
    for(int mode:modes) for(int team=0;team<3;++team) for(int marked=0;marked<2;++marked)
        for(int colour=0;colour<12;++colour) {
            cache.miMode=mode;cache.miTeam=team;cache.miColour=colour;
            cache.maPlayers[7].mbMarkedMan=marked!=0;
            const int golden=team==2?17:team==1?16:(!marked&&(mode==10||mode==15)?14+colour:14);
            Check(manager.GetCrashNavIconStateForRival(&info)==golden,"network team/marked-man/lobby-colour state matches ARTIST");
        }
    info.SetIconType(BrnGui::MapIconManager::SatNavIconInfo::E_SATNAVICON_RIVAL);
    info.SetCgsId(0x12345678cafebeefull);info.SetRotation(0.75f);
    BrnGui::FixtureDrawIcon icon={};
    for(int caller=0;caller<2;++caller) {
        const s32 state=caller==0?manager.CallerCrashNav(info,icon):manager.CallerSatNav(info,icon);
        Check(state==14,"both LARGE-map callers publish real rival state");
        Check(gConvertedId==0x12345678cafebeefull,"both callers convert all64bits of car ID");
        Check(std::strcmp(icon.macText,"CAR_EXAMPLE")==0&&icon.mbLocalised,"both callers publish original localized CAR key");
        Check(std::fabs(icon.mfRotation-(3.1415927f-0.75f))<0.000001f,"both callers preserve original rotation");
    }
    std::printf("PlaytestFullMapRivals: %u checks, %u failures\n",guChecks,guFailures);
    return guFailures?1:0;
}
