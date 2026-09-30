// Real shading-frame accessors and production publication bodies; surrounding
// renderer owners are isolated so concurrent producer writes can be exercised.
#include <atomic>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "GameSource/Graphics/BrnShaderConstantsFrame.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
static int checks, failures, asserts;
static void Check(bool ok, const char* why) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s\n",why); }
}
namespace CgsDev {
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++asserts; return 0; }
void* EndAssert() { return nullptr; }
}
}
namespace renderengine { u32 guPresentCount = 0; }
BrnShaderConstantsFrame gBrnWorldShaderConstantsFrameBringUp;
bool gbBrnWorldShaderConstantsFrameBringUpValid = false;
BrnSkyCameraBringUp gBrnSkyCameraBringUp = {};
static bool sbEffectsArbitratorConstructed = true;
struct AdjacentBuffer {
    unsigned flips = 0;
    bool IsPreparedPC() const { return true; }
    void Swap() { ++flips; }
    void Clear() {}
    void EndOfFrame() { ++flips; }
};
struct BrnRendererModule {
    AdjacentBuffer mIm2dRenderBuffer, mEffectsArbitrator, mCoronaManager, mDoubleBufferedDispatchFrame;
    unsigned mu8PCMovieWriteFrame = 0;
    void* mpInterpreter = this;
    BrnShaderConstantsFrame maShaderConstantsFrames[2];
    bool maShaderConstantsFrameValidPC[2] = {};
    unsigned mu8ShaderConstantsFrameInternal = 0, mu8ShaderConstantsFrameExternal = 0;
    void SwapBuffers();
    void PublishSkyConstantsBringUp(BrnShaderConstantsFrame*);
};
#include "shader_publication.inc"

static void Produce(float value) {
    auto& p = gBrnWorldShaderConstantsFrameBringUp;
    p.LockForWriting();
    p.SetWhiteLevel(value);
    Vector3 light = {}; light.x = value; light.y = value + 1; light.z = value + 2;
    p.SetKeyLightDirection(light);
    p.SetKeyLightColour(light);
    p.SetEnvMapViewPosition(light);
    Vector4 cloud = {}; cloud.x = value * 2; cloud.w = value * 3;
    p.SetCloudDarkColour0(cloud);
    for (unsigned face=0;face<6;++face) {
        Matrix44 matrix = {}; matrix.xAxis.x = value + face;
        p.SetEnvMapViewProjectionMatrix(static_cast<BrnGraphics::EEnvironmentMapFace>(face),matrix);
    }
    p.UnlockForWriting();
    gBrnSkyCameraBringUp.mViewPosition = light;
    gBrnSkyCameraBringUp.mViewProjection.xAxis.x = value;
    gBrnSkyCameraBringUp.mbValid = true;
    gbBrnWorldShaderConstantsFrameBringUpValid = true;
}
int main() {
    BrnRendererModule renderer;
    for (auto& f : renderer.maShaderConstantsFrames) f.Construct();
    gBrnWorldShaderConstantsFrameBringUp.Construct();
    renderer.SwapBuffers();
    Check(!renderer.maShaderConstantsFrameValidPC[renderer.mu8ShaderConstantsFrameInternal],
          "boot does not publish absent world data");
    Produce(7.0f);
    renderer.SwapBuffers();
    const auto& frozen = renderer.maShaderConstantsFrames[renderer.mu8ShaderConstantsFrameInternal];
    Check(renderer.maShaderConstantsFrameValidPC[renderer.mu8ShaderConstantsFrameInternal],"completed world frame is valid");
    Check(frozen.GetWhiteLevel() == 7.0f,"white level comes from the completed update");
    Check(frozen.GetViewProjectionMatrix().xAxis.x == 7.0f && frozen.GetViewPosition().y == 8.0f,
          "camera and lighting publish together");
    Check(frozen.GetCloudDarkColour0().x == 14.0f && frozen.GetKeyLightDirection().z == 9.0f,
          "clouds and key light retain their values");
    bool faces = true;
    for (unsigned face=0;face<6;++face)
        faces &= frozen.GetEnvMapViewProjectionMatrix(static_cast<BrnGraphics::EEnvironmentMapFace>(face)).xAxis.x == 7.0f + face;
    Check(faces && frozen.GetEnvMapViewPosition().x == 7.0f,"all reflection faces use that frame");
    std::thread producer([] { for (unsigned n=0;n<20000;++n) Produce(static_cast<float>(n)); });
    bool stable = true;
    for (unsigned n=0;n<20000;++n)
        stable &= frozen.GetKeyLightDirection().x == 7.0f && frozen.GetCloudDarkColour0().x == 14.0f;
    producer.join();
    Check(stable,"next-frame writes cannot alter the dispatched frame");
    renderer.SwapBuffers();
    Check(renderer.maShaderConstantsFrames[renderer.mu8ShaderConstantsFrameInternal].GetWhiteLevel() == 19999.0f,
          "join and swap publish the new completed frame");
    Check(asserts == 0,"no accessor observes a write-locked frame");
    std::printf("PCShaderFramePublication: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
