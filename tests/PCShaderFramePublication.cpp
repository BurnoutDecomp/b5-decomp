// Original renderer banks are filled directly by the world producer.
// Peripheral owners are isolated to test simultaneous bank writes and reads.
#include <atomic>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
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
struct BrnSkyCameraBringUp { Matrix44 mViewProjection; Vector3 mViewPosition; bool mbValid; };
BrnSkyCameraBringUp gBrnSkyCameraBringUp = {};
static bool sbEffectsArbitratorConstructed = true;
struct AdjacentBuffer {
    unsigned flips = 0;
    bool IsPreparedPC() const { return true; }
    void Swap() { ++flips; }
    void Clear() {}
    void EndOfFrame() { ++flips; }
};
namespace CgsGraphics { using Im3dRenderBuffer = ::AdjacentBuffer; }
struct BrnRendererModule {
    AdjacentBuffer mIm2dRenderBuffer, mEffectsArbitrator, mCoronaManager, mDoubleBufferedDispatchFrame;
    AdjacentBuffer mIm3dRenderBuffer, mIm3dDebugRenderBuffer, mIm3dBufferRacePosition, mIm3dBufferMenusAndHud;
    AdjacentBuffer mIm3dRenderBufferUntex, mIm2dDebugRenderBuffer;
    struct { struct { int miNumShadows = 0; } maBuffers[2]; u8 mu8Internal = 0, mu8External = 1; } mBlobbyShadowManager;
    u64 muCommandGenerationPC = 0, muCompletedCommandGenerationPC = 0, muPublishedCommandGenerationPC = 0;
    unsigned mu8PCMovieWriteFrame = 0;
    void* mpInterpreter = this;
    BrnShaderConstantsFrame maShaderConstantsFrames[2];
    bool maShaderConstantsFrameValidPC[2] = {};
    unsigned mu8ShaderConstantsFrameInternal = 0, mu8ShaderConstantsFrameExternal = 0;
    void PublishMeshFramePC() {} // Adjacent mesh publication is tested separately.
    void SwapBuffers();
    void PublishSkyConstantsBringUp(BrnShaderConstantsFrame*);
    void ConstructShaderFramesForFixture() {
#include "shader_construct.inc"
    }
    void CompleteWorldDispatchFramePC(bool);
    const BrnShaderConstantsFrame* GetPublishedShaderConstantsFramePC() const;
};
#include "shader_publication.inc"

static void Produce(BrnRendererModule& renderer, float value) {
    auto& p = renderer.maShaderConstantsFrames[renderer.mu8ShaderConstantsFrameExternal];
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
    Matrix44 view = {}; view.xAxis.x = value;
    p.SetViewProjectionMatrix(view); p.SetViewPosition(light);
    renderer.CompleteWorldDispatchFramePC(true);
    // This fixture's producer completed; the separate lifetime regression
    // exercises admission and empty native presentations from production code.
    renderer.muCompletedCommandGenerationPC = ++renderer.muCommandGenerationPC;
}
int main() {
    BrnRendererModule renderer;
    renderer.ConstructShaderFramesForFixture();
    gBrnWorldShaderConstantsFrameBringUp.Construct();
    Check(renderer.mu8ShaderConstantsFrameInternal==0 && renderer.mu8ShaderConstantsFrameExternal==1,
          "original Construct uses separate readable/writable banks");
    Check(!renderer.maShaderConstantsFrames[0].mbLockedForWriting
          && renderer.maShaderConstantsFrames[1].mbLockedForWriting,
          "original Construct opens only the external write bank");
    renderer.SwapBuffers();
    Check(!renderer.maShaderConstantsFrameValidPC[renderer.mu8ShaderConstantsFrameInternal],
          "boot does not publish absent world data");
    Check(!renderer.maShaderConstantsFrames[renderer.mu8ShaderConstantsFrameInternal].mbLockedForWriting
          && renderer.maShaderConstantsFrames[renderer.mu8ShaderConstantsFrameExternal].mbLockedForWriting,
          "Swap closes internal and opens the following external bank");
    Produce(renderer,7.0f);
    renderer.SwapBuffers();
    const auto& frozen = renderer.maShaderConstantsFrames[renderer.mu8ShaderConstantsFrameInternal];
    Check(renderer.GetPublishedShaderConstantsFramePC()==&frozen,
          "native observer reads the actual published internal bank");
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
    std::thread producer([&renderer] { for (unsigned n=0;n<20000;++n) Produce(renderer,static_cast<float>(n)); });
    bool stable = true;
    for (unsigned n=0;n<20000;++n)
        stable &= frozen.GetKeyLightDirection().x == 7.0f && frozen.GetCloudDarkColour0().x == 14.0f;
    producer.join();
    Check(stable,"next-frame writes cannot alter the dispatched frame");
    renderer.SwapBuffers();
    Check(renderer.maShaderConstantsFrames[renderer.mu8ShaderConstantsFrameInternal].GetWhiteLevel() == 19999.0f,
          "join and swap publish the new completed frame");
    Check(!renderer.maShaderConstantsFrames[renderer.mu8ShaderConstantsFrameInternal].mbLockedForWriting
          && renderer.maShaderConstantsFrames[renderer.mu8ShaderConstantsFrameExternal].mbLockedForWriting,
          "second rotation retains the original read/write discipline");
    renderer.CompleteWorldDispatchFramePC(false);
    renderer.muCompletedCommandGenerationPC = ++renderer.muCommandGenerationPC;
    renderer.SwapBuffers();
    Check(!renderer.maShaderConstantsFrameValidPC[renderer.mu8ShaderConstantsFrameInternal],
          "suppressed world producer does not fabricate a sky frame");
    Check(asserts == 0,"no accessor observes a write-locked frame");
    std::printf("PCShaderFramePublication: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
