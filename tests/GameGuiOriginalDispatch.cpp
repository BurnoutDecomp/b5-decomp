// Execute the production full GUI leg with real IO lock/queue bodies.
// Subsystem producers are observed at their typed boundaries; no GPU is used.
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include "types.hpp"
#include "SharedClasses/BrnSharedConstants.h"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameSource/Gui/BrnGuiPerfmons.h"
#include "GameSource/Game/BrnGlobalCpuMonitors.h"
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueueImpl.h"

static unsigned checks, failures, assertions;
static std::vector<unsigned> trace;
static void Check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s\n", message); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++assertions; return 0; }
void* EndAssert() { return nullptr; }
} namespace PerfMonCpu {
void StartMonitor(s32 index) { trace.push_back(100 + index); }
void StopMonitor(s32 index) { trace.push_back(200 + index); }
} }
int32_t BrnGui::GuiPerfmons::miGuiModuleUpdate = 2;
namespace CgsDev { namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; } }
namespace CgsModule { class IOBufferStack {}; }
namespace CgsInput { const u32 KU_NUMBER_OF_PADS = 4;
namespace InputIO { struct OutputBuffer : CgsModule::IOBuffer {}; } }
namespace BrnNetwork { namespace BrnNetworkModuleIO {
struct OutputBuffer : CgsModule::IOBuffer {};
} }
namespace BrnGameState { namespace GameStateModuleIO {
struct OutputBuffer : CgsModule::IOBuffer {};
} }
namespace BrnWorldIO { struct UpdateOutputBuffer : CgsModule::IOBuffer {}; }
namespace BrnReplays { namespace ReplayIO {
struct OutputBuffer_PreSim : CgsModule::IOBuffer { u32 producedSequence; };
} }
namespace BrnDirector { namespace DirectorIO { struct OutputBuffer : CgsModule::IOBuffer {}; } }
namespace BrnEffects { namespace EffectsIO { struct OutputBuffer : CgsModule::IOBuffer {}; } }
namespace BrnResource { namespace GameDataIO {
struct InputBuffer : CgsModule::IOBuffer {
    RequestInterface<32768> requests;
    auto* GetRequestInterface() { Check(IsBufferLockedForWriting(), "resource bridge has GameData write lock"); return &requests; }
    template<s32 N> bool AppendRequestInterface(const RequestInterface<N>& source)
    { return requests.mRequestQueue.Append<N, 16>(source.mRequestQueue); }
};
struct OutputBuffer : CgsModule::IOBuffer {};
} }
namespace CgsGui {
namespace CgsGuiModuleIO {
struct InputBuffer : CgsModule::IOBuffer
{
    CgsModule::VariableEventQueue<32768, 16> events;
    auto* GetGuiEvents() { Check(IsBufferLockedForWriting(), "GUI input accessor has write lock"); return &events; }
};
struct OutputBuffer : CgsModule::IOBuffer
{
    unsigned clearCount = 0;
    using GuiEventQueue = CgsModule::VariableEventQueue<18432, 16>;
    GuiEventQueue events;
    const GuiEventQueue* GetOutEventQueue() const
    { Check(IsBufferLockedForReading(), "resource bridge reads module output under lock"); return &events; }
    void Clear() { trace.push_back(10); ++clearCount; }
};
}
namespace ModelIO { struct OutputBuffer : CgsModule::IOBuffer {
    unsigned clearCount = 0;
    CgsResource::ResourceIO::ResourceRequestQueue<2048> requests;
    const auto* GetGuiResourceRequestQueue() const
    { Check(IsBufferLockedForReading(), "resource bridge reads model output under lock"); return &requests; }
    void Clear() { trace.push_back(11); ++clearCount; }
}; }
namespace ViewIO { struct InputBuffer : CgsModule::IOBuffer {}; }
}

struct ExpectedInputs
{
    CgsModule::IOBufferStack *inStack, *outStack;
    CgsGui::CgsGuiModuleIO::InputBuffer* gui;
    CgsGui::CgsGuiModuleIO::OutputBuffer* guiOut;
    CgsGui::ModelIO::OutputBuffer* modelOut;
    CgsGui::ViewIO::InputBuffer* view;
    const CgsInput::InputIO::OutputBuffer* input;
    const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* network;
    const BrnGameState::GameStateModuleIO::OutputBuffer* gameState;
    const BrnWorldIO::UpdateOutputBuffer* world;
    const BrnReplays::ReplayIO::OutputBuffer_PreSim* replay;
    BrnDirector::DirectorIO::OutputBuffer* director;
    BrnResource::GameDataIO::InputBuffer* dataIn;
    const BrnResource::GameDataIO::OutputBuffer* dataOut;
    BrnUpdateSet updateSet;
} expected;

