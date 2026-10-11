#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuItem.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm3dRenderBuffer.h"

namespace {
unsigned suChecks, suFailures;
void Check(bool lbGood, const char* lpcName)
{
    ++suChecks;
    if (!lbGood) { ++suFailures; std::printf("FAIL %s\n", lpcName); }
}
struct Submission {
    s32 miTopology;
    std::vector<CgsGraphics::Basic2dColouredTexturedVertex> mVertices;
};
std::vector<Submission> saSubmissions;
u32 sauPixels[80][100];
unsigned suTextCalls;
f32 sfTextX, sfTextY, sfTextScale;
u32 suTextColour;
const char* spText;
CgsDev::DebugUI::Metrics sMetrics = {};
CgsDev::DebugUI::Palette sPalette = {};

float Edge(const CgsGraphics::Vector2& a, const CgsGraphics::Vector2& b, float x, float y)
{ return (b.x-a.x)*(y-a.y) - (b.y-a.y)*(x-a.x); }
// Independent pixel-center triangle coverage over the actual submitted strip.
// The sink does not know the producer's position/size or normalize its rectangles.
void Triangle(const CgsGraphics::Basic2dColouredTexturedVertex& a,
              const CgsGraphics::Basic2dColouredTexturedVertex& b,
              const CgsGraphics::Basic2dColouredTexturedVertex& c)
{
    if (!std::isfinite(a.mv2Pos.x) || !std::isfinite(a.mv2Pos.y)
        || !std::isfinite(b.mv2Pos.x) || !std::isfinite(b.mv2Pos.y)
        || !std::isfinite(c.mv2Pos.x) || !std::isfinite(c.mv2Pos.y)) return;
    const float area = Edge(a.mv2Pos,b.mv2Pos,c.mv2Pos.x,c.mv2Pos.y);
    if (area == 0) return;
    u32 colour;
    std::memcpy(&colour,&a.mv4Colour,sizeof(colour));
    for (unsigned y=0;y<80;++y) for (unsigned x=0;x<100;++x) {
        const float e0=Edge(a.mv2Pos,b.mv2Pos,x+.5f,y+.5f);
        const float e1=Edge(b.mv2Pos,c.mv2Pos,x+.5f,y+.5f);
        const float e2=Edge(c.mv2Pos,a.mv2Pos,x+.5f,y+.5f);
        if ((e0>=0 && e1>=0 && e2>=0) || (e0<=0 && e1<=0 && e2<=0))
            sauPixels[y][x]=colour;
    }
}
void Reset()
{ saSubmissions.clear(); std::memset(sauPixels,0,sizeof(sauPixels)); suTextCalls=0; }
unsigned PixelCount()
{ unsigned n=0; for(auto& row:sauPixels) for(u32 colour:row) n+=colour!=0; return n; }
bool Bounds(float x0,float y0,float x1,float y1)
{
    if(saSubmissions.size()!=1 || saSubmissions[0].mVertices.size()!=4) return false;
    const auto& v=saSubmissions[0].mVertices;
    return v[0].mv2Pos.x==x0 && v[0].mv2Pos.y==y0
        && v[1].mv2Pos.x==x1 && v[1].mv2Pos.y==y0
        && v[2].mv2Pos.x==x0 && v[2].mv2Pos.y==y1
        && v[3].mv2Pos.x==x1 && v[3].mv2Pos.y==y1;
}
}

