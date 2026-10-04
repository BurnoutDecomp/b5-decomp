// Real preparation/publication methods. The conversion boundary emits commands
// retaining input-bank constant pointers, just as the real mesh emitter does.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#include "pc/gcm/renderengine/FrameProfilePCLeaf.h"
#include "pc/gcm/renderengine/MeshPreparationPCLeaf.h"
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using f32 = float;
#define CGS_ASSERT(ok, message) do { if (!(ok)) std::abort(); } while (0)
static unsigned checks, failures, conversions, sorts, materialVersion = 1, resetWhileSorting = 0;
static void Check(bool ok, const char* name)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s\n", name); }
}
namespace CgsGraphics {
struct DispatchObjectContext {
    void* mapConstantData[64];
    int miListIdBase;
    bool mbPreZEnabled, mbPreZAlphaEnabled;
    float mvPreZDistanceThreshold[4];
    void ResetShadowing() { std::memset(mapConstantData, 0, sizeof(mapConstantData)); }
};
struct Mesh { int id; const float* constant; unsigned material; };
struct DispatchFrame {
    float constant = 0;
    bool* pendingSort = nullptr;
    std::vector<int> objects;
    std::vector<Mesh> meshes;
    void Reset() { if (pendingSort && *pendingSort) ++resetWhileSorting; objects.clear(); meshes.clear(); }
};
struct BufferedDispatchFrame {
    DispatchFrame frames[2];
    unsigned read = 1;
    DispatchFrame& GetDispatchFrameForRead() { return frames[read]; }
    DispatchFrame& GetDispatchFrameForWrite() { return frames[1u - read]; }
    void Swap() { read = 1u - read; }
};
struct DispatchPacketInterpreter {
    DispatchFrame* frame = nullptr;
    float time = -1;
    void SetSingleBufferedDispatchFrame(DispatchFrame* p) { frame = p; }
    void SetTime(float value) { time = value; }
};
}
// Sorting is the asynchronous boundary in this fixture. Native worker execution
// and joins are covered by PCDispatchSortJobs; here a pending marker catches
// resets before the production preparation code joins its bank.
namespace renderengine {
struct DispatchSortJobsPC {
    bool pending = false;
    void WaitAll() { pending = false; }
};
}
struct BrnRendererModule {
    CgsGraphics::DispatchFrame mSingleBufferedDispatchFrame, mSecondMeshFramePC;
    CgsGraphics::BufferedDispatchFrame mDoubleBufferedDispatchFrame;
    CgsGraphics::DispatchPacketInterpreter consumer, producer;
    CgsGraphics::DispatchPacketInterpreter* mpInterpreter = &consumer;
    CgsGraphics::DispatchPacketInterpreter* mpMeshProducerInterpreterPC = &producer;
    bool mbRenderPreZ = true, mbRenderPreZAlpha = false;
    float mfPreZDistanceThreshold = 200;
    unsigned muMeshReadFramePC = 0;
    #include "pc_mesh_preparation_state.inc"
    PreparedMeshFramePC maPreparedMeshFramesPC[2];
    BrnRendererModule()
    {
        maPreparedMeshFramesPC[0].mpFrame = &mSingleBufferedDispatchFrame;
        maPreparedMeshFramesPC[1].mpFrame = &mSecondMeshFramePC;
    }
    void InitializeDispatchContextPC(CgsGraphics::DispatchObjectContext*) const;
    bool BuildDispatchLists(CgsGraphics::DispatchObjectContext*);
    void PrepareMeshFramePC(u32, CgsGraphics::DispatchFrame*);
    void BeginMeshFramePC();
    void PrepareMeshFrameForWritePC();
    void PublishMeshFramePC();
    void ConvertObjectsToMeshesPC(CgsGraphics::DispatchFrame* in,
        CgsGraphics::DispatchFrame* out, CgsGraphics::DispatchPacketInterpreter* interpreter,
        const CgsGraphics::DispatchObjectContext*)
    {
        CGS_ASSERT(interpreter->frame == out, "conversion interpreter owns destination");
        ++conversions;
        for (int id : in->objects) out->meshes.push_back({id, &in->constant, materialVersion});
    }
    void ConvertObjectsToMeshes(CgsGraphics::BufferedDispatchFrame* in,
        CgsGraphics::DispatchFrame* out, CgsGraphics::DispatchPacketInterpreter* interpreter,
        const CgsGraphics::DispatchObjectContext* context)
    { ConvertObjectsToMeshesPC(&in->GetDispatchFrameForRead(), out, interpreter, context); }
    void SortDispatchLists(CgsGraphics::DispatchFrame* frame)
    {
        ++sorts;
        for (auto& bank : maPreparedMeshFramesPC) if (bank.mpFrame == frame) {
            bank.mSortJobs.pending = true;
            frame->pendingSort = &bank.mSortJobs.pending;
        }
        std::sort(frame->meshes.begin(), frame->meshes.end(),
            [](const auto& a, const auto& b) { return a.id < b.id; });
    }
};
#include "pc_mesh_preparation.inc"

