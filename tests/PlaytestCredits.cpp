#include "GameSource/Gui/CustomRenderer/Renderers/BrnCreditsTextRenderer.h"
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm2dRenderBuffer.h"
#include "pc/gcm/renderengine/renderstates.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdarg>

static int checks, failures, assertions, draws, begins, ends;
struct Draw { const unsigned char* text; u32 colour; float height, top, bottom; } draw[8];
static CgsGraphics::Im2dTransform transform;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char*, int) { ++assertions; std::printf("ASSERT %s\n",message); return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsDev {
DebugComponent::DebugComponent() {}
void DebugComponent::Update() {}
void DebugComponent::RenderWorld(Debug3DImmediateRender*) {}
void DebugComponent::RenderHUD(Debug2DImmediateRender*) {}
const char* DebugComponent::GetName() const { return ""; }
const char* DebugComponent::GetPath() const { return ""; }
bool DebugComponent::IsSimple() const { return true; }
void DebugComponent::OnActivate() {}
void DebugComponent::OnRegister() {}
}
namespace CgsCore {
void SPrintf(char* out,u32 size,const char* fmt,...) { va_list args;va_start(args,fmt);vsnprintf(out,size,fmt,args);va_end(args); }
}
namespace CgsGui {
renderengine::Texture* CustomRenderComponentInterface::GetRenderOutput(s32,s32*,ImRendererSet*) { return nullptr; }
}
namespace CgsLanguage {
void LanguageManagerDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender*) {}
const char* LanguageManagerDebugComponent::GetName() const { return ""; }
LanguageManager::LanguageManager() {}
const char* LanguageManager::GetDefaultFont() const { return "default"; }
const u8* LanguageManager::FindString(const char* key) const {
    if(!std::strcmp(key,"CREDITS_TITLE_0")) return reinterpret_cast<const u8*>("Criterion");
    if(!std::strcmp(key,"CREDITS_DETAIL_0")) return reinterpret_cast<const u8*>("Alice\nBob");
    if(!std::strcmp(key,"REPLAY_CREDITS_DETAIL_1")) return reinterpret_cast<const u8*>("Replay");
    return nullptr;
}
}
namespace renderengine {
void TextureState::GetResourceDescriptor(u32* desc) { for(int i=0;i<5;++i) {desc[i*2]=0;desc[i*2+1]=1;} }
TextureState* TextureState::Initialize(rw::Resource*,const Parameters*) { return nullptr; }
}
namespace CgsGraphics {
// Font line measurement, GPU submission and identity aspect correction are the
// fixture boundaries. The renderer, notifications and TextObject defaults are real.
void TextObject::CalculateAutosizing() {}
u32 TextObject::GetNumLinesAndStartLine(u32,const CgsResource::CgsUtf8** line) const {
    *line=mpUtf8String; u32 n=2;
    for(auto p=mpUtf8String; *p; ++p) if(*p=='\n') ++n;
    return n;
}
void TextRenderer::RenderStringFadingY(Im2dRenderBuffer*,const TextObject& text,f32,f32 top,f32 bottom,f32) {
    if(draws<8) draw[draws]={text.mpUtf8String,text.mTextColour,text.mfFontHeight,top,bottom};
    ++draws;
}
void Im2dTransform::TransformByAspectRatio() {}
void Im2dRenderBuffer::Dispatch(Im2d*) const {}
void Im2dRenderBuffer::SetTransform(const Im2dTransform& value) { transform=value; }
template<> void ImRenderBuffer<Basic2dColouredTexturedVertex>::BeginRendering() { ++begins; }
template<> void ImRenderBuffer<Basic2dColouredTexturedVertex>::EndRendering() { ++ends; }
}
#include "playtest_credits_textobject.inc"

