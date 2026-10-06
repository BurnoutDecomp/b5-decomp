#include "GameSource/Gui/CustomRenderer/Renderers/BrnAboveCarRenderer.h"
#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Replays/Serialisers/BrnReplayGuiModuleSerialiser.h"
#include "GameSource/Replays/BrnReplayGuiModuleStaticLayout.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm3dRenderBuffer.h"
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdarg>

static int checks,failures,assertions,draws,transforms,projections,defaultFontQueries;
struct Draw { char text[32];float height;bool shadow;Matrix44 model; } recorded[64];
static Matrix44 model,viewProjection;
namespace CgsDev { namespace Assert {
int BeginAssert(){return 0;}
int FireAssert(const char* text,const char*,int){++assertions;std::printf("ASSERT %s\n",text);return 0;}
void* EndAssert(){return nullptr;}
} namespace PerfMonCpu { s32 AddMonitor(const char*,s32,s32,double,s32,s32){return 1;} }
DebugComponent::DebugComponent(){}
void DebugComponent::Update(){}
void DebugComponent::RenderWorld(Debug3DImmediateRender*){}
void DebugComponent::RenderHUD(Debug2DImmediateRender*){}
const char* DebugComponent::GetName() const{return "";}
const char* DebugComponent::GetPath() const{return "";}
bool DebugComponent::IsSimple() const{return true;}
void DebugComponent::OnActivate(){}
void DebugComponent::OnRegister(){}
}
namespace CgsCore {
void SnPrintf(char* p,u32 n,const char* fmt,...){va_list a;va_start(a,fmt);vsnprintf(p,n,fmt,a);va_end(a);}
}
namespace CgsGui {
renderengine::Texture* CustomRenderComponentInterface::GetRenderOutput(s32,s32*,ImRendererSet*){return nullptr;}
}
namespace CgsLanguage {
void LanguageManagerDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender*){}
const char* LanguageManagerDebugComponent::GetName() const{return "";}
LanguageManager::LanguageManager(){}
const char* LanguageManager::GetDefaultFont() const{++defaultFontQueries;return "default";}
bool LanguageManager::FormatText(char* p,u32 n,s32 value,ParameterFormatType format){
    if(format!=E_FORMAT_MONEY)++failures;std::snprintf(p,n,"$%d",value);return true;
}
}
namespace CgsResource {
float Font::GetStringWidth(const CgsUtf8* s) const{return float(std::strlen(reinterpret_cast<const char*>(s)));}
}
namespace CgsGraphics {
void TextObject::CalculateAutosizing(){++failures;}
void Camera::UpdatePerspectiveProjectionMatrix(){++projections;}
void TextRenderer::RenderString(Im3dRenderBuffer*,const TextObject& t){
    auto& d=recorded[draws++];std::snprintf(d.text,32,"%s",t.mpUtf8String);
    d.height=t.mfFontHeight;d.shadow=t.mbDropShadow;d.model=model;
}
template<> void ImRenderBuffer<BasicColouredTexturedVertex>::Construct(){}
template<> void Im3dRenderBufferBase<BasicColouredTexturedVertex>::SetTransform(Matrix44::InParam m,Matrix44::InParam vp){model=m;viewProjection=vp;++transforms;}
template<> bool Im3dRenderBufferBase<BasicColouredTexturedVertex>::HandleCommand(const ImCommand*,Im3dBase<BasicColouredTexturedVertex>*) const{return false;}
bool Im3dRenderBuffer::HandleCommand(const ImCommand*,Im3dBase<BasicColouredTexturedVertex>*) const{return false;}
}
namespace BrnGui {
// Only the explicit cash/lifecycle entry points are exercised by this fixture.
void AboveCarRenderer::RenderComponent(CgsGui::ImRendererSet*){++failures;}
}
#include "playtest_above_car.inc"

