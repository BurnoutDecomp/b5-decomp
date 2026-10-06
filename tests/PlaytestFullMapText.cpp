#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h"
#include <cmath>
#include <cstdio>
#include <cstring>

// Identity aspect isolates the constructor's ARTIST VMX basis and the native-to-
// logical command boundary. The aspect fold itself has its own production tests.
namespace CgsGraphics {
using RGBA = u32;
void Im2dTransform::TransformByAspectRatio() {}
#include "playtest_full_map_text_adapter.inc"
}
namespace BrnGui {
static f32 Clamp01(f32 f) { return f < 0 ? 0 : f > 1 ? 1 : f; }
struct FixtureBuffer {
    CgsGraphics::Im2dTransform mSubmitted;
    void SetTransform(const CgsGraphics::Im2dTransform& t) { mSubmitted = t; }
};
class CrashNavIconRenderer {
public:
    CgsGraphics::Im2dTransform mTextTransform;
    void ConstructTextTransform();
    void SubmitTextTransform(FixtureBuffer*);
};
#include "playtest_full_map_text_bodies.inc"
}
static unsigned guChecks, guFailures;
static void Check(bool b, const char* name) {
    ++guChecks;
    if (!b) { ++guFailures; std::printf("FAIL %s\n",name); }
}
static bool Near(float a,float b) { return std::fabs(a-b) < 0.00015f; }
int main() {
    BrnGui::CrashNavIconRenderer renderer = {};
    renderer.ConstructTextTransform();
    const auto& t = renderer.mTextTransform;
    // ARTIST82463744..8246382C: CDA3C0/CDA350 vperm then vsldoi8.
    Check(Near(t.mRightUp.x,1.0f/640.0f),"native right.x from VMX oracle");
    Check(t.mRightUp.y==0,"native right.y from VMX oracle");
    Check(t.mRightUp.z==0,"native up.x from VMX oracle");
    Check(Near(t.mRightUp.w,-1.0f/360.0f),"native up.y from VMX oracle");
    Check(t.mOriginXYZ.x==-1 && t.mOriginXYZ.y==1,"native pixel origin");
    Check(t.mColourScale.x==1 && t.mColourScale.w==1,"native unit colour scale");
    BrnGui::FixtureBuffer buffer = {};
    renderer.SubmitTextTransform(&buffer);
    const auto& cmd = buffer.mSubmitted;
    const float points[5][2]={{0,0},{640,360},{1280,720},{700,270},{400,505}};
    for (const auto& point:points) {
        const float x=cmd.mOriginXYZ.x + point[0]*cmd.mRightUp.x + point[1]*cmd.mRightUp.z;
        const float y=cmd.mOriginXYZ.y + point[0]*cmd.mRightUp.y + point[1]*cmd.mRightUp.w;
        Check(Near(x,point[0]) && Near(y,point[1]),"road glyph logical-pixel position survives command seam");
    }
    Check(cmd.mColourScale.x==255 && cmd.mColourScale.y==255 &&
          cmd.mColourScale.z==255 && cmd.mColourScale.w==255,
          "road glyph colour and opacity survive command seam");
    const Vector4 colours[2]={{0.90196079f,0.90196079f,0.90196079f,1},{0,0,0,1}};
    for (int i=0;i<2;++i) {
        const u32 colour=BrnGui::RoadTextColour(colours[i]);
        CgsGraphics::RGBA8 bytes;
        std::memcpy(&bytes,&colour,sizeof(bytes));
        Check(bytes.a==255,"both road-rule sign colours stay opaque");
        Check(bytes.r==(i==0?230:0) && bytes.g==(i==0?230:0) && bytes.b==(i==0?230:0),
              "both sign glyph RGB colours retain channel order");
    }
    std::printf("PlaytestFullMapText: %u checks, %u failures\n",guChecks,guFailures);
    return guFailures?1:0;
}
