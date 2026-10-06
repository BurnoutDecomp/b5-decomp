#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static unsigned guChecks, guFailures, guDraws;
#undef CGS_ASSERT
#define CGS_ASSERT(test, msg) do { if (!(test)) ++guFailures; } while (0)
static CgsGraphics::Basic2dColouredTexturedVertex gaVertices[4];
namespace CgsGui { const void* gpGuiBlendStateStandard = nullptr; }
namespace BrnGui {
static CgsGraphics::Vector2 MakeV2(f32 x, f32 y) { return {x,y}; }
static f32 Clamp01(f32 f) { return f < 0 ? 0 : f > 1 ? 1 : f; }
struct FixtureBuffer {
    template<class T> void SetState(T) {}
    template<class P> void Render(P, const CgsGraphics::Basic2dColouredTexturedVertex* p, u32 n) {
        if (n != 4) { ++guFailures; return; }
        std::memcpy(gaVertices,p,sizeof(gaVertices)); ++guDraws;
    }
};
struct FixtureCache { f32 GetTime() const { return 5; } };
struct FixtureCursorStatus { Vector2 mv2Position; int miDisplayState, miAnimationState; };
class CrashNavIconRenderer {
public:
    using Im2dCommandBuffer = FixtureBuffer;
    const void* mpIconsTextureState;
    FixtureCache* mpGuiCache;
    FixtureCursorStatus mGuiEventMapCursorStatus;
    f32 mfHoveredIconScaleEndTime, mfCursorScaleFactor;
    bool mfHoveredIconGrowing;
    void RenderCursor(Im2dCommandBuffer*);
};
#include "playtest_full_map_cursor_geometry.inc"
}
static void Check(bool b, const char* s) {
    ++guChecks; if (!b) { ++guFailures; std::printf("FAIL %s\n",s); }
}
static bool Near(f32 a, f32 b) { return std::fabs(a-b) < 0.000001f; }
int main() {
    BrnGui::FixtureCache lCache;
    BrnGui::FixtureBuffer lBuffer;
    BrnGui::CrashNavIconRenderer lRenderer = {};
    lRenderer.mpGuiCache = &lCache;
    lRenderer.mpIconsTextureState = &lCache;
    lRenderer.mGuiEventMapCursorStatus.mv2Position = {856,364,0,0};
    lRenderer.mGuiEventMapCursorStatus.miDisplayState = 1;
    lRenderer.RenderCursor(&lBuffer);
    Check(guDraws == 1,"free cursor draws one strip");
    f32 cx=(gaVertices[0].mv2Pos.x+gaVertices[3].mv2Pos.x)*0.5f;
    f32 cy=(gaVertices[0].mv2Pos.y+gaVertices[3].mv2Pos.y)*0.5f;
    Check(Near(cx,856.0f/1280-0.0035f) && Near(cy,364.0f/720+0.007f),
          "free cursor has the original authored center offset");
    Check(Near(gaVertices[3].mv2Pos.x-cx,0.026f) && Near(gaVertices[3].mv2Pos.y-cy,0.045f),
          "free cursor uses original normalized extents");
    lRenderer.mGuiEventMapCursorStatus.miDisplayState = 0;
    lRenderer.mGuiEventMapCursorStatus.miAnimationState = 0;
    lRenderer.RenderCursor(&lBuffer);
    cx=(gaVertices[0].mv2Pos.x+gaVertices[3].mv2Pos.x)*0.5f;
    cy=(gaVertices[0].mv2Pos.y+gaVertices[3].mv2Pos.y)*0.5f;
    Check(Near(cx,856.0f/1280-0.007f) && Near(cy,364.0f/720+0.015f),
          "event highlight uses its distinct authored center offset");
    Check(Near(gaVertices[3].mv2Pos.x-cx,0.052f) && Near(gaVertices[3].mv2Pos.y-cy,0.09f),
          "event highlight is twice the free-cursor size");
    guDraws=0;
    lRenderer.RenderCursor(&lBuffer);
    Check(guDraws==0,"original cursor consumption prevents a second working-record draw");
    Check(BrnGui::KAF_ICON_HALFWIDTH[0]==75.0f &&
          BrnGui::KAF_ICON_HALFHEIGHT[1]==133.3333435f,
          "full event extents match original CRT fmuls results");
    Check(BrnGui::KAF_MINI_ICON_HALFWIDTH[1]==31.25f &&
          BrnGui::KAF_MINI_ICON_HALFHEIGHT[0]==55.555557f,
          "mini event extents match their separate CRT fmuls results");
    Check(BrnGui::KF_DRIVETHROUGH_HALFWIDTH==0.0125f &&
          BrnGui::KF_DRIVETHROUGH_HALFHEIGHT==0.03f &&
          BrnGui::KF_DRIVETHROUGH_HOVER_LIFT==-0.02f,
          "drive-through geometry retains original sizes and negative hover lift");
    u32 luPulseBits;
    std::memcpy(&luPulseBits, &BrnGui::KF_PLAYER_ICON_PULSE_PERIOD, sizeof(luPulseBits));
    Check(luPulseBits==0x3F933333u,"player pulse period matches ARTIST data at82F25C84");
    std::printf("PlaytestFullMapCursorGeometry: %u checks, %u failures\n",guChecks,guFailures);
    return guFailures ? 1 : 0;
}
