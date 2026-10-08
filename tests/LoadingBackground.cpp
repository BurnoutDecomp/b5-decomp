// Capture vertices emitted by the production loading-screen Render body.
// Opaque alpha must hold through black, the autosave dim level, and hide.
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <vector>
using u8=uint8_t;using u32=uint32_t;using s32=int32_t;using f32=float;
namespace renderengine { enum PrimitiveType{Strip=6}; }
namespace CgsGraphics {
struct Vector2{float x,y;};struct RGBA8{u8 r,g,b,a;};struct BlendState{};
struct Basic2dColouredTexturedVertex{Vector2 mv2Pos,mv2Tex0UV;RGBA8 mv4Colour;};
struct Im2dTransform{void TransformByAspectRatio(){}};
struct Im2d{
    std::vector<RGBA8> colours;
    void BeginRendering(){}void EndRendering(){}void SetTransform(const Im2dTransform&){}
    void SetState(const BlendState*){}void SetTexture(void*){}
    void Render(renderengine::PrimitiveType,const Basic2dColouredTexturedVertex* v,u32){colours.push_back(v->mv4Colour);}
};
}
namespace BrnGame { struct LoadingScreenRenderer{
    enum{E_LOADINGLANGUAGE_COUNT=6};
    bool mbVisible=true,mbHiding=false,mbRenderInBackground=false;
    float mfTimeStep=0,mfFade=0,mfRotateSpeedInterp=0,mfArrowRotation=0;
    int meLanguage=0;void *mpCarTexture=nullptr,*mpArrowTexture=nullptr,*mpTextTexture=nullptr;
    void Render(CgsGraphics::Im2d*);
}; }
#include "loading_constants.inc"
namespace BrnGame {
#include "loading_render.inc"
}
int main(){
    unsigned checks=0,failures=0;
    auto expect=[&](bool ok,const char* msg){++checks;if(!ok){++failures;std::printf("FAIL %s\n",msg);}};
    BrnGame::LoadingScreenRenderer loading; CgsGraphics::Im2d im;
    for(float level:{0.f,.175f,.5f,1.f}){
        loading.mfFade=level;im.colours.clear();loading.Render(&im);
        expect(!im.colours.empty(),"loading background submitted");
        expect(im.colours.front().a==255,"background blocks previous scene at every brightness");
    }
    loading.mbRenderInBackground=true;loading.mfFade=.175f;im.colours.clear();loading.Render(&im);
    expect(im.colours.front().r==89&&im.colours.front().a==255,"autosave background is dim and opaque");
    loading.mbRenderInBackground=false;loading.mbHiding=true;loading.mfFade=.2f;loading.mfTimeStep=.1f;
    im.colours.clear();loading.Render(&im);
    expect(im.colours.front().r==0&&im.colours.front().a==255,"hide reaches opaque black");
    std::printf("LoadingBackground: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
