// Production GUI producer bodies with real typed IO/camera semantics. View, movie
// player and effects are observed at their subsystem boundaries; no GPU is used.
#include <cstdio>
#include <cstring>
#include <cstdint>
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModuleIO.h"
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameSource/Gui/BrnGuiPerfmons.h"

namespace RendererIO { struct OutputBuffer; }
static unsigned checks, failures, asserts;
static unsigned trace[32], traceLength;
static void Check(bool good, const char* why)
{
    ++checks;
    if (!good) { ++failures; std::printf("FAIL %s\n", why); }
}
static void Record(unsigned operation) { trace[traceLength++] = operation; }
static void ResetTrace() { traceLength = 0; }
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++asserts; return 0; }
void* EndAssert() { return nullptr; }
} namespace PerfMonCpu {
void StartMonitor(s32 handle) { Record(10 + handle); }
void StopMonitor(s32 handle) { Record(20 + handle); }
} }
int32_t BrnGui::GuiPerfmons::miGuiModuleRender = 1;
int32_t BrnGui::GuiPerfmons::miGuiRender = 2;

using ModuleInput = CgsGui::CgsGuiModuleIO::InputBuffer;
using ViewInput = CgsGui::ViewIO::InputBuffer;
using CgsGui::ImRendererSet;
using CgsGraphics::Camera;
static ModuleInput* expectedSource;
static ViewInput* expectedView;

static bool SameCamera(const Camera& a, const Camera& b)
{
    return std::memcmp(&a.mView, &b.mView, sizeof(Matrix44)) == 0
        && std::memcmp(&a.mProjection, &b.mProjection, sizeof(Matrix44)) == 0
        && std::memcmp(&a.mViewProjection, &b.mViewProjection, sizeof(Matrix44)) == 0
        && std::memcmp(a.maClipState, b.maClipState, sizeof(a.maClipState)) == 0
        && std::memcmp(a.maProjectionScalars, b.maProjectionScalars, sizeof(a.maProjectionScalars)) == 0;
}
static void SeedCamera(Camera& camera, float seed)
{
    std::memset(&camera, 0, sizeof(camera));
    camera.mView.wAxis.x = seed;
    camera.mProjection.xAxis.x = seed + 1;
    camera.mViewProjection.zAxis.w = seed + 2;
    for (unsigned i = 0; i < 16; ++i) camera.maClipState[i] = seed + 10000.0 + i * 0.125;
    for (unsigned i = 0; i < 9; ++i) camera.maProjectionScalars[i] = seed + 200 + i;
}
static void SeedPointers(ImRendererSet& set, uintptr_t base)
{
    set.mpIm2dRenderBuffer = reinterpret_cast<decltype(set.mpIm2dRenderBuffer)>(base + 0x101);
    set.mpIm3dRenderBuffer = reinterpret_cast<decltype(set.mpIm3dRenderBuffer)>(base + 0x202);
    set.mpIm3dRenderBufferUntex = reinterpret_cast<decltype(set.mpIm3dRenderBufferUntex)>(base + 0x303);
    set.mpIm3dRenderBufferRacePosition = reinterpret_cast<decltype(set.mpIm3dRenderBufferRacePosition)>(base + 0x404);
    set.mpIm3dRenderBufferMenusAndHud = reinterpret_cast<decltype(set.mpIm3dRenderBufferMenusAndHud)>(base + 0x505);
}
static void CheckSet(const ImRendererSet& actual, const ImRendererSet& expected)
{
    Check(actual.mpIm2dRenderBuffer == expected.mpIm2dRenderBuffer, "forward the supplied Im2d owner");
    Check(actual.mpIm3dRenderBuffer == expected.mpIm3dRenderBuffer, "forward the supplied Im3d owner");
    Check(actual.mpIm3dRenderBufferUntex == expected.mpIm3dRenderBufferUntex, "forward the supplied untextured owner");
    Check(actual.mpIm3dRenderBufferRacePosition == expected.mpIm3dRenderBufferRacePosition, "forward the supplied race-position owner");
    Check(actual.mpIm3dRenderBufferMenusAndHud == expected.mpIm3dRenderBufferMenusAndHud, "forward the supplied menus/HUD owner");
    Check(SameCamera(actual.mCamera, expected.mCamera), "forward the complete original GUI camera");
}
struct ObservedView
{
    ImRendererSet seen;
    unsigned renders = 0;
    void Render(ViewInput* input)
    {
        Record(30);
        ++renders;
        Check(input == expectedView, "render the actual completed view producer");
        Check(!input->IsBufferLocked() && !expectedSource->IsBufferLocked(),
              "release both forwarding locks before ViewModule::Render");
        input->LockForRead();
        seen = input->GetImRenderers();
        input->UnlockForRead();
    }
};
struct ObservedMoviePlayer
{
    CgsGraphics::Im2dRenderBuffer* seen = nullptr;
    unsigned renders = 0;
    void Render(CgsGraphics::Im2dRenderBuffer* input)
    {
        Record(50);
        ++renders;
        seen = input;
        Check(expectedView->IsBufferLockedForReading(), "movie player reads the same supplied view under read lock");
    }
};
struct ObservedEffects
{
    RendererIO::OutputBuffer* seen = nullptr;
    void GenerateEffectFrameEvents(RendererIO::OutputBuffer* output) { Record(60); seen = output; }
};
namespace BrnGui {
class MovieManager
{
public:
    enum State { E_MOVIEMANAGERSTATE_IDLE, E_MOVIEMANAGERSTATE_PLAYING_MOVIE, E_MOVIEMANAGERSTATE_STOP_MOVIE };
    State meState = E_MOVIEMANAGERSTATE_IDLE;
    ObservedMoviePlayer mMoviePlayer;
    void Update() { Record(40); }
    void Render(CgsGraphics::Im2dRenderBuffer*);
};
class GuiModule
{
public:
    bool mbPrepared = false;
    ObservedView mViewModule;
    MovieManager mMovieManager;
    ObservedEffects mEffectsArbitrator;
    ViewInput mViewInputBuffer;
    ModuleInput mCompletedRenderInputPC;
    bool mbRenderInputCompletedPC = false;
    void Render(ViewInput*, ModuleInput*, RendererIO::OutputBuffer*);
    void UpdateAndRenderMovieManager(ViewInput*);
    void CaptureRenderInputPC(const ModuleInput*);
    ModuleInput* GetCompletedRenderInputPC();
};
}
#include "gui_original_producer.inc"

