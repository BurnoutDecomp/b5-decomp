// Actual selected GameMain producer admission and virtual loading Render bodies.
// GUI/state/module boundaries observe calls; no blank production IO is supplied.
#include <cstdio>
#include <initializer_list>

static unsigned checks, failures, guiProduces, worldProduces, guiUpdates, gameConsumers;
static bool finalIoLive;
static void Check(bool good, const char* label) {
    ++checks; if (!good) { ++failures; std::printf("FAIL %s\n", label); }
}
namespace BrnGame { class BrnGameModule; BrnGameModule* GetMainGameModule(); }
namespace BrnGameMainFlowController {
enum EMainGameFlowState {
    E_MGS_INITIAL_LOADING_SCREEN, E_MGS_CHECK_DISK_SPACE, E_MGS_MARKETING_SCREENS,
    E_MGS_START_SCREEN, E_MGS_MEMORY_CARD, E_MGS_COMPLETE_LOADING, E_MGS_IN_GAME
};
}
static int gBrnScriptedLoadStage;
struct LoadingScriptedState {
    virtual void Render();
    void RenderGUI(void*, void*, void*, void*, bool skip) {
        Check(finalIoLive, "loading GUI producer consumes the live final-step input");
        Check(!skip, "host supplies the original false skip argument");
        ++guiProduces;
    }
};
struct MainGameFlowStateInitialLoadingScreen : LoadingScriptedState {
    enum { E_LOADINGSTAGE_GUIMODULE = 2 };
    int meLoadingScreenStage = 0;
};
struct MainGameFlowStateStartScreen : LoadingScriptedState { void Render() override; };
struct MainGameFlowStateMarketingScreens : LoadingScriptedState { void Render() override; };
struct MainGameFlowStateCheckDiskSpace : LoadingScriptedState { void Render() override; };
struct MainGameFlowStateMemoryCard : LoadingScriptedState { void Render() override; };
struct MainGameFlowStateCompleteLoading : LoadingScriptedState { void Render() override; };
struct MainGameFlowStateInGame : LoadingScriptedState { void Render() override; };
namespace BrnGame {
class BrnGameModule {
public:
    struct FlowBoundary {
        LoadingScriptedState* state = nullptr;
        LoadingScriptedState* GetState(BrnGameMainFlowController::EMainGameFlowState) { return state; }
    } mMainFlowStateMachine;
    struct GuiBoundary { void* GetViewInputBuffer() { return this; } } mGuiModule;
    void* mpGuiInputBuffer = this;
    void* mpGuiOutputBuffer = this;
    void* mpGuiModelOutputBuffer = this;
    void* mpNetworkOutputBuffer = nullptr;
    void DoDispatch() {
        Check(finalIoLive, "stage8 world producer consumes the same live final-step IO");
        ++worldProduces;
    }
    void HostLoadingProducer(BrnGameMainFlowController::EMainGameFlowState leState) {
        ++guiUpdates; ++gameConsumers;
#include "loading_host_admission.inc"
    }
};
static BrnGameModule* gpFixture;
BrnGameModule* GetMainGameModule() { return gpFixture; }
}
#include "loading_virtual_renders.inc"

int main() {
    using namespace BrnGameMainFlowController;
    BrnGame::BrnGameModule game; BrnGame::gpFixture = &game;
    MainGameFlowStateInitialLoadingScreen initial;
    MainGameFlowStateCheckDiskSpace disk;
    MainGameFlowStateMarketingScreens marketing;
    MainGameFlowStateStartScreen start;
    MainGameFlowStateMemoryCard card;
    MainGameFlowStateCompleteLoading complete;
    MainGameFlowStateInGame ingame;
    // OnEnter asserts/parks the initial global stage at0; it does not reset it.
    gBrnScriptedLoadStage = 0;
    game.mMainFlowStateMachine.state = &initial;
    for (int stage : {1, 2, 3, 9}) {
        initial.meLoadingScreenStage = stage;
        const unsigned before = guiProduces + worldProduces;
        finalIoLive = true; game.HostLoadingProducer(E_MGS_INITIAL_LOADING_SCREEN); initial.Render(); finalIoLive = false;
        Check(guiProduces + worldProduces == before + (stage > 2), "initial own stage2 suppresses helper; stages3+ produce once");
    }
    LoadingScriptedState* states[] = {&disk, &marketing, &start, &card, &complete};
    for (unsigned state = 0; state < 5; ++state) for (int stage : {7, 8}) {
        game.mMainFlowStateMachine.state = states[state];
        // Network allocation used the previous stage7; the state update may
        // advance7->8 before admission and virtual Render in this same step.
        game.mpNetworkOutputBuffer = nullptr;
        gBrnScriptedLoadStage = stage;
        const unsigned beforeGui = guiProduces, beforeWorld = worldProduces;
        finalIoLive = true;
        game.HostLoadingProducer(static_cast<EMainGameFlowState>(state + 1));
        states[state]->Render();
        finalIoLive = false;
        Check(guiProduces == beforeGui + (stage != 8), "current stage8 suppresses GUI helper despite stale null Network ownership");
        Check(worldProduces == beforeWorld + (stage == 8), "shared loading virtual Render produces world exactly once at stage8");
        Check(guiProduces + worldProduces == beforeGui + beforeWorld + 1, "loading path has one completed command producer across7->8");
    }
    game.mMainFlowStateMachine.state = &ingame;
    for (int stage : {0, 7, 8}) {
        gBrnScriptedLoadStage = stage;
        const unsigned beforeGui = guiProduces, beforeWorld = worldProduces;
        finalIoLive = true; game.HostLoadingProducer(E_MGS_IN_GAME); ingame.Render(); finalIoLive = false;
        Check(guiProduces == beforeGui && worldProduces == beforeWorld + 1, "in-game uses only its original world Render producer");
    }
    Check(guiUpdates == 17 && gameConsumers == 17, "existing zero-argument GUI update and game-consumer timing is preserved");
    std::printf("PCLoadingCommandProducer: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
