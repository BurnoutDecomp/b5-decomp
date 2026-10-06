#include "GameSource/GameState/BrnGameActions.h"
#include <cstdio>
#include <cstring>
#include <vector>
static unsigned guChecks,guFailures;
#undef CGS_ASSERT
#define CGS_ASSERT(test,msg) do {if(!(test)){++guFailures;std::printf("ASSERT %s\n",msg);}}while(0)
#include "GameSource/Gui/Events/BrnGuiEventPreRaceMessages.h"
static char gacOverlayName[128];
namespace BrnGui {
struct GuiOverlayWaitFinishRequest {
    CgsID mId;
    s32 GetEventType() const {return 188;}
    void Construct(const char* p) {std::strcpy(gacOverlayName,p);mId=19;}
};
struct FixtureRace {
    s32 GetStartLandmarkIndex() const {return 10;}
    s32 GetFinishLandmarkIndex() const {return 20;}
};
struct GuiCache {
    GuiEventPreRaceMessages mPreRaceData;
    bool mbInEventColouringGate=false;
    s32 miPresetRaces=1,miMode=0;
    FixtureRace mRace;
    void RecEvent(const CgsModule::Event*,s32);
    const PreEventInfo* GetPreEventInfo(s32) const;
    bool GetInEventColouringGate() const {return mbInEventColouringGate;}
    s32 GetNumPresetRaces() const {return miPresetRaces;}
    const FixtureRace* GetPresetRace(s32) const {return &mRace;}
    s32 GetGameMode() const {return miMode;}
    s32 GetEventFinishLandmark() const {return 20;}
};
struct FixtureLandmark {u16 mIndex;u16 GetLandmarkIndexHalf() const {return mIndex;}};
struct MapIconManager {
    using SatNavIconInfo=FixtureLandmark;
    GuiCache* mpGuiCache;
    bool mbIsDisplayingEventInfo=false;
    s8 mi8CurrentEventIndex=0;
    bool IsStartIcon(const SatNavIconInfo*);
    bool IsFinishIcon(const SatNavIconInfo*);
};
}
struct FixtureRecord {s32 id;std::vector<u8> bytes;};
struct FixtureQueue {std::vector<FixtureRecord> records;};
namespace BrnGame {
struct NetworkShowFreeBurnIntroWire279 {u8 maZero[2];s32 GetEventType() const {return 279;}};
template<class T> void PushGuiEvent(const T& event,FixtureQueue* pQueue) {
    const u8* bytes=reinterpret_cast<const u8*>(&event);
    pQueue->records.push_back({event.GetEventType(),std::vector<u8>(bytes,bytes+sizeof(event))});
}
}
#include "playtest_pre_race_map_bodies.inc"
static void Check(bool b,const char* name) {++guChecks;if(!b){++guFailures;std::printf("FAIL %s\n",name);}}
int main() {
    using Message=BrnGui::GuiEventPreRaceMessages;
    Check(sizeof(Message)==1744,"original header-free GUI159 size");
    Check(sizeof(Message::MessageInfo)==580,"original message row size");
    Check(Message{}.GetEventType()==159,"original event id");
    Check(offsetof(Message::MessageInfo,meRelationshipType)==16,"original relationship offset");
    Check(offsetof(Message::MessageInfo,maacMessageParameters)==404,"original parameter lane offset");
    Check(offsetof(Message::MessageInfo,macGamerName)==452,"original gamer-name offset");
    BrnGameState::GameStateModuleIO::StartModeIntroAction intro{};
    const auto* action=reinterpret_cast<const CgsModule::Event*>(&intro);
    FixtureQueue empty;
    Check(BrnGame::TranslateIntro(29,action,&empty),"intro action handled");
    Check(empty.records.size()==1&&empty.records[0].id==159&&empty.records[0].bytes.size()==1744,"zero-car intro still posts complete159");
    Message blank;std::memcpy(&blank,empty.records[0].bytes.data(),sizeof(blank));
    Check(blank.miNumMessages==0,"zero-car intro count0");
    intro.mFlybyData.miNumberOfCars=3;
    for(int i=0;i<3;++i) {
        auto& rival=intro.mFlybyData.mRivalsToShow[i];
        rival.meMessageStyle=static_cast<decltype(rival.meMessageStyle)>(i);
        std::strcpy(rival.mPlayerName.macName,"DRIVER");rival.miNumberOfMessages=2;
        std::strcpy(rival.maacMessageIDs[0],"$RIVAL_A");std::strcpy(rival.maacMessageIDs[1],"$RIVAL_B");
        rival.maiNumberOfParameters[0]=1;rival.maiNumberOfParameters[1]=0;
        std::strcpy(rival.maacMessageParameter[0],"PARAMETER");
    }
    FixtureQueue filled;BrnGame::TranslateIntro(29,action,&filled);
    Message payload;std::memcpy(&payload,filled.records[0].bytes.data(),sizeof(payload));
    Check(payload.miNumMessages==3,"all three fly-by rows posted");
    const int styles[3]={0,2,1};
    for(int i=0;i<3;++i) {
        const auto& row=payload.mMessages[i];
        Check(row.meRelationshipType==styles[i]&&std::strcmp(row.macGamerName,"DRIVER")==0,"relationship permutation and gamer copied");
        Check(row.miNumMsgIDs==2&&std::strcmp(row.maacMessageIDs[1],"$RIVAL_B")==0,"message ID rows retained");
        Check(row.maiNumParams[0]==1&&row.maiNumParams[1]==0&&std::strcmp(row.maacMessageParameters[0],"PARAMETER")==0,"parameter count and full text retained");
    }
    BrnGui::GuiCache cache;cache.mPreRaceData.Construct();
    cache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&payload),159);
    Check(std::memcmp(&cache.mPreRaceData,&payload,1744)==0,"cache copies the entire original1744-byte record");
    Check(cache.mbInEventColouringGate,"original pre-race gate published");
    Check(cache.GetPreEventInfo(2)==&cache.mPreRaceData.mMessages[2],"typed accessor returns correct580-byte row");
    BrnGui::MapIconManager map{&cache};
    BrnGui::FixtureLandmark start{10},finish{20},other{99};
    Check(map.IsStartIcon(&start),"running event enables start landmark");
    Check(map.IsFinishIcon(&finish),"running event enables finish landmark");
    Check(!map.IsStartIcon(&other)&&!map.IsFinishIcon(&other),"ordinary landmarks remain ordinary");
    cache.miPresetRaces=0;Check(!map.IsStartIcon(&start),"start requires original preset race data");cache.miPresetRaces=1;
    cache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&payload),162);
    Check(cache.mPreRaceData.miNumMessages==0,"162 clears original message count");
    Check(std::memcmp(cache.mPreRaceData.mMessages,payload.mMessages,sizeof(payload.mMessages))==0,"162 retains original message rows");
    Check(cache.mbInEventColouringGate,"162 leaves event colouring gate unchanged");
    cache.mbInEventColouringGate=false;
    Check(!map.IsStartIcon(&start),"without event gate start remains hidden");
    Check(!map.IsFinishIcon(&finish),"without event gate finish remains hidden");
    for(int mode:{15,16}) for(int finished=0;finished<2;++finished) for(int lobby=0;lobby<2;++lobby) {
        intro.meGameMode=static_cast<decltype(intro.meGameMode)>(mode);
        intro.mbFinishedOnlineEvent=finished!=0;intro.mbFinishedOnlineLobbyMode=lobby!=0;
        FixtureQueue q;BrnGame::TranslateIntro(29,action,&q);
        Check(q.records.size()==(lobby?2:3),"online tail emits correct number of records");
        Check(q.records[1].id==188&&std::strcmp(gacOverlayName,"CNOnlEntGame")==0,"online tail ends original overlay wait");
        if(!lobby) Check(q.records[2].id==279&&q.records[2].bytes[0]==finished&&q.records[2].bytes[1]==1,"online intro original flags");
    }
    intro.meGameMode=static_cast<decltype(intro.meGameMode)>(13);
    FixtureQueue ordinaryOnline;BrnGame::TranslateIntro(29,action,&ordinaryOnline);
    Check(ordinaryOnline.records.size()==1,"other online modes post159 without lobby-only tail");
    std::printf("PlaytestPreRaceMap: %u checks, %u failures\n",guChecks,guFailures);return guFailures?1:0;
}