namespace CgsDev::Assert {
int BeginAssert() { Check(false,"unexpected production assert"); return 0; }
int FireAssert(const char*,const char*,int) { return 0; }
void* EndAssert() { return nullptr; }
}
namespace CgsDev::Log { StrStreamBase* gpDebugPrint=nullptr; }
namespace CgsDev::Message { u64 gxMessageFilterFlags=0; }
namespace CgsGraphics {
// Graphics-device boundary only. EmitQuad/AddVertex/DispatchVertices remain production bodies.
const renderengine::TextureState* GetIm3dTextureStateUntexturedPC() { return nullptr; }
void Im2dRenderBuffer::Dispatch(Im2d*) const { Check(false,"no native device dispatch in fixture"); }
template<> void ImRenderBuffer<Basic2dColouredTexturedVertex>::SetState(const renderengine::TextureState*) {}
template<> void ImRenderBuffer<Basic2dColouredTexturedVertex>::Render(
    renderengine::PrimitiveType topology,const Basic2dColouredTexturedVertex* vertices,u32 count)
{
    saSubmissions.push_back({static_cast<s32>(topology),{vertices,vertices+count}});
    if(static_cast<s32>(topology)==6 && count==4) {
        Triangle(vertices[0],vertices[1],vertices[2]);
        Triangle(vertices[2],vertices[1],vertices[3]);
    }
}
}
namespace CgsDev {
// Text measurement/output and UI configuration are boundaries, not box/queue replacements.
void Debug2DImmediateRender::DrawText(const char* text,float x,float y,float scale,RGBA colour)
{ ++suTextCalls; spText=text; sfTextX=x; sfTextY=y; sfTextScale=scale; suTextColour=colour; }
Vector2 Debug2DImmediateRender::CalcTextExtent(const char*,f32) const { return {32,8,0,0}; }
namespace DebugUI {
const Palette& MenuItem::GetPalette() const { return sPalette; }
const Metrics& MenuItem::GetMetrics() const { return sMetrics; }
}
}
#include "debug_2d_box_contract.inc"

