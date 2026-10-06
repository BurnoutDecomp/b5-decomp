#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h"
#include <cstdio>
#include <cmath>

namespace BrnGui {
static Vector4 MakeV4(f32 x, f32 y, f32 z, f32 w) { return {x,y,z,w}; }
static f32 Clamp01(f32 f) { return f < 0 ? 0 : f > 1 ? 1 : f; }
struct FixtureIcon {
    int miState;
    f32 mfAlpha;
    Vector2 mv2Pos;
    int GetState() const { return miState; }
    Vector2 GetPosition() const { return mv2Pos; }
    f32 GetAlpha() const { return mfAlpha; }
    f32 GetRotation() const { return 0; }
};
using CrashNavMapIcon = FixtureIcon;
static Vector4 GetRivalIconColour(int i, bool* lpLocal) {
    *lpLocal = i == 1;
    return {1,1,1,1};
}
struct FixtureCache { f32 mfTime = 5.0f; f32 GetTime() const { return mfTime; } };
struct FixtureBuffer {};
struct FixtureHover { CgsID mHoveredPlayerID; };
static Vector4 gaUV[3], gaColour[3];
static unsigned guDraws;
class CrashNavIconRenderer {
public:
    using Im2dCommandBuffer = FixtureBuffer;
    enum { E_CRASHNAVICON_NUM=2, E_CRASHNAVICON_EVENT_NOTATTEMPTED=0,
           KU_ICON_EVENT_TYPE_COUNT=11, E_CRASHNAVICON_EVENTTYPE_MINI_INDEX_COUNT=6,
           KI_PLAYER_ICON_INDEX=5, KI_PLAYER_ICON_OVERLAY_INDEX=7 };
    CgsGraphics::Vector2 mav2IconUvTopLeft[2][11], mav2IconUvBottomLeft[2][11];
    CgsGraphics::Vector2 mav2IconUvTopRight[2][11], mav2IconUvBottomRight[2][11];
    CgsGraphics::Vector2 mav2MiniIconUvTopLeft[2][6], mav2MiniIconUvBottomLeft[2][6];
    CgsGraphics::Vector2 mav2MiniIconUvTopRight[2][6], mav2MiniIconUvBottomRight[2][6];
    FixtureIcon mRivalIcons[8];
    int miRivalIconsCount;
    FixtureCache* mpGuiCache;
    FixtureHover mHoveredEventIcon, mHoveredEventIconLastFrame;
    f32 mfHoveredIconScaleEndTime, mfHoveredIconScaleFactor;
    bool mfHoveredIconGrowing;
    f32 mfPlayerIconPulseEndTime, mfPlayerIconPulseScale;
    const void* mapIconTextureStates[2];
    void InitEventTypeUvs();
    void RenderRivals(Im2dCommandBuffer*);
    void RotatateRect(Vector4, f32, CgsGraphics::Vector2&, CgsGraphics::Vector2&,
                     CgsGraphics::Vector2&, CgsGraphics::Vector2&);
    void RenderQuad(Im2dCommandBuffer*, CgsGraphics::Vector2, CgsGraphics::Vector2,
                    CgsGraphics::Vector2, CgsGraphics::Vector2, Vector4 colour,
                    const void*, const void*, const Vector4& uv) {
        gaUV[guDraws] = uv; gaColour[guDraws] = colour; ++guDraws;
    }
};
}
namespace CgsGui { const void* gpGuiBlendStateStandard = nullptr; }
namespace BrnGui {
#include "playtest_full_map_player_uv.inc"
}
static unsigned guChecks, guFailures;
static void Check(bool lbPass, const char* lpcName) {
    ++guChecks;
    if (!lbPass) { ++guFailures; std::printf("FAIL %s\n", lpcName); }
}
static bool UV(Vector4 v, f32 x, f32 y, f32 z, f32 w) {
    return v.x == x && v.y == y && v.z == z && v.w == w;
}
int main() {
    BrnGui::FixtureCache lCache;
    BrnGui::CrashNavIconRenderer lRenderer = {};
    lRenderer.mpGuiCache = &lCache;
    lRenderer.InitEventTypeUvs();
    lRenderer.miRivalIconsCount = 1;
    lRenderer.mRivalIcons[0].miState = 1;
    lRenderer.mRivalIcons[0].mfAlpha = 100;
    lRenderer.mRivalIcons[0].mv2Pos = {856,364,0,0};
    lRenderer.RenderRivals(nullptr);
    Check(BrnGui::guDraws == 2, "local player draws halo then marker");
    Check(UV(BrnGui::gaUV[0],0,0.5f,0.25f,0.75f), "halo uses original column7 atlas cell");
    Check(UV(BrnGui::gaUV[1],0.25f,0.25f,0.5f,0.5f), "player uses original column5 atlas cell");
    BrnGui::guDraws = 0;
    lRenderer.mRivalIcons[0].miState = 14;
    lRenderer.mRivalIcons[0].mfAlpha = 50;
    lRenderer.RenderRivals(nullptr);
    Check(BrnGui::guDraws == 1, "rival does not get the local player's halo");
    Check(UV(BrnGui::gaUV[0],0.25f,0.25f,0.5f,0.5f), "rival uses the same marker cell");
    Check(BrnGui::gaColour[0].w == 0.5f, "marker keeps original icon alpha scale");
    BrnGui::guDraws = 0;
    lRenderer.mRivalIcons[0].miState = 1;
    lRenderer.mHoveredEventIconLastFrame.mHoveredPlayerID = 0x100000001ull;
    lRenderer.mHoveredEventIcon.mHoveredPlayerID = 0x200000001ull;
    lRenderer.mfHoveredIconGrowing = false;
    lRenderer.mfHoveredIconScaleEndTime = 20;
    lRenderer.RenderRivals(nullptr);
    Check(lRenderer.mfHoveredIconGrowing && lRenderer.mfHoveredIconScaleEndTime < 6,
          "hover comparison keeps all64bits even when lowwords collide");
    for (int i=1; i<=3; ++i) {
        BrnGui::guDraws = 0;
        lCache.mfTime = 5.0f + 1.15f * (i *0.25f);
        lRenderer.mfPlayerIconPulseEndTime = 6.15f;
        lRenderer.RenderRivals(nullptr);
        const f32 expected = 1.0f + 1.5f * (i *0.25f);
        Check(std::fabs(lRenderer.mfPlayerIconPulseScale-expected)<0.00001f,
              "halo grows according to ARTIST fnmsubs of remaining pulse time");
        Check(std::fabs(BrnGui::gaColour[0].w-(2.0f-expected))<0.00001f,
              "growing halo fades with the original alpha");
    }
    std::printf("PlaytestFullMapPlayerUV: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