static void Check(bool ok,const char* text){++checks;if(!ok){++failures;std::printf("FAIL %s\n",text);}}
static bool Near(float a,float b,float tolerance=2e-5f){return std::fabs(a-b)<=tolerance;}
static void ResetDraws(){draws=transforms=projections=0;}
int main(){
    BrnGui::AboveCarRenderer above;above.Construct();
    above.Update(); // The original accepts the pre-bind/null-serialiser interval.
    CgsLanguage::LanguageManager language;language.meLanguage=static_cast<decltype(language.meLanguage)>(0);
    CgsGraphics::TextRenderer text{};CgsGraphics::Im3dRenderBuffer buffer;
    CgsGui::ImRendererSet set{};set.mpIm3dRenderBufferRacePosition=&buffer;
    set.mCamera.mView.SetIdentity();set.mCamera.mViewProjection.SetIdentity();set.mCamera.maProjectionScalars[7]=0.25f;
    above.SetTextRenderer(&text);above.SetLanguageManager(&language);
    Check(above.maBankingScores.GetLength()==0,"banking array is constructed empty");
    Check(above.GetID()==0x4DCEFF7CA8C29C00ULL && above.GetNumTextures()==1,"full component id and original count");
    Check(above.Prepare(nullptr,nullptr,nullptr)&&above.Release(),"empty resource lifecycle");
    CgsResource::Font fallback{},marker{},score{};CgsResource::Font* fonts[]={&fallback,&marker,&score};
    std::strcpy(fallback.macTypefaceFamilyName,"default");std::strcpy(marker.macTypefaceFamilyName,"B5EAConDisS");std::strcpy(score.macTypefaceFamilyName,"B5DOTMAT");
    CgsGui::GuiEventLoadNotification load{};load.meRequestType=static_cast<CgsGui::ResourceRequestTypes>(16);
    load.mResourceHandle.mpResourceMemory=&fonts[0];above.RecvEvent(&load,14);
    Check(above.mTextObject.mpFont.mpResourceMemory==&fonts[0]&&above.mpScoreFont.mpResourceMemory==&fonts[0],"independent fallback fonts");
    load.mResourceHandle.mpResourceMemory=&fonts[1];above.RecvEvent(&load,14);
    load.mResourceHandle.mpResourceMemory=&fonts[2];above.RecvEvent(&load,14);
    Check(above.mTextObject.mpFont.mpResourceMemory==&fonts[1]&&above.mpScoreFont.mpResourceMemory==&fonts[2],"preferred marker and score fonts");
    Check(defaultFontQueries==2,"fallback queried only while each font is unset");
    static BrnGui::GuiCache cache;
    BrnGui::GuiCache* pc=&cache;above.RecvEvent(reinterpret_cast<const CgsModule::Event*>(&pc),64);
    BrnGui::GuiTrafficCarInfoEvent traffic;traffic.Construct();
    BrnTraffic::BrnTrafficIO::VehicleScoreData car{};car.mPosition={0,0,-10,0};car.miScore=250;car.miMultiplier=2;car.muVehicleIndex=7;traffic.mScoreTargets.Append(car);
    car.mPosition={5,0,-20,0};car.miScore=100;car.miMultiplier=0;car.muVehicleIndex=599;traffic.mScoreTargets.Append(car);
    cache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&traffic),208);
    BrnGui::GuiOverheadSignInfoEvent signs;signs.mVisibleOverheadSignArray.Construct();cache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&signs),210);
    Check(cache.GetScoringTrafficCount()==2&&cache.GetScoringTrafficData(1)->muVehicleIndex==599,"complete32-byte traffic records reach cache");
    cache.meGameModeType=2;ResetDraws();above.RenderTrafficCarScores(&set);
    Check(draws==3&&transforms==2,"two cars and one additional bonus draw");
    Check(!std::strcmp(recorded[0].text,"$250")&&!std::strcmp(recorded[1].text,"+2")&&!std::strcmp(recorded[2].text,"$100"),"original currency and plus-bonus strings");
    Check(recorded[0].height==0.5f&&recorded[1].height==1&&recorded[0].shadow,"original font sizes and shadow");
    Check(projections==2&&set.mCamera.maProjectionScalars[7]==0.25f&&above.mTextObject.mpFont.mpResourceMemory==&fonts[1]&&!above.mTextObject.mbDropShadow,"projection and text state restored");
    Check(Near(recorded[0].model.xAxis.x,1)&&Near(recorded[0].model.yAxis.y,-1)&&Near(recorded[0].model.zAxis.z,-1)&&Near(recorded[0].model.wAxis.z,-10),"raw-VMX billboard basis faces camera with downward text Y");
    float distance=above.SetTransformMatrixForCar(&set,{3,4,-10,0});
    Check(Near(distance,11.180340767f)&&Near(model.xAxis.x,.957826316f)&&Near(model.xAxis.z,.287347883f)&&Near(model.yAxis.x,.102804706f)&&Near(model.yAxis.y,-.933809459f)&&Near(model.yAxis.z,-.342682362f)&&Near(model.zAxis.x,.268328160f),"off-axis raw-VMX transform oracle");
    BrnGui::GuiHitVehicleEvent hit{};hit.muVehicleIndex=7;hit.miVehicleBaseScore=250;hit.miVehicleChainBonus=50;above.RecvEvent(reinterpret_cast<const CgsModule::Event*>(&hit),394);
    Check(above.maBankingScores.GetLength()==1&&above.maBankingScores.GetItem(0).miBaseScore==250&&above.maBankingScores.GetItem(0).miComboBonus==50,"hit banks score from matching vehicle");
    ResetDraws();above.RenderTrafficCarScores(&set);Check(draws==1&&!std::strcmp(recorded[0].text,"$100"),"recent hit suppresses old cash marker");
    BrnGui::GuiRemovedTrafficEvent removed;removed.mRemovedTrafficArray.Construct();removed.mRemovedTrafficArray.Append(7);above.RecvEvent(reinterpret_cast<const CgsModule::Event*>(&removed),209);
    ResetDraws();above.RenderTrafficCarScores(&set);Check(draws==3,"reused vehicle index regains marker after removal");
    BrnGui::OverheadSignScore sign{};sign.mWorldSpacePosition={0,10,-30,0};signs.mVisibleOverheadSignArray.Append(sign);
    sign.mWorldSpacePosition={2,20,-30,0};signs.mVisibleOverheadSignArray.Append(sign);
    sign.mWorldSpacePosition={30,5,-30,0};signs.mVisibleOverheadSignArray.Append(sign);cache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&signs),210);
    ResetDraws();above.RenderTrafficCarScores(&set);Check(draws==5&&!std::strcmp(recorded[3].text,"$10000")&&Near(recorded[3].model.wAxis.x,1)&&Near(recorded[3].model.wAxis.y,15),"nearby signs group horizontally at mean position");
    u32 extension=20;above.RecvEvent(reinterpret_cast<const CgsModule::Event*>(&extension),427);
    BrnGui::GuiPlayerCrashingStateChangeEvent state{BrnGui::GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_LEAVE_TAKEDOWN};above.RecvEvent(reinterpret_cast<const CgsModule::Event*>(&state),377);
    const auto& bank=above.maBankingScores.GetItem(1);Check(bank.mbIsRoadRageTimeExtension&&bank.miBaseScore==20&&bank.mv2ScreenSpacePosition.y==400&&!above.mbTimeExtensionPending,"time extension waits for takedown exit");
    BrnReplays::GuiModuleStaticLayout layout{};BrnReplays::GuiModuleSerialiser serialiser{};serialiser.mpStaticBuffer=&layout;serialiser.miStaticBufferSize=sizeof(layout);serialiser.meMode=BrnReplays::BaseSerialiser::E_MODE_RECORDING;above.SetReplaySerialiser(&serialiser);
    above.maAboveCarObjectLayouts[7].mWorldPosition={7,8,9,0};above.maAboveCarObjectLayouts[7].mColour=0x12345678;above.maAboveCarObjectLayouts[7].muRacePosition=8;above.maAboveCarObjectLayouts[7].mbVisible=true;above.Update();
    Check(!std::memcmp(layout.maCarRecordsA,&above.maAboveCarObjectLayouts,sizeof(above.maAboveCarObjectLayouts)),"all eight complete replay records copied");
    auto& last=above.maAboveCarObjectLayouts[7];std::memset(&last,0xA5,sizeof(last));last.Clear();
    Check(last.mColour==0xA5A5A5A5&&!last.mbVisible&&!last.muRacePosition&&last.mWorldPosition.x==0&&reinterpret_cast<u8*>(&last)[25]==0xA5,"clear preserves colour and padding and clears one visibility byte");
    serialiser.meMode=BrnReplays::BaseSerialiser::E_MODE_PLAYING;above.Update();Check(last.mbVisible&&last.muRacePosition==8&&last.mWorldPosition.z==9,"last replay record restored without stride drift");
    cache.meGameModeType=0;above.mRecentCrashSet.SetBit(7);ResetDraws();above.RenderTrafficCarScores(&set);Check(draws==0&&!above.mRecentCrashSet.IsBitSet(7),"leaving Showtime clears recent-hit set");
    Check(assertions==0,"no unexpected assertions");
    std::printf("PlaytestAboveCar: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