static void Check(bool ok,const char* message) { ++checks;if(!ok){++failures;std::printf("FAIL %s\n",message);} }
static bool Near(float a,float b,float epsilon=1e-5f) { return std::fabs(a-b)<=epsilon; }
int main() {
    BrnGui::CreditsTextRenderer credits{};
    CgsLanguage::LanguageManager language;
    CgsGraphics::TextRenderer renderer{};
    credits.Construct();credits.SetTextRenderer(&renderer);credits.SetLanguageManager(&language);
    credits.Prepare(nullptr,nullptr,nullptr);
    Check(!credits.GetRenderEnabled(),"constructed disabled");
    Check(credits.GetNumTextures()==1,"original custom-renderer texture count");
    CgsResource::Font normal{},title{};
    std::strcpy(normal.macTypefaceFamilyName,"default");
    std::strcpy(title.macTypefaceFamilyName,"machinestd-bold");
    CgsResource::Font* fonts[]={&normal,&title};
    CgsGui::GuiEventLoadNotification notification{};
    notification.meRequestType=static_cast<CgsGui::ResourceRequestTypes>(16);
    notification.mResourceHandle.mpResourceMemory=&fonts[0];
    credits.RecvEvent(&notification,14);
    notification.mResourceHandle.mpResourceMemory=&fonts[1];credits.RecvEvent(&notification,14);
    Check(credits.mpNormalFont.mpResourceMemory==&fonts[0],"default font delivered");
    Check(credits.mpTitleFont.mpResourceMemory==&fonts[1],"title font delivered");
    credits.SetRenderEnabled(true);
    Check(credits.miNumStrings==2,"localized title and names both present");
    Check(Near(credits.mTitleTextObject.mfFontHeight,37)&&Near(credits.mNormalTextObject.mfFontHeight,25),"original font heights");
    Check(Near(credits.maParagraphs[1].mfPosition,32)&&Near(credits.maParagraphs[1].mfHeight,50),"paragraph spacing and multiline height");
    Check(credits.mfScroll==-400 && credits.mfFade==-2,"original scroll and fade entry");
    credits.Update();
    Check(Near(credits.mfScroll,-400+62.0f/60)&&Near(credits.mfFade,-2+1.0f/60),"60Hz scroll/fade step");
    // Independent values from ARTIST Update VMX evaluation with identity aspect.
    Check(Near(credits.mScreenTransform.mOriginXYZ.x,-0.776509881f)&&Near(credits.mScreenTransform.mOriginXYZ.y,0.441477329f),"original rotated origin");
    Check(Near(credits.mScreenTransform.mRightUp.x,0.00155051879f,1e-8f)&&Near(credits.mScreenTransform.mRightUp.y,0.000343337451f,1e-8f)&&
          Near(credits.mScreenTransform.mRightUp.z,0.000193127315f,1e-8f)&&Near(credits.mScreenTransform.mRightUp.w,-0.00275647780f,1e-8f),"original rotated basis");
    CgsGraphics::Im2dRenderBuffer buffer;
    CgsGui::ImRendererSet set{};set.mpIm2dRenderBuffer=&buffer;
    credits.RenderComponent(&set);Check(draws==0,"negative fade delays visible credits");
    credits.mfFade=1;credits.mfScroll=0;credits.RenderComponent(&set);
    Check(draws==4&&begins==1&&ends==1,"two paragraphs shadowed and drawn in a complete batch");
    Check(draw[0].colour==0xFF000000&&draw[2].colour==0xFFFFFFFF,"black shadow and visible white text");
    Check(!std::strcmp(reinterpret_cast<const char*>(draw[3].text),"Alice\nBob"),"actual localized names submitted");
    Check(draw[2].top==20&&draw[2].bottom==415&&draw[0].top==-100&&draw[0].bottom==535,"original fade bands");
    credits.miNumStrings=500;credits.maParagraphs[499].mfPosition=200;credits.maParagraphs[499].mfHeight=20;
    credits.mfScroll=656;credits.Update();Check(credits.mfScroll==-435,"wrap reads final valid paragraph at capacity");
    credits.SetRenderEnabled(false);float scroll=credits.mfScroll;credits.Update();Check(credits.mfScroll==scroll,"disabled credits do not advance");
    s32 type=1;credits.RecvEvent(reinterpret_cast<const CgsModule::Event*>(&type),587);credits.SetRenderEnabled(true);
    Check(credits.miNumStrings==1&&!std::strcmp(reinterpret_cast<const char*>(credits.maParagraphs[0].mpText),"Replay"),"replay key set skips missing first entry");
    Check(assertions==0,"no assertions");
    std::printf("PlaytestCredits: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
