// Production banking/projection bodies extracted by run_playtest_above_car_banking.py.
// Fixed gold comes from the actual ARTIST8245BF28 and82222060 instruction words.
// Models only external rendering, font, localisation, camera-update and Array calls,
// the same boundaries recorded by the independent original-opcode oracle.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <stdexcept>

std::vector<std::string> gEvents;
std::vector<std::string> gRenders;
std::vector<f32> gTransform;
f32 gWidth;
std::string S(const char* value)
{
    std::string out="\"";
    for (;*value;++value) { if (*value=='"'||*value=='\\') out+='\\'; out+=*value; }
    return out+'"';
}
std::string N(f32 value) { char buf[64]; std::snprintf(buf,sizeof(buf),"%.9g",value); return buf; }
std::string I(s32 value) { return std::to_string(value); }
std::string U(u32 value) { return std::to_string(value); }
void Event(const std::string& value) { gEvents.push_back(value); }
std::string List(const std::vector<std::string>& values)
{
    std::string out="[";
    for(size_t i=0;i<values.size();++i) { if(i)out+=',';out+=values[i]; }
    return out+']';
}
std::string Pair(f32 x,f32 y) { return '['+N(x)+','+N(y)+']'; }
std::string Vec(f32 x,f32 y,f32 z,f32 w) { return '['+N(x)+','+N(y)+','+N(z)+','+N(w)+']'; }
#undef CGS_ASSERT
#define CGS_ASSERT(cond,msg) do { if(!(cond))throw std::runtime_error(msg); } while(0)

