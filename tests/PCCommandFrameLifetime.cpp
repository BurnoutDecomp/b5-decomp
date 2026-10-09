#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <vector>
#include "GameSource/Game/BrnDispatchThreadInputBuffer.h"
#include "GameSource/Graphics/BrnShaderConstantsFrame.h"
#include "GameSource/Graphics/BrnEffectsArbitrator.h"
#include "GameSource/Graphics/BrnBlobbyShadowManager.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsTextureScopeTable.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h"

static unsigned checks, failures, assertions, particleReads;
static void Check(bool good, const char* label) {
    ++checks; if (!good) { ++failures; if (failures <= 16) std::printf("FAIL %s\n", label); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++assertions; return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsDev { namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; } }
namespace CgsGraphics {
#include "command_im_methods.inc"
}
namespace BrnGame {
#include "command_io_methods.inc"
}
namespace BrnGraphics {
static const u8 kau8SlotsPerEffectsLayer[3] = {1, 4, 2};
#include "command_effects_methods.inc"
}
#include "command_effect_defaults.inc"

struct FrameRing {
    struct Frame { unsigned value = 0; void Reset() { value = 0; } } frames[2];
    unsigned muCurrentFrameForWrite = 0, muCurrentFrameForRead = 1, muNumDispatchFrames = 2;
    Frame& GetDispatchFrameForWrite() { return frames[muCurrentFrameForWrite]; }
    unsigned& GetDispatchBinForWrite() { return frames[muCurrentFrameForWrite].value; }
    void Swap();
};
#include "command_ring_methods.inc"
namespace CgsGraphics {
struct ShaderTableBoundary { unsigned begins = 0; void BeginFrame(unsigned*) { ++begins; } } mShaderConstantTable;
}
struct CoronaBoundary {
    unsigned write = 0, values[2] = {};
    void Clear() { values[write] = 0; }
    void Swap() { write ^= 1; }
};
static bool sbEffectsArbitratorConstructed = true;
static void EnsureEffectsArbitratorBringUp(BrnGraphics::EffectsArbitrator&) {}
using ImBank = CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>;
enum Phase { BEGIN_CLEAR, BEGIN_KEEP, LOADING_COMMAND, BUILD_GDL, EFFECTS_CALLBACK, DRAW_BODY, PRESENT };
static std::vector<Phase> phases;
namespace renderengine {
namespace Device {
static unsigned clears, keeps, presents;
static bool FrameBegin() { ++clears; phases.push_back(BEGIN_CLEAR); return true; }
static bool FrameBeginNoClear() { ++keeps; phases.push_back(BEGIN_KEEP); return true; }
static void ShowPixelBuffer() { ++presents; phases.push_back(PRESENT); }
}
namespace FrameProfile {
enum { DISPATCH_EFFECTS, RENDER_BUILD_LISTS, RENDER_SETUP, RENDER_PRESENT };
struct Scope { explicit Scope(int) {} };
struct Stage { void Next(int) {} };
}
}
namespace BrnEffects {
// Observe the Particle callback boundary. Its full startup walk is separately
// audited against 8229C5F0; this tests the real Effects lock wrapper and control
// queue lifecycle, not Lion implementation or native debris drawing.
struct EffectsCallbackOwner {
    struct ParticleBoundary {
        unsigned calls = 0, events = 0, changed = 0;
        bool startupEmpty = false;
        void DispatchThreadUpdate(const BrnGame::DispatchThreadInputBuffer* input) {
            phases.push_back(EFFECTS_CALLBACK); ++calls;
            const auto* particle = input->GetParticleData();
            changed += particle->muChangedEffects;
            const auto* queue = input->GetParticleInterThreadEventQueue();
            const CgsModule::Event* event = nullptr; s32 size = 0;
            s32 type = queue->GetFirstEvent(&event, &size);
            if (calls == 1) startupEmpty = !event && !particle->muChangedEffects
                && particle->mfCurrentTimeStep == 0 && input->GetParticleRenderData()->muFlags == 0;
            while (event) {
                Check(type == 99 && size == 16, "actual one-shot event payload reaches the fresh callback");
                ++events; type = queue->GetNextEvent(event, &event, &size);
            }
        }
    } mParticleModule;
    void DispatchThreadUpdate(const BrnGame::DispatchThreadInputBuffer*);
};
#include "command_effects_callback.inc"
}
static const BrnGame::DispatchThreadInputBuffer* gpRepeatedControlNegative;
static const BrnGame::DispatchThreadInputBuffer* GetRepeatedControlForNegativePC(
    const BrnGame::DispatchThreadInputBuffer* current) {
    return gpRepeatedControlNegative ? gpRepeatedControlNegative : current;
}
struct BrnRendererModule {
    enum EFrameStallStage { E_FRAMESTALL_NOT_STALLED, E_FRAMESTALL_SYNCING_BUFFERS, E_FRAMESTALL_STALLED };
    enum ECommandProducerPC { E_COMMAND_PRODUCER_GUI, E_COMMAND_PRODUCER_WORLD_AND_EFFECTS };
    EFrameStallStage meFrameStallStage = E_FRAMESTALL_NOT_STALLED;
    int miFrameStallCountdown = 0;
    bool mbUpdateThreadTakeScreenshot = false, mbDispatchThreadTakeScreenshot = false;
    u64 muCommandGenerationPC = 0, muCompletedCommandGenerationPC = 0, muPublishedCommandGenerationPC = 0;
    struct CommandFrameInputPC {
        BrnParticle::ParticleModule::ParticleRenderData mParticleRenderData;
        bool mabEnvMapFaceRender[6], mbParticleRecordProduced;
    } maCommandInputsPC[2] = {};
    BrnShaderConstantsFrame maShaderConstantsFrames[2];
    bool maShaderConstantsFrameValidPC[2] = {};
    u8 mu8ShaderConstantsFrameInternal = 0, mu8ShaderConstantsFrameExternal = 1;
    void* mpInterpreter = this;
    ImBank mIm2dRenderBuffer, mIm3dRenderBuffer, mIm3dRenderBufferUntex, mIm3dDebugRenderBuffer;
    ImBank mIm2dDebugRenderBuffer, mIm3dBufferRacePosition, mIm3dBufferMenusAndHud;
    FrameRing mDoubleBufferedDispatchFrame;
    BrnGraphics::EffectsArbitrator mEffectsArbitrator;
    BrnBlobbyShadowManager mBlobbyShadowManager = {};
    CoronaBoundary mCoronaManager;
    unsigned meshPublishes = 0;
    unsigned draws = 0;
    struct LoadingBoundary { void AddCommand(BrnGame::ELoadingScreenCommand) { phases.push_back(LOADING_COMMAND); } } mLoadingScreenRenderer;
    bool BuildDispatchLists(CgsGraphics::DispatchObjectContext*) { phases.push_back(BUILD_GDL); return true; }
    void BeginMeshFramePC() {}
    void PublishMeshFramePC() { ++meshPublishes; }
    void StartOfFrame();
    void SwapBuffers();
    void EndOfFrame(bool);
    void CompleteCommandFramePC(const BrnGame::DispatchThreadInputBuffer*, ECommandProducerPC);
    const BrnParticle::ParticleModule::ParticleRenderData* GetPublishedParticleRenderDataPC() const;
    bool GetPublishedEnvMapFaceRenderPC(u32) const;
    void Render(BrnEffects::EffectsCallbackOwner*, const BrnGame::DispatchThreadInputBuffer*);
};
#include "command_frame_methods.inc"
void BrnRendererModule::Render(BrnEffects::EffectsCallbackOwner* lpEffectsModule,
                              const BrnGame::DispatchThreadInputBuffer* lpDispatchThreadInputBuffer) {
    using namespace renderengine::FrameProfile;
    Stage lRenderStage;
#include "command_render_prefix.inc"
    phases.push_back(DRAW_BODY); ++draws;
    if (lpDispatchThreadInputBuffer != nullptr) lpDispatchThreadInputBuffer->UnlockForRead();
    renderengine::Device::ShowPixelBuffer();
}

struct ParticleSnapshotOwner {
    using ParticleRenderData = BrnParticle::ParticleModule::ParticleRenderData;
    struct SimpleSnapshot { unsigned publishes = 0; void Publish(unsigned) { ++publishes; } } mSimpleParticleFramePC;
    struct TrailSystem {
        unsigned ends = 0, updates = 0; float mfCurrentTimeStep = 0, time = -1;
        Matrix44 view = {};
        void EndOfFrame() { ++ends; }
        void Update(float dt, float t, Matrix44::InParam vp) { ++updates; mfCurrentTimeStep = dt; time = t; view = vp; }
    } mTrailSystem;
    struct TrailSnapshot { unsigned publishes = 0; void Publish(TrailSystem&) { ++publishes; } } mTrailFramePC;
    unsigned maSimpleParticles = 0;
    u32 muTrailSystemUpdateFramePC = 0;
    f32 mfTrailSystemTimeStepPC = 0.0f;
    bool mbStalled = false;
    void EndOfFrame(bool);
    void PublishRenderCommandsPC(const ParticleRenderData&);
};
#include "command_particle_methods.inc"

struct BankStorage { unsigned char commands[2][128], vertices[2][128]; };
static void Prepare(ImBank& bank, BankStorage& storage) {
    bank.Construct();
    for (unsigned i = 0; i < 2; ++i) {
        bank.maBuffers[i].mpu8CommandBuffer = storage.commands[i];
        bank.maBuffers[i].mpu8VertexBuffer = storage.vertices[i];
    }
    bank.mpWriteBuffer = &bank.maBuffers[0]; bank.mpDispatchBuffer = &bank.maBuffers[1];
    bank.muCommandBufferSize = bank.muVertexBufferSize = 128;
}
static void Record(ImBank& bank, unsigned token) {
    auto* c = reinterpret_cast<CgsGraphics::ImCommand*>(bank.mpWriteBuffer->mpu8CommandBuffer);
    c->muType = token; c->muSize = 16; bank.mpWriteBuffer->muCommandBufferWritePos = 16;
}
static std::vector<ImBank*> Banks(BrnRendererModule& r) {
    return {&r.mIm2dRenderBuffer, &r.mIm3dRenderBuffer, &r.mIm3dRenderBufferUntex,
        &r.mIm3dDebugRenderBuffer, &r.mIm2dDebugRenderBuffer, &r.mIm3dBufferRacePosition, &r.mIm3dBufferMenusAndHud};
}
static void RecordFrame(BrnRendererModule& r, BrnGame::DispatchThreadInputBuffer& input, unsigned value, bool empty = false) {
    for (auto* bank : Banks(r)) if (!empty) Record(*bank, value);
    r.mDoubleBufferedDispatchFrame.GetDispatchFrameForWrite().value = value;
    r.maShaderConstantsFrames[r.mu8ShaderConstantsFrameExternal].SetWhiteLevel(float(value));
    r.mEffectsArbitrator.GetExternalEffectsFrame(0, 0)->mfBloomWeight = float(value);
    r.mCoronaManager.values[r.mCoronaManager.write] = value;
    input.LockForWrite();
    auto* data = input.GetParticleRenderData();
    std::memset(data, 0, sizeof(*data));
    data->mpParticleModule = reinterpret_cast<BrnParticle::ParticleModule*>(UINT64_C(0x1234567800000040));
    data->muCurrentFrame = value; data->mfCurrentTimeStep = .125f; data->mfCurrentTime = float(value);
    data->mCgsCamera.mViewProjection.xAxis.x = float(value);
    for (unsigned face = 0; face < 6; ++face) input.SetEnvMapFaceRender(face, ((value + face) & 1) != 0);
    input.UnlockForWrite();
    r.CompleteCommandFramePC(&input, BrnRendererModule::E_COMMAND_PRODUCER_WORLD_AND_EFFECTS);
}
static void CheckFrame(BrnRendererModule& r, unsigned value, bool empty = false) {
    for (auto* bank : Banks(r)) {
        const auto* c = bank->GetFirstCommand();
        Check(empty ? c == nullptr : c != nullptr && c->muType == value, "all seven immediate banks retain the completed command frame");
    }
    Check(r.mDoubleBufferedDispatchFrame.frames[r.mDoubleBufferedDispatchFrame.muCurrentFrameForRead].value == value, "world GDL retains the same generation");
    Check(r.maShaderConstantsFrames[r.mu8ShaderConstantsFrameInternal].GetWhiteLevel() == float(value), "shader bank retains the same generation");
    Check(r.mEffectsArbitrator.GetInternalEffectsFrame(0, 0).mfBloomWeight == float(value), "effects bank retains the same generation");
    const auto* data = r.GetPublishedParticleRenderDataPC();
    Check(data && data->muCurrentFrame == value && data->mCgsCamera.mViewProjection.xAxis.x == float(value)
        && data->mpParticleModule == reinterpret_cast<BrnParticle::ParticleModule*>(UINT64_C(0x1234567800000040)), "whole actual particle metadata follows its generation");
    for (unsigned face = 0; face < 6; ++face)
        Check(r.GetPublishedEnvMapFaceRenderPC(face) == (((value + face) & 1) != 0), "all six reflection face flags follow the command bank");
}
int main() {
    BrnRendererModule r;
    BankStorage storage[7] = {};
    auto banks = Banks(r);
    for (unsigned i = 0; i < banks.size(); ++i) Prepare(*banks[i], storage[i]);
    BrnGraphics::EffectsArbitrator::EffectsFramePair effects[7];
    for (auto& pair : effects) for (auto& frame : pair) frame.Construct();
    r.mEffectsArbitrator.mapaEffectsFrames[0] = effects;
    r.mEffectsArbitrator.mapaEffectsFrames[1] = effects + 1;
    r.mEffectsArbitrator.mapaEffectsFrames[2] = effects + 5;
    r.mEffectsArbitrator.mu8EffectsFrameInternal = 0; r.mEffectsArbitrator.mu8EffectsFrameExternal = 1;
    for (auto& frame : r.maShaderConstantsFrames) frame.Construct();
    r.maShaderConstantsFrames[1].LockForWriting(); r.mBlobbyShadowManager.mu8External = 1;
    BrnGame::DispatchThreadInputBuffer input;
    input.CgsModule::IOBuffer::Construct();
    r.StartOfFrame(); r.EndOfFrame(false);
    Check(r.muPublishedCommandGenerationPC == 0 && r.GetPublishedParticleRenderDataPC() == nullptr, "no completed producer means no published command frame");
    RecordFrame(r, input, 11); r.EndOfFrame(false); CheckFrame(r, 11);
    const u64 published = r.muPublishedCommandGenerationPC;
    for (unsigned n = 0; n < 100; ++n) {
        r.StartOfFrame(); r.EndOfFrame(false); CheckFrame(r, 11);
        Check(r.muPublishedCommandGenerationPC == published, "extra presentations cannot rotate or publish an empty bank");
    }
    r.StartOfFrame(); RecordFrame(r, input, 12, true); r.EndOfFrame(false); CheckFrame(r, 12, true);
    Check(r.muPublishedCommandGenerationPC != published, "an actually completed empty frame still publishes without content tests");
    r.StartOfFrame(); RecordFrame(r, input, 13); r.EndOfFrame(true);
    Check(r.meFrameStallStage == BrnRendererModule::E_FRAMESTALL_SYNCING_BUFFERS && r.miFrameStallCountdown == 1, "original stall entry begins the two-frame countdown");
    CheckFrame(r, 12, true);
    r.StartOfFrame(); r.EndOfFrame(true);
    Check(r.meFrameStallStage == BrnRendererModule::E_FRAMESTALL_STALLED && r.miFrameStallCountdown == 0, "second original stalled frame reaches stage 2");
    CheckFrame(r, 12, true);
    Check(r.mEffectsArbitrator.GetExternalEffectsFrame(0, 0)->mfBloomWeight == 0, "canceled external effects cannot leak into the following producer");
    r.StartOfFrame(); RecordFrame(r, input, 14); r.EndOfFrame(true); CheckFrame(r, 14);
    Check(r.meFrameStallStage == BrnRendererModule::E_FRAMESTALL_STALLED, "stage 2 still schedules original swaps when a real producer completed");
    r.StartOfFrame(); r.mbUpdateThreadTakeScreenshot = true; r.EndOfFrame(false);
    Check(r.meFrameStallStage == BrnRendererModule::E_FRAMESTALL_NOT_STALLED && r.miFrameStallCountdown == 0, "stall release resets original stage and countdown");
    Check(!r.mbUpdateThreadTakeScreenshot && r.mbDispatchThreadTakeScreenshot, "screenshot edge transfers even without command production");
    const unsigned beforeReads = particleReads;
    r.StartOfFrame(); r.CompleteCommandFramePC(&input, BrnRendererModule::E_COMMAND_PRODUCER_GUI); r.EndOfFrame(false);
    Check(particleReads == beforeReads && r.GetPublishedParticleRenderDataPC() == nullptr, "GUI-only completion never reads untouched particle payload");
    for (unsigned face = 0; face < 6; ++face) Check(!r.GetPublishedEnvMapFaceRenderPC(face), "GUI-only frame has no world face producer");
    ParticleSnapshotOwner p;
    BrnParticle::ParticleModule::ParticleRenderData data = {};
    data.muCurrentFrame = 1; data.mfCurrentTimeStep = .25f; data.mfCurrentTime = 3;
    p.EndOfFrame(true); p.PublishRenderCommandsPC(data);
    Check(p.mbStalled && p.mTrailSystem.ends == 1 && p.mTrailSystem.updates == 1, "original particle EOF and completed snapshot publication remain distinct");
    p.EndOfFrame(false); p.PublishRenderCommandsPC(data);
    Check(!p.mbStalled && p.mTrailSystem.ends == 2 && p.mTrailSystem.updates == 1, "original particle record guard suppresses duplicate clock application");
    data.muCurrentFrame = 2; data.mfCurrentTimeStep = 0; data.mfCurrentTime = 4; p.PublishRenderCommandsPC(data);
    Check(p.mTrailSystem.updates == 2 && p.mTrailSystem.mfCurrentTimeStep == .25f
        && p.mTrailSystem.time == 4 && data.mfCurrentTimeStep == 0,
        "a zero-step record preserves wheel cadence, its absolute clock and its original particle step");
    // Same once-allocated zero storage as the real saDispatchMem control pair.
    // Construct never manufactures a ParticleRenderData/default camera writer.
    static BrnGame::DispatchThreadInputBuffer control[2];
    for (auto& bank : control) bank.Construct();
    BrnGame::DispatchThreadInputBufferManager manager;
    manager.mapBuffers[0] = &control[0]; manager.mapBuffers[1] = &control[1];
    manager.mpWriteBuffer = &control[0]; manager.mpReadBuffer = &control[1]; manager.muWriteBufferIndex = 0;
    control[0].SetIsWriteBuffer(true); control[1].SetIsWriteBuffer(false);
    BrnEffects::EffectsCallbackOwner effectsOwner;
    auto renderControl = [&](int stage) {
        r.meFrameStallStage = static_cast<BrnRendererModule::EFrameStallStage>(stage);
        const unsigned beforeDraws = r.draws, beforeCalls = effectsOwner.mParticleModule.calls;
        const unsigned beforeClear = renderengine::Device::clears, beforeKeep = renderengine::Device::keeps;
        phases.clear(); r.Render(&effectsOwner, manager.GetReadBuffer());
        const bool draw = stage == 0 || stage == 1;
        const std::vector<Phase> expected = draw
            ? std::vector<Phase>{BEGIN_CLEAR, LOADING_COMMAND, BUILD_GDL, EFFECTS_CALLBACK, DRAW_BODY, PRESENT}
            : std::vector<Phase>{BEGIN_KEEP, LOADING_COMMAND, BUILD_GDL, EFFECTS_CALLBACK, PRESENT};
        Check(phases == expected, "original loading/GDL/callback/gate/present order, with no stage2 draw or clear");
        Check(r.draws == beforeDraws + draw && effectsOwner.mParticleModule.calls == beforeCalls + 1,
              "fresh control callback runs unconditionally in every stall stage");
        Check(renderengine::Device::clears == beforeClear + draw && renderengine::Device::keeps == beforeKeep + !draw,
              "suppressed draw presents the persistent engine surface without clearing it");
        Check(!manager.GetReadBuffer()->IsBufferLockedForReading(), "effects and frame read windows are balanced separately");
    };
    renderControl(0);
    Check(effectsOwner.mParticleModule.startupEmpty, "existing startup control storage reaches the empty callback without a fake producer");
    auto* write = manager.GetWriteBuffer(); write->LockForWrite();
    write->GetParticleData()->mfCurrentTime = 3; write->GetParticleData()->mfCurrentTimeStep = .125f;
    write->GetParticleData()->muChangedEffects = 1;
    unsigned char eventBytes[16] = {};
    write->GetParticleInterThreadEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(eventBytes), 99, 16);
    write->UnlockForWrite(); manager.Swap();
    renderControl(2);
    // Negative control only: retaining the entire callback input would keep
    // one-shot queue/count contents alive with the immutable draw frame.
    static BrnGame::DispatchThreadInputBuffer repeatedCallbackNegative;
    std::memcpy(&repeatedCallbackNegative, manager.GetReadBuffer(), sizeof(repeatedCallbackNegative));
    gpRepeatedControlNegative = &repeatedCallbackNegative;
    Check(effectsOwner.mParticleModule.events == 1 && effectsOwner.mParticleModule.changed == 1,
          "pre-render one-shot batch is consumed even while stage2 retains draw commands");
    const u64 heldGeneration = r.muPublishedCommandGenerationPC;
    manager.Swap(); renderControl(2);
    Check(effectsOwner.mParticleModule.events == 1 && effectsOwner.mParticleModule.changed == 1,
          "extra presentation consumes fresh empty control IO and cannot replay retained events or Lion changes");
    Check(r.muPublishedCommandGenerationPC == heldGeneration, "control rotation leaves the retained command generation unchanged");
    for (int stage : {1, 3, -1}) { manager.Swap(); renderControl(stage); }
    Check(assertions == 0, "all original buffer locks and shader accesses remain valid");
    std::printf("PCCommandFrameLifetime: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
