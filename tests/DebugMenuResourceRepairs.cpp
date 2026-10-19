#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"
#include "pc/math/VmxFloat.h"
#include <cmath>
#include <cstdio>
#include <vector>

// Whole production methods; these boundary spies capture draw arguments only.
// They establish no renderer, serialized-pointer, lifecycle or native UI verdict.
namespace CgsDev
{
    struct Draw { char kind; f32 x, y, width, height, scale; RGBA colour; };
    struct Debug2DImmediateRender
    {
        std::vector<Draw> draws;
        void DrawBox(Vector2 p, Vector2 s, RGBA c) { draws.push_back({'B',p.x,p.y,s.x,s.y,0,c}); }
        void DrawBox(f32 x, f32 y, f32 r, f32 b, RGBA c) { DrawBox({x,y,0,0},{r-x,b-y,0,0},c); }
        void DrawFrame(f32 x, f32 y, f32 r, f32 b, RGBA c, f32 s) { draws.push_back({'F',x,y,r-x,b-y,s,c}); }
        void DrawText(const char*, f32 x, f32 y, f32 s, RGBA c) { draws.push_back({'T',x,y,0,0,s,c}); }
    };
    namespace DebugUI
    {
        struct Window
        {
            enum { KX_FLAGDISABLED=1, KX_FLAGNOCAPTION=2, KX_FLAGNOBORDER=4,
                   KX_FLAGNOBACKGROUND=8, KX_FLAGPINNED=32 };
            s32 mxFlags=0;
            f32 mfX=100, mfY=200, mfWidth=64, mfHeight=32, mfTransparency=0.5f;
            const char* mpcCaption="caption";
            bool active=false;
            Palette palette{};
            Metrics metrics{};
            const Palette& GetPalette() const { return palette; }
            const Metrics& GetMetrics() const { return metrics; }
            bool IsActiveWindow() const { return active; }
            bool IsEnabled() const { return (mxFlags & KX_FLAGDISABLED)==0; }
            bool IsPinned() const { return (mxFlags & KX_FLAGPINNED)!=0; }
            RGBA ScaleColour(RGBA) const;
            f32 CalcCaptionHeight();
            void Render(Debug2DImmediateRender*);
        };
    }
}
namespace renderengine { struct Texture {}; }
namespace CgsResource
{
    struct DebugPoolTextures
    {
        struct TextureEntry { renderengine::Texture* mpTexture; };
        struct Call { bool stretched; renderengine::Texture* texture; Vector2 position, size; bool first, second; };
        TextureEntry* mpTextureEntries=nullptr;
        s32 miTextureIndex=0, miTotalTextures=0;
        std::vector<Call> calls;
        void RenderTexture(renderengine::Texture* t, Vector2 p, Vector2 s, bool a, bool b)
        { calls.push_back({false,t,p,s,a,b}); }
        void RenderTextureStretched(renderengine::Texture* t, Vector2 p, Vector2 s, bool a, bool b)
        { calls.push_back({true,t,p,s,a,b}); }
        void RenderThumbnails(CgsDev::Debug2DImmediateRender*,Vector2,Vector2,s32);
    };
}
#include "ResourceRepairs.inc"
namespace
{
    int checks=0, failures=0;
    void Check(bool ok, const char* label)
    { ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n",label); } }
}
int main()
{
    using namespace CgsDev;
    using namespace CgsDev::DebugUI;
    Window w;
    w.metrics.mfTextSize=10;
    w.metrics.mfWindowBorderSize=2;
    w.palette.mColourWindow=0x80445566u;
    w.palette.mColourBorder=0xC0112233u;
    w.palette.mColourCaption=0xA0223344u;
    w.palette.mColourCaptionText=0xD0334455u;
    w.palette.mColourActiveCaption=0xE0445566u;
    w.palette.mColourDisabled=0xB0556677u;
    w.palette.mColourDisabledText=0xF0667788u;
    w.palette.mColourPinned=0x90778899u;
    w.palette.mColourPinnedText=0xA08899AAu;
    Debug2DImmediateRender r;
    w.Render(&r);
    Check(r.draws.size()==4,"caption window emits background/frame/caption/text");
    Check(r.draws[0].kind=='B' && r.draws[1].kind=='F' && r.draws[2].kind=='B' && r.draws[3].kind=='T',"original draw order");
    Check(r.draws[0].colour==0x39445566u,"inactive client background opacity");
    Check(r.draws[1].colour==0xC0112233u,"frame uses raw palette word");
    Check(r.draws[2].colour==0x48223344u,"inactive caption opacity");
    Check(r.draws[3].colour==0xD0334455u,"caption text uses raw palette word");
    Check(r.draws[3].x==100 && r.draws[3].y==188 && r.draws[3].scale==10,"caption text x and strip placement");
    Check(r.draws[1].x==100 && r.draws[1].y==186 && r.draws[1].width==64 && r.draws[1].height==46,"caption frame extent");
    r.draws.clear();
    w.mxFlags=Window::KX_FLAGDISABLED;
    w.Render(&r);
    Check(r.draws[1].colour==0xF0667788u,"disabled frame uses disabled-text palette");
    Check(r.draws[3].colour==0xF0667788u,"disabled text remains raw");
    Check(r.draws[0].colour==0x0E445566u,"disabled background applies quarter opacity");
    r.draws.clear();
    w.mxFlags=Window::KX_FLAGPINNED;
    w.active=true;
    w.Render(&r);
    Check(r.draws[0].colour==0x48778899u && r.draws[2].colour==0x48778899u,"pinned caption and background use pinned colour");
    Check(r.draws[1].colour==0xC0112233u && r.draws[3].colour==0xA08899AAu,"pinned frame/text remain raw");
    r.draws.clear();
    w.mxFlags=Window::KX_FLAGNOCAPTION;
    w.Render(&r);
    Check(r.draws.size()==2 && r.draws[0].kind=='B' && r.draws[1].kind=='F',"no-caption branch emits no strip or text");
    Check(r.draws[1].y==200 && r.draws[1].height==32 && r.draws[1].colour==0xC0112233u,"no-caption frame rectangle and raw colour");
    r.draws.clear();
    w.mxFlags=Window::KX_FLAGNOBACKGROUND|Window::KX_FLAGNOBORDER;
    w.Render(&r);
    Check(r.draws.size()==2 && r.draws[0].kind=='B' && r.draws[1].kind=='T',"flags suppress client background/frame only");
    w.mxFlags=0; w.active=true; w.mfTransparency=-1.0f;
    Check(w.ScaleColour(0xC0556677u)==0x80556677u,"alpha conversion retains low byte without saturation");

    CgsResource::DebugPoolTextures thumbnails;
    renderengine::Texture textures[70];
    CgsResource::DebugPoolTextures::TextureEntry entries[70];
    for (int i=0;i<70;++i) entries[i].mpTexture=&textures[i];
    thumbnails.mpTextureEntries=entries;
    for (s32 grid : {8,16,32})
    {
        thumbnails.miTextureIndex=0; thumbnails.miTotalTextures=1;
        thumbnails.calls.clear(); r.draws.clear();
        thumbnails.RenderThumbnails(&r,{10,20,0,0},{264,264,0,0},grid);
        Check(thumbnails.calls.size()==3,"one texture enclosed by original frame calls");
        const auto& frame=thumbnails.calls[0];
        const auto& cell=thumbnails.calls[1];
        const auto& close=thumbnails.calls[2];
        Check(!frame.stretched && !frame.texture && frame.first && !frame.second && frame.position.x==14 && frame.position.y==24 && frame.size.x==256,"outer origin four and extent eight");
        Check(cell.stretched && cell.texture==&textures[0] && cell.position.x==15.5f && cell.position.y==25.5f,"cell has three-unit gap centered in stride");
        Check(cell.size.x==(256.0f/grid)-3.0f && cell.size.y==cell.size.x,"original three-unit cell gap");
        Check(!close.stretched && !close.first && close.second,"closing frame flags");
    }
    thumbnails.miTextureIndex=66; thumbnails.miTotalTextures=70;
    thumbnails.calls.clear(); r.draws.clear();
    thumbnails.RenderThumbnails(&r,{10,20,0,0},{264,264,0,0},8);
    Check(thumbnails.calls.size()==8 && thumbnails.calls[1].texture==&textures[64] && thumbnails.calls[6].texture==&textures[69],"partial second page stops at live texture count");
    Check(r.draws.size()==1 && r.draws[0].x==78 && r.draws[0].y==24 && r.draws[0].width==32 && r.draws[0].colour==0xFF0000FFu,"selection highlights full page-relative stride");
    thumbnails.miTextureIndex=0; thumbnails.miTotalTextures=0;
    thumbnails.calls.clear(); r.draws.clear();
    thumbnails.RenderThumbnails(&r,{10,20,0,0},{264,264,0,0},8);
    Check(thumbnails.calls.size()==2 && r.draws.empty(),"empty texture set still emits two frame calls");
    std::printf("ResourceRepairs: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