int main()
{
    CgsGraphics::Im2dRenderBuffer sink={};
    CgsDev::Debug2DImmediateRender render={};
    render.mpRenderBuffer=&sink;
    render.mfVirtualScreenWidth=100; render.mfVirtualScreenHeight=80;
    render.miIm2dVertsHead=0; render.meDrawingMode=CgsDev::Debug2DImmediateRender::E_DRAWING_COUNT;
    const u32 white=0xFFFFFFFF, red=0xFF0000FF, blue=0xFFFF0000;

    Reset(); render.DrawBox(Vector2{20,10,0,0},Vector2{7,5,0,0},white);
    Check(Bounds(20,10,27,15),"vector box adds size to nonzero origin");
    Check(PixelCount()==35,"vector box produces independent 7-by-5 pixel rectangle");
    Check(sauPixels[10][20]==white && sauPixels[14][26]==white,"both interior corners are covered");
    Check(sauPixels[10][27]==0 && sauPixels[15][20]==0,"right and bottom outside pixels stay clear");
    Check(saSubmissions.size()==1 && saSubmissions[0].miTopology==6,"native box remains a single four-vertex strip");
    Reset(); render.DrawBox(20,10,27,15,white);
    Check(Bounds(20,10,27,15) && PixelCount()==35,"scalar overload accepts two corners");

    Reset(); render.DrawBox(Vector2{-3,4,0,0},Vector2{5,3,0,0},white);
    Check(Bounds(-3,4,2,7),"partly offscreen geometry retains original unclipped corners");
    Check(PixelCount()==6,"partly offscreen box has six visible pixels");
    Reset(); render.DrawBox(Vector2{-10,4,0,0},Vector2{3,3,0,0},white);
    Check(saSubmissions.empty(),"strict separation on left culls before submission");
    Reset(); render.DrawBox(Vector2{100,20,0,0},Vector2{5,5,0,0},white);
    Check(saSubmissions.size()==1 && PixelCount()==0,"touching right screen edge is submitted");
    Reset(); render.DrawBox(Vector2{100.01f,20,0,0},Vector2{5,5,0,0},white);
    Check(saSubmissions.empty(),"strictly beyond right screen edge is culled");
    Reset(); render.DrawBox(Vector2{-4,20,0,0},Vector2{4,5,0,0},white);
    Check(saSubmissions.size()==1 && PixelCount()==0,"touching left screen edge is submitted");
    Reset(); render.DrawBox(Vector2{20,10,0,0},Vector2{0,0,0,0},white);
    Check(saSubmissions.size()==1 && PixelCount()==0,"zero size retains degenerate original draw");
    Reset(); render.DrawBox(Vector2{30,20,0,0},Vector2{-6,4,0,0},white);
    Check(Bounds(30,20,24,24),"negative width retains reversed vertex order");
    Check(PixelCount()==24,"negative-width strip still covers its two-sided pixel rectangle");
    Reset(); render.DrawBox(Vector2{120,20,0,0},Vector2{-40,4,0,0},white);
    Check(saSubmissions.empty(),"reversed bounds are not sorted to rescue an outside origin");
    Reset(); render.DrawBox(Vector2{100,80,0,0},Vector2{-10,-10,0,0},white);
    Check(Bounds(100,80,90,70) && PixelCount()==100,"negative dimensions at screen corner retain original extent");
    Reset(); const float nan=std::numeric_limits<float>::quiet_NaN();
    render.DrawBox(Vector2{20,5,0,0},Vector2{nan,4,0,0},white);
    Check(saSubmissions.size()==1 && std::isnan(saSubmissions[0].mVertices[1].mv2Pos.x),
          "unordered end does not trigger strict-separation cull");

    CgsDev::DebugRender buffered={}; buffered.m2DQueue.Construct();
    buffered.Draw2DBox(Vector2{20,10,0,0},Vector2{7,5,0,0},red);
    const CgsModule::Event* event=nullptr; s32 size=0;
    const s32 type=buffered.m2DQueue.GetFirstEvent(&event,&size);
    const auto* box=static_cast<const CgsDev::Internal::CInEventDrawBox2D*>(event);
    Check(type==3 && size==sizeof(*box),"actual queue stores original box event type and width");
    Check(box && box->mfX==20 && box->mfY==10 && box->mfWidth==7 && box->mfHeight==5,
          "buffered vector publisher stores size directly");
    Reset(); buffered.Dispatch2D(&render,true);
    Check(Bounds(20,10,27,15) && PixelCount()==35,"real queue replay reaches canonical position-plus-size backend");
    Check(buffered.m2DQueue.GetLength()==0,"clear-after-replay empties real variable queue");
    buffered.Draw2DBox(20,10,27,15,blue);
    Reset(); buffered.Dispatch2D(&render,true);
    Check(Bounds(20,10,27,15) && sauPixels[12][24]==blue,"native scalar buffered convenience keeps min/max contract");
    buffered.Draw2DBox(Vector2{20,10,0,0},Vector2{7,5,0,0},red);
    buffered.Draw2DBox(Vector2{24,12,0,0},Vector2{7,5,0,0},blue);
    Reset(); buffered.Dispatch2D(&render,false);
    Check(saSubmissions.size()==2 && sauPixels[12][24]==blue && sauPixels[10][20]==red,
          "two production events preserve ordered pixel overlap");
    Check(buffered.m2DQueue.GetLength()==2,"replay without clear preserves real queue records");
    Reset(); buffered.Dispatch2D(&render,true);
    Check(saSubmissions.size()==2 && buffered.m2DQueue.GetLength()==0,"retained records replay then clear once");

    Reset(); render.DrawFrame(20,10,27,15,white,1);
    Check(saSubmissions.size()==4 && PixelCount()==28,"existing frame keeps four original strips around two corners");
    Check(sauPixels[9][19]==white && sauPixels[15][27]==white && sauPixels[10][20]==0,
          "frame corner coverage and empty interior survive contract correction");
    Reset(); render.DrawTextWithBackground("Sound",20,30,8,white,red,2);
    Check(Bounds(18,28,54,40) && PixelCount()==432,"text background keeps measured extent plus border");
    Check(suTextCalls==1 && sfTextX==20 && sfTextY==30 && suTextColour==white,
          "text background retains foreground origin and color");

    sMetrics.mfWindowBorderSize=2; sMetrics.mfTextSize=8;
    sPalette.mColourHighlight=red; sPalette.mColourHighlightText=white; sPalette.mColourText=blue;
    CgsDev::DebugUI::MenuItem item;
    Reset(); item.RenderMenuItemText(&render,"Draw Active Hulls",20,30,40,10,true,40);
    Check(Bounds(18,28,62,42) && PixelCount()==616,"production selected menu row retains full readable highlight extent");
    Check(suTextCalls==1 && std::strcmp(spText,"Draw Active Hulls")==0
          && sfTextX==20 && sfTextY==30 && sfTextScale==8 && suTextColour==white,
          "production selected row keeps label position size and foreground color");
    Reset(); item.RenderMenuItemText(&render,"Draw Active Hulls",20,30,40,10,false,40);
    Check(saSubmissions.empty() && suTextCalls==1 && suTextColour==blue,"ordinary menu row keeps text and no highlight");
    Reset(); item.RenderMenuItemBackground(&render,20,30,40,10,true,40);
    Check(Bounds(18,28,62,42) && suTextCalls==0,"separate selected-row background uses the same original geometry");
    Reset(); item.RenderMenuItemBackground(&render,20,30,40,10,false,40);
    Check(saSubmissions.empty(),"unselected row background remains absent");

    const Vector2 barMin={20,10,0,0}, barMax={40,20,0,0};
    Reset(); render.DrawHorizontalBar(barMin,barMax,5,10,red,blue);
    Check(saSubmissions.size()==2 && PixelCount()==200,"horizontal bar fills the complete background before foreground");
    Check(sauPixels[10][20]==blue && sauPixels[19][39]==blue,"horizontal bar second color is background");
    Check(sauPixels[12][20]==red && sauPixels[17][29]==red && sauPixels[12][30]==blue,
          "half bar uses original left edge and fused right endpoint at30");
    Reset(); render.DrawHorizontalBar(barMin,barMax,0,10,red,blue);
    Check(saSubmissions.size()==2 && sauPixels[12][20]==red && sauPixels[12][21]==blue,
          "original zero-value bar keeps one-unit foreground strip");
    Reset(); render.DrawHorizontalBar(barMin,barMax,10,10,red,blue);
    Check(sauPixels[12][38]==red && sauPixels[12][39]==blue,
          "full bar retains original one-unit right background border");
    Reset(); render.DrawHorizontalBar(barMin,barMax,-1,10,red,blue);
    Check(saSubmissions.empty(),"negative bar value rejects before background");
    Reset(); render.DrawHorizontalBar(barMin,barMax,11,10,red,blue);
    Check(saSubmissions.empty(),"above-maximum bar value rejects before background");
    Reset(); render.DrawHorizontalBar(barMin,barMax,0,0,red,blue);
    Check(saSubmissions.empty(),"nonpositive bar maximum rejects before background");
    Reset(); render.DrawHorizontalBar(barMin,barMax,0,nan,red,blue);
    Check(saSubmissions.empty(),"unordered maximum takes original not-GT rejection");
    Reset(); render.DrawHorizontalBar(barMin,barMax,nan,10,red,blue);
    Check(saSubmissions.size()==2 && PixelCount()==200 && sauPixels[12][20]==blue,
          "unordered value passes strict branches and leaves background pixels beneath NaN foreground");
    Reset(); render.DrawHorizontalBar(barMin,barMax,5,std::numeric_limits<float>::infinity(),red,blue);
    Check(saSubmissions.size()==2 && sauPixels[12][20]==red && sauPixels[12][21]==blue,
          "infinite maximum uses the original zero ratio without a finite-value guard");
    std::printf("Debug2DBoxContract: %u checks, %u failures\n",suChecks,suFailures);
    return suFailures ? 1 : 0;
}
