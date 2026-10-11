#include <cstdio>
#include <cmath>
#include <limits>
#include <cstdlib>
#include <cstring>
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Development/VectorFont/CgsVectorFontData.h"
namespace CgsDev::Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*,const char*,int) { return 0; }
void* EndAssert() { return nullptr; }
}
namespace CgsDev::Log { struct DebugPrint { void AppendFormat(const char*,...) {} }; static DebugPrint* gpDebugPrint=nullptr; }
static unsigned suChecks,suFailures,suTextCalls,suVectorCalls,suFlushes;
static CgsGraphics::TextObject sText;
static float sfVectorX,sfVectorY;
static void Check(bool good,const char* label) { ++suChecks; if(!good) { ++suFailures; std::printf("FAIL %s\n",label); } }
// The font resource and text sink are seams; the original DrawText body builds
// the actual TextObject that is inspected here. It never substitutes a draw body.
namespace CgsResource {
float Font::GetStringWidth(const CgsUtf8*) const { return 2.5f; }
}
namespace CgsGraphics {
void TextRenderer::RenderString(Im2dRenderBuffer*,const TextObject& text) { ++suTextCalls; sText=text; }
void TextObject::CalculateAutosizing() { Check(false,"debug text does not enable autosizing"); }
}
namespace CgsDev {
void Debug2DImmediateRender::DispatchVertices() { ++suFlushes; miIm2dVertsHead=0; }
float Debug2DImmediateRender::CalcTextWidth(const char*,float) const { return 20; }
float VectorFont::Print(float x,float y,const char*,u32) { ++suVectorCalls; sfVectorX=x; sfVectorY=y; return x; }
}
#include "debug_2d_text.inc"
int main() {
    CgsDev::Debug2DImmediateRender render={};
    CgsResource::Font font={}; CgsResource::Font* memory=&font;
    render.mpFont.mpResourceMemory=&memory;
    render.mpFont.mpSourceEntry=reinterpret_cast<CgsResource::Entry*>(1);
    render.mTextRenderer.mpIm2dRenderBuffer=reinterpret_cast<CgsGraphics::Im2dRenderBuffer*>(1);
    render.mTextRenderer.mpIm3dRenderBuffer=reinterpret_cast<CgsGraphics::Im3dRenderBuffer*>(1);
    render.Construct(nullptr,100,80);
    Check(!render.HasResourceFont() && render.mpFont.mpSourceEntry==nullptr,"Construct restores the original null font handle");
    Check(render.mTextRenderer.mpIm2dRenderBuffer==nullptr && render.mTextRenderer.mpIm3dRenderBuffer==nullptr,
          "Construct initializes the original TextRenderer member");
    Check(render.mVectorFont.mSize.x==1.25f && render.mVectorFont.mSize.y==1.25f,"Construct restores the ten-unit vector-font default size");
    render.mpFont.mpResourceMemory=&memory;
    render.mpFont.mpSourceEntry=nullptr;
    render.DrawText("Average speed: 42 mph",Vector2{40,20,0,0},8,0x12345678,false);
    Check(suTextCalls==1 && sText.mv2TopLeft.mX==40 && sText.mv2TopLeft.mY==20,"resource text retains the requested origin");
    Check(sText.mv2BottomRight.mX==60 && sText.mv2BottomRight.mY==28,"resource text uses the original exact one-line bounds");
    Check(sText.mfStringWidth==2.5f && sText.mfFontHeight==8,"cached string width stays unscaled while font height carries scale");
    Check(sText.mbMultiLine==0 && sText.mbWordWrap==0 && sText.meAlignment==CgsGraphics::TextObject::E_ALIGNMENT_LEFT,
          "the original alignment store does not enable multiline wrapping");
    Check(sText.mTextColour==0x12345678,"packed text colour reaches the real TextObject");
    render.DrawText("ENGINE RUNNING",Vector2{40,20,0,0},8,0xffffffff,true);
    Check(suTextCalls==2 && sText.mv2TopLeft.mX==40,"the bool parameter checks screen visibility without centering");
    render.DrawText("offscreen",Vector2{-1,20,0,0},8,0xffffffff,true);
    Check(suTextCalls==2,"requested screen check culls an offscreen point");
    render.DrawText("offscreen",Vector2{-1,20,0,0},8,0xffffffff,false);
    Check(suTextCalls==3 && sText.mv2TopLeft.mX==-1,"disabled screen check preserves an offscreen draw");
    render.DrawText("edge",Vector2{100,80,0,0},8,0xffffffff,true);
    Check(suTextCalls==4 && sText.mv2TopLeft.mX==100,"screen-edge culling is inclusive");
    render.DrawText("NaN",Vector2{std::numeric_limits<float>::quiet_NaN(),20,0,0},8,0xffffffff,true);
    Check(suTextCalls==4,"unordered coordinates fail the original screen check");
    MaybeDrawText(&render,"wrapper",-1,20,8,0x12345678,true);
    Check(suTextCalls==5 && sText.mv2TopLeft.mX==-1 && sText.mTextColour==0x12345678,
          "scalar wrapper always disables screen checking as in ARTIST");
    render.mpFont.Clear();
    render.DrawText("vector",Vector2{40,20,0,0},8,0xffffffff,true);
    Check(suVectorCalls==1 && sfVectorX==40 && sfVectorY==20 && render.mVectorFont.mSize.x==1,
          "no-font fallback keeps the same position and original eighth-scale strokes");
    render.DrawText("vector",Vector2{40,-1,0,0},8,0xffffffff,true);
    Check(suVectorCalls==1,"screen culling applies to the vector fallback too");
    Check(suFlushes==2,"drawing-mode transitions flush once into font mode and once into vector mode");
    std::printf("Debug2DText: %u checks, %u failures\n",suChecks,suFailures);
    return suFailures ? 1 : 0;
}