static void CheckBridgeLocks()
{
    Check(expected.gui->IsBufferLockedForWriting(), "bridge destination is write locked");
    Check(expected.input->IsBufferLockedForReading(), "input output is read locked");
    Check(expected.gameState->IsBufferLockedForReading(), "game-state output is read locked");
    Check(expected.world->IsBufferLockedForReading(), "world output is read locked");
    Check(expected.replay->IsBufferLockedForReading(), "actual replay output is read locked");
    Check(expected.director->IsBufferLockedForReading(), "director output is read locked");
}
struct ObservedGuiModule
{
    unsigned updates = 0;
    void Update(BrnUpdateSet set, CgsModule::IOBufferStack* in, CgsModule::IOBufferStack* out,
        CgsGui::CgsGuiModuleIO::InputBuffer* gui, CgsGui::CgsGuiModuleIO::OutputBuffer* guiOut,
        CgsGui::ModelIO::OutputBuffer* modelOut, CgsGui::ViewIO::InputBuffer* view,
        BrnResource::GameDataIO::InputBuffer* dataIn,
        const BrnResource::GameDataIO::OutputBuffer* dataOut, bool enable)
    {
        trace.push_back(30); ++updates;
        Check(set == expected.updateSet && in == expected.inStack && out == expected.outStack,
              "original update set and both stacks reach GUI owner");
        Check(gui == expected.gui && guiOut == expected.guiOut && modelOut == expected.modelOut
              && view == expected.view && dataIn == expected.dataIn && dataOut == expected.dataOut,
              "all six typed supplied buffers reach GUI owner");
        Check(enable, "original trailing true argument reaches GUI owner");
        Check(dataIn->IsBufferLockedForWriting() && dataOut->IsBufferLockedForReading(),
              "GameData lock bracket encloses GUI update");
        Check(!gui->IsBufferLocked() && !expected.input->IsBufferLocked()
              && !expected.gameState->IsBufferLocked() && !expected.world->IsBufferLocked()
              && !expected.replay->IsBufferLocked() && !expected.director->IsBufferLocked()
              && !expected.network->IsBufferLocked(), "release bridge locks before module update");
    }
};
namespace BrnGame {
class BrnGameModule
{
public:
    BrnCpuMonitors mCpuMonitors = {};
    ObservedGuiModule mGuiModule;
    bool mbDiskError = false;
    void BridgeWorldToGui(CgsGui::CgsGuiModuleIO::InputBuffer* gui, const BrnWorldIO::UpdateOutputBuffer* world)
    { trace.push_back(20); CheckBridgeLocks(); Check(gui == expected.gui && world == expected.world, "actual world source"); }
    void BridgeControllerToGui(CgsGui::CgsGuiModuleIO::InputBuffer* gui, const CgsInput::InputIO::OutputBuffer* input)
    { trace.push_back(21); CheckBridgeLocks(); Check(gui == expected.gui && input == expected.input, "actual controller source"); }
    void BridgeGameStateToGui(CgsGui::CgsGuiModuleIO::InputBuffer* gui, const BrnGameState::GameStateModuleIO::OutputBuffer* gameState)
    { trace.push_back(22); CheckBridgeLocks(); Check(gui == expected.gui && gameState == expected.gameState, "actual game-state source"); }
    void BridgeDirectorToGui(CgsGui::CgsGuiModuleIO::InputBuffer* gui, BrnDirector::DirectorIO::OutputBuffer* director)
    { trace.push_back(23); CheckBridgeLocks(); Check(gui == expected.gui && director == expected.director, "actual director source"); }
    void BridgeReplayToGui(CgsGui::CgsGuiModuleIO::InputBuffer* gui, const BrnReplays::ReplayIO::OutputBuffer_PreSim* replay)
    { trace.push_back(24); CheckBridgeLocks(); Check(gui == expected.gui && replay == expected.replay && replay->producedSequence == 7654321,
          "consume the actual completed replay producer, including inactive frames"); }
    void BridgeNetworkToGui(CgsGui::CgsGuiModuleIO::InputBuffer* gui, const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* network)
    { trace.push_back(25); CheckBridgeLocks(); Check(gui == expected.gui && network == expected.network && network->IsBufferLockedForReading(), "nested network read bracket"); }
    void BridgeGameToGui(CgsGui::CgsGuiModuleIO::InputBuffer* gui)
    { trace.push_back(26); CheckBridgeLocks(); Check(gui == expected.gui && !expected.network->IsBufferLocked(), "game bridge follows network unlock"); }
    void BridgeGuiToGame(const CgsGui::CgsGuiModuleIO::OutputBuffer* guiOut)
    { trace.push_back(31); Check(guiOut == expected.guiOut && guiOut->IsBufferLockedForReading(), "read actual GUI output in game consumer");
      Check(!expected.dataIn->IsBufferLocked() && !expected.dataOut->IsBufferLocked(), "release GameData pair before game consumer"); }
    void DoUpdate_GUI(CgsModule::IOBufferStack*, CgsModule::IOBufferStack*,
        CgsGui::CgsGuiModuleIO::InputBuffer*, CgsGui::ViewIO::InputBuffer*,
        const CgsInput::InputIO::OutputBuffer*, const BrnNetwork::BrnNetworkModuleIO::OutputBuffer*,
        const BrnGameState::GameStateModuleIO::OutputBuffer*, const BrnWorldIO::UpdateOutputBuffer*,
        const BrnReplays::ReplayIO::OutputBuffer_PreSim*, CgsGui::CgsGuiModuleIO::OutputBuffer*,
        CgsGui::ModelIO::OutputBuffer*, BrnDirector::DirectorIO::OutputBuffer*,
        BrnEffects::EffectsIO::OutputBuffer*, BrnResource::GameDataIO::InputBuffer*,
        const BrnResource::GameDataIO::OutputBuffer*, BrnUpdateSet, u32);
    void BridgeGuiToResource(BrnResource::GameDataIO::InputBuffer*,
        const CgsGui::ModelIO::OutputBuffer*, const CgsGui::CgsGuiModuleIO::OutputBuffer*);
};
}
#include "game_gui_original_dispatch.inc"