static void Produce(BrnRendererModule& renderer, float value)
{
    auto& frame = renderer.mDoubleBufferedDispatchFrame.GetDispatchFrameForWrite();
    frame.Reset();
    frame.constant = value;
    frame.objects = {9, 2, 7};
}
static void Publish(BrnRendererModule& renderer)
{
    renderer.PublishMeshFramePC();
    renderer.mDoubleBufferedDispatchFrame.Swap();
}
static bool Matches(const CgsGraphics::DispatchFrame* frame, float constant, unsigned material)
{
    if (!frame || frame->meshes.size() != 3) return false;
    const int ids[] = {2, 7, 9};
    for (unsigned i = 0; i < 3; ++i)
        if (frame->meshes[i].id != ids[i] || *frame->meshes[i].constant != constant
            || frame->meshes[i].material != material) return false;
    return true;
}
int main()
{
    _putenv_s("BRN_PREZ_ALL", "0");
    namespace fp = renderengine::FrameProfile;
    fp::Frame profile;
    fp::gCapture.mpCurrent = &profile;
    fp::gCapture.mbTimingOnly = true;
    BrnRendererModule r;
    r.BeginMeshFramePC();
    Check(conversions == 1 && r.consumer.frame == &r.mSingleBufferedDispatchFrame,
          "cold read frame is prepared before rendering");
    CgsGraphics::DispatchObjectContext context;
    Check(r.BuildDispatchLists(&context) && r.consumer.frame->meshes.empty(),
          "boot can consume a valid empty frame");
    Produce(r, 10);
    r.PrepareMeshFrameForWritePC();
    Check(r.consumer.frame == &r.mSingleBufferedDispatchFrame && r.consumer.frame->meshes.empty(),
          "preparing write bank leaves consumer interpreter and read contents unchanged");
    Check(Matches(&r.mSecondMeshFramePC, 10, 1), "write bank contains sorted commands and live GDL constants");
    if (failures)
    {
        // Stop the wrong-bank negative before it dereferences missing output.
        std::printf("PCMeshPreparation: %u checks, %u failures\n", checks, failures);
        return 1;
    }
    const unsigned before = conversions;
    Publish(r);
    Check(conversions == before && r.consumer.frame == &r.mSecondMeshFramePC,
          "unchanged resources publish prepared work without a second conversion");
    Check(r.consumer.frame->meshes[0].constant == &r.mDoubleBufferedDispatchFrame.GetDispatchFrameForRead().constant,
          "mesh output and constant-owning GDL become readable together");

    bool isolated = true, noRenderConversion = true;
    for (unsigned n = 0; n < 128; ++n)
    {
        r.BeginMeshFramePC();
        auto* frozen = r.consumer.frame;
        const float oldValue = *frozen->meshes[0].constant;
        std::atomic<bool> started{false}, done{false}, stable{true};
        std::thread render([&] {
            CgsGraphics::DispatchObjectContext renderContext;
            if (!r.BuildDispatchLists(&renderContext)) stable = false;
            started.store(true, std::memory_order_release);
            do {
                if (r.consumer.frame != frozen || !Matches(frozen, oldValue, 1)) stable = false;
                std::this_thread::yield();
            } while (!done.load(std::memory_order_acquire));
        });
        while (!started.load(std::memory_order_acquire)) std::this_thread::yield();
        const unsigned oldCount = conversions;
        Produce(r, float(n + 20));
        r.PrepareMeshFrameForWritePC();
        done.store(true, std::memory_order_release);
        render.join();
        isolated &= stable.load();
        Publish(r);
        noRenderConversion &= conversions == oldCount + 1;
        isolated &= Matches(r.consumer.frame, float(n + 20), 1);
    }
    Check(isolated, "128 overlapping frames never overwrite read commands or their GDL constants");
    Check(noRenderConversion, "only one conversion per update after cold start");
    Check(&r.mSingleBufferedDispatchFrame == r.maPreparedMeshFramesPC[0].mpFrame
          && &r.mSecondMeshFramePC == r.maPreparedMeshFramesPC[1].mpFrame,
          "physical frames do not move when ownership flips");

    r.BeginMeshFramePC(); Produce(r, 300); r.PrepareMeshFrameForWritePC();
    const unsigned oldCount = conversions;
    ++materialVersion;
    renderengine::MeshPreparationPC::ResourceChanged();
    Publish(r);
    Check(conversions == oldCount + 1 && Matches(r.consumer.frame, 300, 2),
          "resource publication after preparation rebuilds affected commands before swap");
    Check(profile.muMeshRebuilt == 1, "resource invalidation is visible in frame counters");
    r.BeginMeshFramePC();
    const unsigned settingsCount = conversions;
    r.mbRenderPreZ = false; r.mbRenderPreZAlpha = true; r.mfPreZDistanceThreshold = 80.5f;
    r.BeginMeshFramePC();
    r.BuildDispatchLists(&context);
    Check(conversions == settingsCount + 1 && !context.mbPreZEnabled && context.mbPreZAlphaEnabled
          && context.mvPreZDistanceThreshold[3] == 80.5f,
          "changed pre-Z controls rebuild read frame before worker release");
    Check(profile.muMeshRebuilt == 2, "control invalidation is counted separately from normal preparation");
    r.mDoubleBufferedDispatchFrame.GetDispatchFrameForWrite().Reset();
    r.PrepareMeshFrameForWritePC(); Publish(r);
    Check(r.consumer.frame->meshes.empty() && r.BuildDispatchLists(&context),
          "empty/loading frames clear all old commands");
    Check(sorts == conversions, "every newly expanded bank is sorted exactly once");
    Check(resetWhileSorting == 0, "bank reuse and resource/control rebuilds join before resetting command storage");
    Check(profile.maTicks[fp::UPDATE_MESH_PREPARE] == 0,
          "timing-only mode does not add preparation clock reads");

    BrnRendererModule serial;
    serial.mpMeshProducerInterpreterPC = nullptr;
    serial.BeginMeshFramePC(); serial.PrepareMeshFrameForWritePC(); serial.PublishMeshFramePC();
    const unsigned serialCount = conversions;
    auto& serialInput = serial.mDoubleBufferedDispatchFrame.GetDispatchFrameForRead();
    serialInput.objects = {7, 9, 2}; serialInput.constant = 400;
    Check(serial.BuildDispatchLists(&context) && Matches(serial.consumer.frame, 400, 2)
          && conversions == serialCount + 1,
          "disabled experiment retains original render-side conversion");
    serial.mpInterpreter = nullptr;
    Check(!serial.BuildDispatchLists(&context) && conversions == serialCount + 1,
          "missing renderer never converts or publishes a frame");
    fp::gCapture.mpCurrent = nullptr;
    std::printf("PCMeshPreparation: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
