#include "GameSource/Gui/CustomRenderer/Renderers/BrnBlackBarRenderer.h"
#include "GameSource/Gui/Events/BrnGuiEventSetBlackBars.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"
#include "GameShared/GameClasses/Gui/View/ParticleSystem2d/CgsBillboardRenderer.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static unsigned guChecks, guFailures, guDraws, guBegins, guEnds;
static CgsGraphics::Basic2dColouredTexturedVertex gaVertices[2][4];
static CgsGraphics::Im2dTransform gTransform;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++guFailures; return 0; }
void* EndAssert() { return 0; }
} }
namespace CgsGui {
const renderengine::BlendState* gpGuiBlendStateStandard = 0;
const renderengine::RasterizerState* gpGuiRasterizerStateCullNone = 0;
const renderengine::DepthStencilState* gpBillboardDepthStencilState = 0;
const CgsGraphics::Im2dTransform gBillboardScreenTransform = {
    { 0, 0, 0, 0 }, { 1280, 0, 0, 720 }, { 0, 0, 0, 0 }, { 255, 255, 255, 255 }
};
renderengine::Texture* CustomRenderComponentInterface::GetRenderOutput(s32, s32*, ImRendererSet*) { return 0; }
}
namespace CgsGraphics {
using Vertex = Basic2dColouredTexturedVertex;
void Im2dRenderBuffer::Dispatch(Im2d*) const {}
template<> void ImRenderBuffer<Vertex>::BeginRendering() { ++guBegins; }
template<> void ImRenderBuffer<Vertex>::EndRendering() { ++guEnds; }
template<> void ImRenderBuffer<Vertex>::SetTransform(const Im2dTransform& lrTransform) { gTransform = lrTransform; }
template<> void ImRenderBuffer<Vertex>::SetState(const renderengine::BlendState*) {}
template<> void ImRenderBuffer<Vertex>::SetState(const renderengine::RasterizerState*) {}
template<> void ImRenderBuffer<Vertex>::SetState(const renderengine::DepthStencilState*) {}
template<> void ImRenderBuffer<Vertex>::SetTexture(renderengine::Texture* lpTexture) { if (lpTexture) ++guFailures; }
template<> void ImRenderBuffer<Vertex>::Render(renderengine::PrimitiveType leType, const Vertex* lpVertices, u32 luCount) {
    if (static_cast<int>(leType) != 6 || luCount != 4 || guDraws >= 2) { ++guFailures; return; }
    std::memcpy(gaVertices[guDraws++], lpVertices, sizeof(gaVertices[0]));
}
}
namespace BrnGui {
#include "playtest_cinematic_bars.inc"
}
static void Check(bool lbPassed, const char* lpcLabel) {
    ++guChecks; if (!lbPassed) { ++guFailures; std::printf("FAIL %s\n", lpcLabel); }
}
static void Send(BrnGui::BlackBarRenderer& lrRenderer, f32 lfHeight) {
    lrRenderer.RecvEvent(reinterpret_cast<const CgsModule::Event*>(&lfHeight), 221);
}
int main() {
    BrnGui::BlackBarRenderer lRenderer;
    lRenderer.Construct();
    Check(!lRenderer.GetRenderEnabled(), "construct disables rendering");
    Check(lRenderer.GetNumTextures() == 1, "original custom-renderer texture count");
    Check(lRenderer.GetID() == 0x55082A6E21681C00ull, "complete 64-bit identity");
    Check(sizeof(BrnGui::GuiEventSetBlackBars) - sizeof(CgsGui::GuiEvent<221>) == sizeof(f32), "one float event payload");
    Send(lRenderer, -1.0f); Check(!lRenderer.GetRenderEnabled(), "negative size disables");
    Send(lRenderer, 0.17f); Check(lRenderer.GetRenderEnabled(), "positive size enables");
    CgsGui::AptIm2dRenderBuffer lBuffer = {};
    CgsGui::ImRendererSet lSet = {};
    lSet.mpIm2dRenderBuffer = &lBuffer;
    lRenderer.RenderComponent(&lSet);
    Check(guDraws == 0, "no bars before game-start event");
    lRenderer.RecvEvent(0, 145);
    lRenderer.RenderComponent(&lSet);
    Check(guDraws == 2 && guBegins == 1 && guEnds == 1, "two strips in one complete bracket");
    Check(gaVertices[0][0].mv2Pos.y == 0 && gaVertices[0][1].mv2Pos.y == 0.17f, "top strip covers requested height");
    Check(gaVertices[1][0].mv2Pos.y == 1.0f - 0.17f && gaVertices[1][1].mv2Pos.y == 1.0f, "bottom strip preserves center view");
    Check(gaVertices[0][0].mv4Colour.a == 255 && gaVertices[0][0].mv4Colour.r == 0, "opaque black bars");
    Check(gTransform.mRightUp.x == 1280 && gTransform.mRightUp.w == 720, "full logical screen transform");
    Send(lRenderer, 1.0f); guDraws = 0; lRenderer.RenderComponent(&lSet);
    Check(gaVertices[0][1].mv2Pos.y == 0.5f && gaVertices[1][0].mv2Pos.y == 0.5f, "invalid-camera request clamps to complete screen coverage");
    Send(lRenderer, 0.0f); Check(!lRenderer.GetRenderEnabled(), "return to gameplay removes bars");
    std::printf("PlaytestCinematicBars: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