static void CheckTrace()
{
    const unsigned expected[] = {11, 12, 30, 22, 40, 50, 60, 21};
    Check(traceLength == 8 && std::memcmp(trace, expected, sizeof(expected)) == 0,
          "original render, movie and effects operations retain their order");
}
int main()
{
    BrnGui::GuiModule gui;
    gui.mCompletedRenderInputPC.CgsModule::IOBuffer::Construct();
    gui.mViewInputBuffer.CgsModule::IOBuffer::Construct();
    ModuleInput input;
    input.CgsModule::IOBuffer::Construct();
    ImRendererSet supplied;
    SeedPointers(supplied, UINT64_C(0x1234567800000000));
    SeedCamera(supplied.mCamera, 17.5f);
    SeedPointers(gui.mViewInputBuffer.mRendererSet, UINT64_C(0x00007FF800000000));
    SeedCamera(gui.mViewInputBuffer.mRendererSet.mCamera, -400.0f);
    input.LockForWrite();
    input.SetCamera(supplied.mCamera);
    input.SetImRenderers(supplied);
    input.UnlockForWrite();
    expectedSource = &input;
    expectedView = &gui.mViewInputBuffer;
    unsigned outputToken = 1;
    auto* output = reinterpret_cast<RendererIO::OutputBuffer*>(&outputToken);

    Check(gui.GetCompletedRenderInputPC() == nullptr, "constructed storage is not a completed input");
    ResetTrace();
    gui.Render(expectedView, &input, output);
    Check(gui.mViewModule.renders == 0 && traceLength == 2 && trace[0] == 11 && trace[1] == 21,
          "original preparation gate skips the entire pass");
    gui.mbPrepared = true;
    const BrnGui::MovieManager::State states[] = {BrnGui::MovieManager::E_MOVIEMANAGERSTATE_IDLE,
        BrnGui::MovieManager::E_MOVIEMANAGERSTATE_PLAYING_MOVIE, BrnGui::MovieManager::E_MOVIEMANAGERSTATE_STOP_MOVIE};
    for (auto state : states)
    {
        gui.mMovieManager.meState = state;
        ResetTrace();
        gui.Render(expectedView, &input, output);
        CheckSet(gui.mViewModule.seen, supplied);
        CheckTrace();
        Check(gui.mMovieManager.mMoviePlayer.seen == supplied.mpIm2dRenderBuffer,
              "idle, playing and stopped managers all forward the supplied movie buffer");
        Check(gui.mEffectsArbitrator.seen == output, "GUI effects use this dispatch's real output");
        Check(!input.IsBufferLocked() && !expectedView->IsBufferLocked(), "render leaves IO locks balanced");
    }

    gui.CaptureRenderInputPC(&input);
    auto* completed = gui.GetCompletedRenderInputPC();
    Check(completed != nullptr && completed != &input, "completed storage owns its input lifetime");
    std::memset(&input, 0x5A, sizeof(input));
    completed->LockForRead();
    CheckSet(completed->GetImRenderers(), supplied);
    completed->UnlockForRead();

    // The dispatch owner must run the original bridge again for the new renderer banks.
    // This fixture supplies its fresh set through the real module setter before Render.
    ImRendererSet fresh;
    SeedPointers(fresh, UINT64_C(0x00007FF900000000));
    SeedCamera(fresh.mCamera, -100.0f);
    completed->LockForWrite();
    completed->SetImRenderers(fresh);
    completed->UnlockForWrite();
    fresh.mCamera = supplied.mCamera; // module setter preserves the completed GUI camera
    expectedSource = completed;
    ResetTrace();
    gui.Render(expectedView, completed, output);
    CheckSet(gui.mViewModule.seen, fresh);
    CheckTrace();
    Check(gui.mMovieManager.mMoviePlayer.seen == fresh.mpIm2dRenderBuffer,
          "zero-substep render consumes refreshed renderer pointers and retained GUI camera");
    Check(!completed->IsBufferLocked() && !expectedView->IsBufferLocked(), "retained-input locks balance");

    gui.mbPrepared = false;
    const unsigned before = asserts;
    ResetTrace();
    gui.Render(expectedView, nullptr, nullptr);
    Check(asserts == before + 2, "original input/output assertions precede the preparation gate");
    Check(asserts == 2, "valid producer inputs satisfy every real IO assertion");
    std::printf("GuiOriginalProducer: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
