// Real RendererIO/WorldDispatch/EffectsIO camera copies and original timer bridge.
// Peripheral owners are isolated; no GPU or game is created.
#include <cstdio>
#include <cstring>
#include <memory>
#include <type_traits>
#include <initializer_list>
#include "GameSource/Graphics/BrnRendererModuleIO.h"
#include "GameSource/World/BrnWorldModuleIO_DispatchInputBuffer.h"
#include "GameSource/Effects/SharedIO/BrnEffectsModuleIO_DispatchInputBuffer.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsTextureScopeTable.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "SharedClasses/BrnSharedConstants.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h"
static int checks,failures,asserts;
static void Check(bool ok,const char* label) {
    ++checks; if(!ok && failures++<16) std::printf("FAIL %s\n",label);
}
namespace CgsDev { namespace Log { DebugPrint* gpDebugPrint=nullptr; }
namespace Assert {
int BeginAssert(){return 0;}
int FireAssert(const char*,const char*,int){++asserts;return 0;}
void* EndAssert(){return nullptr;}
} }
namespace BrnDirector {
void DebugPrinter::ActualPrint(const char*,u32) {}
void DebugLog::ActualAppend(const char*,u32) {}
}
namespace BrnDirector { namespace DirectorIO {
struct OutputBuffer: CgsModule::IOBuffer {
    BrnDirector::Camera::Camera camera;
    const BrnDirector::Camera::Camera* GetCameraOutput() const {return &camera;}
};
} }
namespace BrnGame {
class BrnGameModule {
public:
    struct TimerOracle {
        int ticks=0;float fraction=0;
        int GetAccumTicks() const{return ticks;}
        float GetAccumulator() const{return fraction;}
    } mGameTimer,mSimTimer;
    BrnDirector::DirectorIO::OutputBuffer* mpDirectorOutputBuffer=nullptr;
    BrnDirector::Camera::Camera mDispatchCamera;
    bool mbDispatchCameraPublished=false,mbStalled=false,mbSimPaused=false,mbSteppingFrames=false;
    struct GuiRenderObserver {
        unsigned renders = 0;
        void* GetViewInputBuffer() { return this; }
        void Render(void*, void*, RendererIO::OutputBuffer*) { ++renders; }
    } mGuiModule;
    struct Prepared {bool prepared=true;bool IsPrepared() const{return prepared;}} mDirectorModule;
    struct Manager {
        DispatchThreadInputBuffer* buffer=nullptr;
        DispatchThreadInputBuffer* GetWriteBuffer(){return buffer;}
    } mDispatchThreadInputBufferManager;
    struct EffectsOwner {int preCalls=0;void PreRenderUpdate(DispatchThreadInputBuffer*){++preCalls;}} mEffectsModule;
    BrnUpdateSet updateSet=0x80;
    int dispatches=0,updates=0;
    BrnUpdateSet ConstructUpdateSetFromFsm(){return updateSet;}
    void BridgeRendererToWorld(BrnWorldIO::DispatchInputBuffer*,RendererIO::OutputBuffer*);
    void BridgeRendererToEffects(BrnEffects::EffectsIO::DispatchInputBuffer*,RendererIO::OutputBuffer*);
    void LatchDispatchCamera();
    const BrnDirector::Camera::Camera* GetDispatchCamera() const;
    int DoDispatch() {
#include "dispatch_lifecycle.inc"
        ++dispatches;return 0;
    }
    void RenderGuiForFixture(bool hasInput, RendererIO::OutputBuffer* lpRendererOutput) {
        void* const lpGuiRenderInput = hasInput ? this : nullptr;
#include "dispatch_gui_gate.inc"
    }
    void DoUpdate_Director(bool lbPostGui) {
#include "director_lifecycle.inc"
        ++updates;
    }
};
static BrnGameModule* gpFixture;
BrnGameModule* GetMainGameModule(){return gpFixture;}
}
namespace BrnGameMainFlowController {
static int gBrnScriptedLoadStage;
struct LoadingScriptedState { virtual void Render(); };
struct MainGameFlowStateInitialLoadingScreen : LoadingScriptedState {};
struct MainGameFlowStateStartScreen : LoadingScriptedState { void Render() override; };
struct MainGameFlowStateMarketingScreens : LoadingScriptedState { void Render() override; };
struct MainGameFlowStateCheckDiskSpace : LoadingScriptedState { void Render() override; };
struct MainGameFlowStateMemoryCard : LoadingScriptedState { void Render() override; };
struct MainGameFlowStateCompleteLoading : LoadingScriptedState { void Render() override; };
struct MainGameFlowStateInGame : LoadingScriptedState { void Render() override; };
}
struct WorldConsumer {
    bool mbIsInJunkyard=false;
    struct Lod {void Update(){}} mShaderLodInfo;
    struct Environment {void EnableJunkyardLightingSetup(){} void DisableJunkyardLightingSetup(){}} mEnvironmentManager;
    BrnDirector::Camera::Camera mLastCameraInput;
    void Consume(BrnWorldIO::DispatchInputBuffer* lpDispatchInputBuffer,const BrnUpdateSet* lpUpdateSet) {
        void* lpInputBufferStack=this;void* lpOutputBufferStack=this;
#include "world_camera_latch.inc"
        lpDispatchThreadInputBuffer->UnlockForWrite();
        lpDispatchInputBuffer->UnlockForRead();
    }
};
#include "original_camera_chain.inc"

