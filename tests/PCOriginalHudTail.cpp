// Original8240E000..E054 order, executing the actual production gate/block.
#include <cstdio>
#include <vector>
static int checks,failures;
static std::vector<int> events;
static void Check(bool ok,const char* label){++checks;if(!ok){++failures;std::printf("FAIL %s\n",label);}}
struct Loading {
    void RenderBackground(void*){events.push_back(1);}
    void RenderForeground(void*){events.push_back(4);}
};
struct Commands {int id;void Dispatch(void*){events.push_back(id);}};
struct BrnRendererModule {
    bool mbRenderHudImmediateMode=false,mbIm3dRendererConstructedPC=false;
    Loading mLoadingScreenRenderer;
    Commands mIm2dRenderBuffer{2},mIm3dBufferMenusAndHud{3};
    int mIm2dRenderer=0,mIm3dRenderer=0;
    void HudTail() {
#include "original_hud_tail.inc"
    }
};
int main(){
    for(bool hud:{false,true})for(bool native3d:{false,true}){
        BrnRendererModule renderer;renderer.mbRenderHudImmediateMode=hud;
        renderer.mbIm3dRendererConstructedPC=native3d;events.clear();renderer.HudTail();
        const std::vector<int> expected=!hud?std::vector<int>{}:
            native3d?std::vector<int>{1,2,3,4}:std::vector<int>{1,2,4};
        Check(events==expected,"original HUD gate and background/Im2d/HUD3D/foreground order");
        int mainDispatches=0;for(int item:events)mainDispatches+=item==2;
        Check(mainDispatches==(hud?1:0),"one renderer-owned Im2d dispatch including movie commands");
    }
    std::printf("PCOriginalHudTail: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