int main()
{
    BrnGame::BrnGameModule game;
    game.mCpuMonitors.miUT_GUI = 1;
    game.mCpuMonitors.miUT_GUI_Bridge = 3;
    CgsModule::IOBufferStack inStack, outStack;
    CgsGui::CgsGuiModuleIO::InputBuffer gui;
    CgsGui::CgsGuiModuleIO::OutputBuffer guiOut;
    CgsGui::ModelIO::OutputBuffer modelOut;
    CgsGui::ViewIO::InputBuffer view;
    CgsInput::InputIO::OutputBuffer input;
    BrnNetwork::BrnNetworkModuleIO::OutputBuffer network;
    BrnGameState::GameStateModuleIO::OutputBuffer gameState;
    BrnWorldIO::UpdateOutputBuffer world;
    BrnReplays::ReplayIO::OutputBuffer_PreSim replay;
    BrnDirector::DirectorIO::OutputBuffer director;
    BrnEffects::EffectsIO::OutputBuffer effects;
    BrnResource::GameDataIO::InputBuffer dataIn;
    BrnResource::GameDataIO::OutputBuffer dataOut;
    CgsModule::IOBuffer* all[] = {&gui, &guiOut, &modelOut, &view, &input, &network, &gameState,
                                &world, &replay, &director, &effects, &dataIn, &dataOut};
    for (auto* buffer : all) buffer->Construct();
    gui.events.MarkUnconstructed(); gui.events.Construct();
    replay.producedSequence = 7654321;
    expected = {&inStack, &outStack, &gui, &guiOut, &modelOut, &view, &input, &network,
                &gameState, &world, &replay, &director, &dataIn, &dataOut, 0};

    auto run = [&](BrnUpdateSet set, u32 port, bool disk, f32 progress)
    {
        trace.clear(); gui.events.Clear(); game.mbDiskError = disk; expected.updateSet = set;
        const unsigned priorUpdates = game.mGuiModule.updates;
        game.DoUpdate_GUI(&inStack, &outStack, &gui, &view, &input, &network, &gameState,
            &world, &replay, &guiOut, &modelOut, &director, &effects, &dataIn, &dataOut, set, port);
        const std::vector<unsigned> normal = {101, 102, 10, 11, 103, 20, 21, 22, 23, 24, 25, 26,
                                              203, 30, 31, 202, 201};
        auto expectedTrace = normal;
        if (disk) expectedTrace.erase(expectedTrace.begin() + 13);
        Check(trace == expectedTrace, "original clear/bridge/update/game-consumer order and disk-error gate");
        Check(game.mGuiModule.updates == priorUpdates + (disk ? 0 : 1), "exactly one GUI update unless original disk-error gate");
        for (auto* buffer : all) Check(!buffer->IsBufferLocked(), "no leaked IO lock after full GUI leg");
        const CgsModule::Event* event = nullptr; s32 size = 0;
        s32 type = gui.events.GetFirstEvent(&event, &size);
        Check(type == 225 && size == 4 && *reinterpret_cast<const f32*>(event) == progress,
              "publish original progress value and event id");
        const CgsModule::Event* next = nullptr;
        type = gui.events.GetNextEvent(event, &next, &size);
        if (port == 4) Check(next == nullptr, "no START event for the original absent-port sentinel");
        else Check(type == 143 && size == 4 && *reinterpret_cast<const u32*>(next) == port,
                   "publish actual START-pressed port without value coercion");
    };
    run(0x80, 4, false, -1.0f);
    run(0, 2, false, 0.5f);
    run(0x80, 0, true, -1.0f);
    for (unsigned i = 2; i <= 200; ++i) run(0, 4, false, (i % 200) * 0.5f);

    // The model append and both GUI list-request cases are actual original
    // writers. Assert IDs/resources against ARTIST, including native pointers.
    dataIn.requests.mRequestQueue.MarkUnconstructed(); dataIn.requests.Construct();
    modelOut.requests.MarkUnconstructed(); modelOut.requests.Construct();
    guiOut.events.MarkUnconstructed(); guiOut.events.Construct();
    const u32 modelToken = 0x76543210;
    modelOut.requests.AddEvent(reinterpret_cast<const CgsModule::Event*>(&modelToken), 777, 4);
    struct ListEvent : CgsModule::Event { CgsModule::BaseEventReceiverQueue* receiver; };
    ListEvent vehicle, wheel;
    vehicle.receiver = reinterpret_cast<CgsModule::BaseEventReceiverQueue*>(UINT64_C(0x1234567887654321));
    wheel.receiver = reinterpret_cast<CgsModule::BaseEventReceiverQueue*>(UINT64_C(0x1020304050607080));
    guiOut.events.AddEvent(&vehicle, 263, sizeof(vehicle));
    guiOut.events.AddEvent(&vehicle, 1234, sizeof(vehicle));
    guiOut.events.AddEvent(&wheel, 417, sizeof(wheel));
    dataIn.LockForWrite(); modelOut.LockForRead(); guiOut.LockForRead();
    game.BridgeGuiToResource(&dataIn, &modelOut, &guiOut);
    guiOut.UnlockForRead(); modelOut.UnlockForRead(); dataIn.UnlockForWrite();
    const CgsModule::Event* request = nullptr; s32 requestSize = 0;
    s32 requestType = dataIn.requests.mRequestQueue.GetFirstEvent(&request, &requestSize);
    Check(requestType == 777 && requestSize == 4 && *reinterpret_cast<const u32*>(request) == modelToken,
          "append the model's actual resource queue first");
    const u64 ids[] = {UINT64_C(0xC98B447411F97E38), UINT64_C(0xCF5D625701228838), UINT64_C(0xCF5D625701228838)};
    const s32 replyIds[] = {0, 1, 1};
    for (unsigned i = 0; i < 3; ++i)
    {
        const CgsModule::Event* next = nullptr;
        requestType = dataIn.requests.mRequestQueue.GetNextEvent(request, &next, &requestSize);
        request = next;
        Check(requestType == 49 && request != nullptr, "publish original GameData list request type49");
        if (!request) continue;
        const auto* asset = reinterpret_cast<const BrnResource::GameDataIO::GameDataAssetEvent*>(request);
        Check(asset->mId == ids[i], "original vehicle/wheel resource identity");
        Check(asset->miEventId == replyIds[i], "original per-list reply event id");
        Check(asset->mpReceiverQueue == (i < 2 ? vehicle.receiver : wheel.receiver), "preserve full native receiver pointer");
        Check(asset->miPoolId == 5 && asset->meType == BrnResource::E_ASSETSET_DATA && !asset->mbFailFlag,
              "original pool/type/failure bytes");
    }
    const CgsModule::Event* tail = nullptr;
    if (request)
        dataIn.requests.mRequestQueue.GetNextEvent(request, &tail, &requestSize);
    Check(tail == nullptr, "unrelated GUI event produces no resource request");
    Check(assertions == 0, "production IO lock/queue bodies raise no asserts");
    std::printf("GameGuiOriginalDispatch: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