int main() {
    auto dispatch=std::make_unique<BrnGame::DispatchThreadInputBuffer>();
    dispatch->CgsModule::IOBuffer::Construct();dispatch->SetIsWriteBuffer(true);
    BrnGame::BrnGameModule game;BrnGame::gpFixture=&game;
    // Normalize padding in the fixture only; the original memberwise camera
    // assignment has no obligation to copy otherwise unobservable padding.
    std::memset(&game.mDispatchCamera,0,sizeof(game.mDispatchCamera));
    game.mDispatchThreadInputBufferManager.buffer=dispatch.get();
    BrnDirector::DirectorIO::OutputBuffer director;director.Construct();
    std::memset(&director.camera,0xA5,sizeof(director.camera));director.camera.Construct();
    Check(!director.camera.GetEffects().mbRequestingScreenshot,
          "original screenshot request byte defaults false");
    Check(director.camera.GetEffects().maReservedBB[0]==0xA5,
          "recovering the screenshot flag preserves original trailing padding");
    game.mpDirectorOutputBuffer=&director;
    Check(game.GetDispatchCamera()==nullptr,"constructed director storage is not a completed update");
    RendererIO::InputBuffer rendererInput; RendererIO::OutputBuffer rendererOutput;
    for (bool stepping : {false, true}) for (bool hasInput : {false, true}) {
        game.mbSteppingFrames = stepping;
        const unsigned before = game.mGuiModule.renders;
        game.RenderGuiForFixture(hasInput, &rendererOutput);
        Check(game.mGuiModule.renders - before == unsigned(!stepping && hasInput),
              "original frame-stepping gate suppresses GUI/movie/effect Render");
    }
    game.mbSteppingFrames = false;
    BrnWorldIO::DispatchInputBuffer world; BrnEffects::EffectsIO::DispatchInputBuffer effects;
    std::memset(&rendererInput,0,sizeof(rendererInput));std::memset(&rendererOutput,0,sizeof(rendererOutput));
    std::memset(&world,0,sizeof(world));std::memset(&effects,0,sizeof(effects));
    rendererInput.CgsModule::IOBuffer::Construct();rendererOutput.CgsModule::IOBuffer::Construct();
    world.CgsModule::IOBuffer::Construct();effects.CgsModule::IOBuffer::Construct();
    game.mGameTimer={37,0.625f};game.mSimTimer={-2,0.25f};
    WorldConsumer consumer;
    std::memset(&consumer.mLastCameraInput,0,sizeof(consumer.mLastCameraInput));
    // Include a stationary origin camera, authored roll/up, unusual FOV, all
    // effects/DOF metadata and widened shot pointers. No arbitrator exists here.
    for(unsigned sample=0;sample<32;++sample) {
        auto& camera=director.camera;std::memset(&camera,0,sizeof(camera));
        camera.mTransform.xAxis={0.8f,0.6f,0,0};camera.mTransform.yAxis={-0.6f,0.8f,0,0};
        camera.mTransform.zAxis={0,0,1,0};camera.mTransform.wAxis={float(sample),0,0,1};
        camera.mfFOV=sample==0?0.05f:163.0f;camera.mfAspectRatio=1.25f;
        camera.mfCustomNearClipDistance=0.035f;camera.mbHasCustomNearClipDistance=true;
        camera.mState_uFlags=0x04800010;camera.mfRunningTime=5.75f;
        camera.mEffects.mfBloomLuminance=0.21f;camera.mEffects.mfTimeOfDay=16.5f;
        camera.mEffects.mbSetTimeOfDay=true;camera.mEffects.mMotionBlurData.mfWorldBlurAmount=0.45f;
        camera.mEffects.mbRequestingScreenshot=(sample&1)!=0;
        camera.mDepthOfField.mfBlurriness=0.7f;
        camera.mpSourceShot=reinterpret_cast<decltype(camera.mpSourceShot)>(uintptr_t(0x1234567890ull));
        camera.mpDebugInfoBehaviour=reinterpret_cast<const BrnDirector::Camera::Behaviour*>(uintptr_t(0x2345678901ull));
        game.LatchDispatchCamera();
        const auto* completed=game.GetDispatchCamera();
        Check(completed && std::memcmp(completed,&camera,sizeof(camera))==0,"completed output retained whole");
        rendererInput.LockForWrite();rendererInput.SetBrnCamera(*completed);rendererInput.UnlockForWrite();
        rendererInput.LockForRead();rendererOutput.LockForWrite();
        rendererOutput.SetBrnCamera(rendererInput.GetBrnCamera());
        rendererOutput.UnlockForWrite();rendererInput.UnlockForRead();
        rendererOutput.LockForRead();world.LockForWrite();
        game.BridgeRendererToWorld(&world,&rendererOutput);
        world.SetDispatchThreadInputBuffer(dispatch.get());world.UnlockForWrite();
        effects.LockForWrite();
#include "bridge_effects_call.inc"
        effects.SetCameraInput(completed);effects.UnlockForWrite();
        rendererOutput.UnlockForRead();
        world.LockForRead();effects.LockForRead();
        Check(std::memcmp(world.GetCameraInput(),completed,sizeof(camera))==0,"world receives every camera field at origin/paused/stationary poses");
        Check(std::memcmp(effects.GetCameraInput(),completed,sizeof(camera))==0,"effects receives the same completed camera");
        Check(world.GetGameTime()==37.625f && world.GetSimTime()==-1.75f,"original whole plus fractional game/sim timers");
        world.UnlockForRead();effects.UnlockForRead();
        for(BrnUpdateSet set:{BrnUpdateSet(0),BrnUpdateSet(0x80)}) {
            consumer.Consume(&world,&set);
            Check(std::memcmp(&consumer.mLastCameraInput,completed,sizeof(camera))==0,"real world latch runs before the render bit gate");
        }
        const auto* held=game.GetDispatchCamera();
        Check(held==completed && std::memcmp(held,&camera,sizeof(camera))==0,"zero-substep renders hold the completed output without blending");
    }
    std::memset(&director.camera,0,sizeof(director.camera));game.LatchDispatchCamera();
    Check(std::memcmp(game.GetDispatchCamera(),&director.camera,sizeof(director.camera))==0,
          "completed default camera replaces an earlier output without setter/pose selection");
    for(bool pause:{false,true}) for(bool stall:{false,true}) {
        game.mbSimPaused=pause;game.mbStalled=stall;const int before=game.dispatches;game.DoDispatch();
        Check(game.dispatches==before+!stall,"only the true resource stall suppresses production");
        dispatch->LockForRead();Check(dispatch->GetIsStalled()==stall,"original stall byte is always published");dispatch->UnlockForRead();
    }
    game.mbStalled=false;
    using namespace BrnGameMainFlowController;
    MainGameFlowStateInitialLoadingScreen initial;
    MainGameFlowStateStartScreen start;
    MainGameFlowStateMarketingScreens marketing;
    MainGameFlowStateCheckDiskSpace disk;
    MainGameFlowStateMemoryCard card;
    MainGameFlowStateCompleteLoading complete;
    MainGameFlowStateInGame ingame;
    for(int stage=0;stage<=8;++stage) {
        gBrnScriptedLoadStage=stage;
        for (LoadingScriptedState* flow : std::initializer_list<LoadingScriptedState*>{&initial, &start,
                 &marketing, &disk, &card, &complete}) {
            const int before=game.dispatches;
            flow->Render();
            Check(game.dispatches==before+(stage==8),"all six loading virtual slots dispatch exactly once only at original stage8");
        }
        const int before=game.dispatches; ingame.Render();
        Check(game.dispatches==before+1,"in-game virtual Render dispatches once independently of scripted stage");
    }
    for(bool post:{false,true}) for(bool prepared:{false,true}) for(bool video:{false,true}) {
        game.mDirectorModule.prepared=prepared;game.updateSet=video?0x20:0;const int before=game.updates;
        game.DoUpdate_Director(post);
        Check(game.updates==before+(prepared&&(post||!video)),"boot video skips only the original pre-GUI camera update");
    }
    auto& registry=CgsGraphics::gTextureScopeTable;
    Check(std::strcmp(registry.maEntries[4].mpcName,"environmentMap")==0 && registry.maEntries[4].miScope==2,
          "original texture purpose4 registration and scope");
    void* texture=reinterpret_cast<void*>(uintptr_t(0x12345000));
    registry.maEntries[4].mClearState=7;registry.SetTexture(CgsGraphics::E_TEXTURE_PURPOSE_ENVIRONMENT_MAP,texture);
    Check(registry.GetTexture(CgsGraphics::E_TEXTURE_PURPOSE_ENVIRONMENT_MAP)==texture && registry.maEntries[4].mClearState==0,
          "original texture replacement invalidates cached scope state");
    registry.maEntries[4].mClearState=7;registry.SetTexture(CgsGraphics::E_TEXTURE_PURPOSE_ENVIRONMENT_MAP,texture);
    Check(registry.maEntries[4].mClearState==7,"unchanged texture preserves cache state");
    Check(asserts==0,"all real accessors satisfy original read/write locks");
    std::printf("PCOriginalCameraChain: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