namespace renderengine { struct BlendState {}; struct RasterizerState {}; struct Texture {}; }
namespace CgsResource
{
using CgsUtf8=u8;
struct Font { f32 GetStringWidth(const u8* text) { Event("[\"width\","+S(reinterpret_cast<const char*>(text))+']');return gWidth; } };
struct Handle
{
    u32 hi=0,lo=0;
    Font* operator->() const { static Font font;return &font; }
};
}
std::string FontBytes(const CgsResource::Handle& h)
{
    std::vector<std::string> bytes;
    for(u32 word:{h.hi,h.lo})for(int shift:{24,16,8,0})bytes.push_back(U((word>>shift)&255));
    return List(bytes);
}
namespace CgsGraphics
{
struct TextObject
{
    enum { E_ALIGNMENT_CENTER=2 };
    CgsResource::Handle mpFont;
    f32 mfFontHeight=0;
    bool mbDropShadow=false;
    u32 mTextColour=0;
    struct Position { f32 mX,mY; } mv2TopLeft{},mv2BottomRight{};
    s32 meAlignment=0,mbMultiLine=0;
    bool mbAutosize=false;
    const u8* mpUtf8String=nullptr;
    f32 mfStringWidth=0;
    void CalculateAutosizing() { Event("[\"autosize\","+N(mfStringWidth)+']'); }
};
struct Im2dRenderBuffer
{
    void BeginRendering() { Event("[\"begin\"]"); }
    void EndRendering() { Event("[\"end\"]"); }
    void SetState(const renderengine::RasterizerState* state) { Event("[\"cull\","+U(static_cast<u32>(reinterpret_cast<uintptr_t>(state)))+']'); }
    void SetState(const renderengine::BlendState* state) { Event("[\"blend\","+U(static_cast<u32>(reinterpret_cast<uintptr_t>(state)))+']'); }
    void SetTexture(renderengine::Texture* texture) { Event("[\"texture\","+U(static_cast<u32>(reinterpret_cast<uintptr_t>(texture)))+']'); }
    void SetTransform(const Im2dTransform& transform)
    {
        const f32* p=reinterpret_cast<const f32*>(&transform);
        gTransform.assign(p,p+16);Event("[\"transform\"]");
    }
};
struct Camera
{
    f32 maProjectionScalars[9]={};
    Matrix44 mViewProjection;
    void SetNearClipPlane(f32 v) { maProjectionScalars[7]=v;Event("[\"near\","+N(v)+']'); }
    const Matrix44& GetViewProjectionMatrix() const { return mViewProjection; }
};
struct TextRenderer
{
    void RenderString(Im2dRenderBuffer*,const TextObject& text)
    {
        std::string item="{\"screen\":"+Pair(text.mv2TopLeft.mX,text.mv2TopLeft.mY)+
            ",\"colour\":"+U(text.mTextColour)+",\"multiline\":"+I(text.mbMultiLine)+
            ",\"string\":"+S(reinterpret_cast<const char*>(text.mpUtf8String))+
            ",\"font\":"+FontBytes(text.mpFont)+",\"height\":"+N(text.mfFontHeight)+
            ",\"bottom\":"+Pair(text.mv2BottomRight.mX,text.mv2BottomRight.mY)+
            ",\"alignment\":"+I(text.meAlignment)+",\"shadow\":"+I(text.mbDropShadow)+
            ",\"width\":"+N(text.mfStringWidth)+'}';
        gRenders.push_back(item);Event("[\"render\"]");
    }
};
}
namespace CgsGui
{
const renderengine::RasterizerState* gpGuiRasterizerStateCullNone=reinterpret_cast<const renderengine::RasterizerState*>(0x55555555);
renderengine::Texture* gpGuiWhiteTexture=reinterpret_cast<renderengine::Texture*>(0x66666666);
struct ImRendererSet { CgsGraphics::Im2dRenderBuffer* mpIm2dRenderBuffer; CgsGraphics::Camera mCamera; };
}
namespace CgsUnicode
{
using CgsUtf8=u8;
struct UnicodeBuffer
{
    u8 maBuffer[256];
    void Convert(const u8* value) { Event("[\"convert\","+S(reinterpret_cast<const char*>(value))+']');std::strcpy(reinterpret_cast<char*>(maBuffer),reinterpret_cast<const char*>(value)); }
    const u8* GetBuffer() const { return maBuffer; }
};
void _Print(u8* target,const u8* format,s32 size,const u8*const* args,u8 count)
{
    std::string result=reinterpret_cast<const char*>(format);
    std::vector<std::string> recorded;
    for(u8 i=0;i<count;++i)
    {
        const std::string value=reinterpret_cast<const char*>(args[i]);recorded.push_back(S(value.c_str()));
        const std::string key="%"+std::to_string(i+1);
        size_t at=result.find(key);if(at!=std::string::npos)result.replace(at,key.size(),value);
    }
    Event("[\"print\","+I(size)+','+List(recorded)+']');
    std::snprintf(reinterpret_cast<char*>(target),size,"%s",result.c_str());
}
}
namespace CgsCore
{
s32 SnPrintf(char* target,s32 size,const char* format,...)
{
    va_list args;va_start(args,format);s32 value=va_arg(args,s32);va_end(args);
    Event("[\"printf\","+I(value)+','+I(size)+']');return std::snprintf(target,size,format,value);
}
}
struct Language
{
    s32 language=0;
    s32 GetCurrentLanguage() const { return language; }
    void FormatCurrencyString(char* target,s32 value,s32 size) { Event("[\"currency\","+I(value)+','+I(size)+']');std::snprintf(target,size,"$%d",value); }
    const u8* FindString(const char* key) { Event("[\"find\","+S(key)+']');return reinterpret_cast<const u8*>(std::strcmp(key,"SHOWTIME_COMBO")==0?"combo: %1 money: %2":"time: %1"); }
};
struct Cache { s32 mode=0; s32 GetGameMode() const { return mode; } };
template<typename T,u32 Capacity> struct Array
{
    static constexpr s32 KI_UNCONSTRUCTED=-1;
    T elements[Capacity];u32 count=0;
    void Clear() { count=0; }
    s32 GetCount() const { return count; }
    u32 GetLength() const { return count; }
    T& GetItem(u32 i) { CGS_ASSERT(i<count,"index");return elements[i]; }
    void EraseFast(u32 i) { elements[i]=elements[count-1];--count;Event("[\"erase\","+U(i)+']'); }
};
namespace BrnDirector { namespace Camera { namespace Utils {
bool ProjectWorldSpacePointToScreen(const Matrix44&,Vector3,Vector2&);
}}}
#include "playtest_above_car_projection.inc"
namespace BrnGui
{
f32 KF_SCORE_BANKING_SHOWTIME_TARGET_X=0;
struct BankingScore { Vector2 mv2ScreenSpacePosition;Vector3 mv3OriginalWorldSpacePosition;s16 miBaseScore,miComboBonus;bool mbIsRoadRageTimeExtension; };
class AboveCarRenderer
{
public:
    void RenderBankingScores(CgsGui::ImRendererSet*);
    Cache* mpGuiCache;
    CgsGraphics::TextObject mTextObject;
    CgsResource::Handle mpScoreFont;
    Array<BankingScore,6> maBankingScores;
    Language* mpLanguageManager;
    void* mpBlendState=reinterpret_cast<void*>(0x12345678);
    CgsGraphics::TextRenderer* mpTextRenderer;
    bool mbTimeExtensionPending=true;
};
#include "playtest_above_car_banking.inc"
}
struct Case
{
    const char* name;s32 mode,language;bool autosize,pending;f32 nearPlane,targetX,width;
    Matrix44 matrix;u32 count;BrnGui::BankingScore scores[6];
};
#include "playtest_above_car_cases.inc"
int main()
{
    std::vector<std::string> results;
    for(const Case& input:K_CASES)
    {
        gEvents.clear();gRenders.clear();gTransform.clear();gWidth=input.width;
        BrnGui::KF_SCORE_BANKING_SHOWTIME_TARGET_X=input.targetX;
        Cache cache;cache.mode=input.mode;Language language;language.language=input.language;
        CgsGraphics::Im2dRenderBuffer buffer;CgsGraphics::TextRenderer textRenderer;
        CgsGui::ImRendererSet set;set.mpIm2dRenderBuffer=&buffer;
        set.mCamera.maProjectionScalars[7]=input.nearPlane;set.mCamera.mViewProjection=input.matrix;
        BrnGui::AboveCarRenderer renderer;
        renderer.mpGuiCache=&cache;renderer.mpLanguageManager=&language;renderer.mpTextRenderer=&textRenderer;
        renderer.mTextObject.mpFont={0x11111111,0x22222222};renderer.mpScoreFont={0x33333333,0x44444444};
        renderer.mTextObject.mbAutosize=input.autosize;renderer.mbTimeExtensionPending=input.pending;
        renderer.maBankingScores.count=input.count;
        for(u32 i=0;i<input.count;++i)renderer.maBankingScores.elements[i]=input.scores[i];
        renderer.RenderBankingScores(&set);
        std::vector<std::string> transform,scores;
        for(f32 lane:gTransform)transform.push_back(N(lane));
        for(u32 i=0;i<renderer.maBankingScores.count;++i)
        {
            const auto& s=renderer.maBankingScores.elements[i];
            scores.push_back("{\"screen\":"+Vec(s.mv2ScreenSpacePosition.x,s.mv2ScreenSpacePosition.y,s.mv2ScreenSpacePosition.z,s.mv2ScreenSpacePosition.w)+
                ",\"world\":"+Vec(s.mv3OriginalWorldSpacePosition.x,s.mv3OriginalWorldSpacePosition.y,s.mv3OriginalWorldSpacePosition.z,s.mv3OriginalWorldSpacePosition.w)+
                ",\"base\":"+I(s.miBaseScore)+",\"bonus\":"+I(s.miComboBonus)+",\"road\":"+I(s.mbIsRoadRageTimeExtension)+'}');
        }
        results.push_back("{\"events\":"+List(gEvents)+",\"transform\":"+List(transform)+",\"renders\":"+List(gRenders)+",\"scores\":"+List(scores)+
            ",\"font\":"+FontBytes(renderer.mTextObject.mpFont)+",\"shadow\":"+I(renderer.mTextObject.mbDropShadow)+
            ",\"pending\":"+I(renderer.mbTimeExtensionPending)+",\"near\":"+N(set.mCamera.maProjectionScalars[7])+",\"asserts\":0}");
    }
    std::puts(List(results).c_str());
}
