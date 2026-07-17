#pragma once

#include "types.hpp"

// Real member types (reconstructed). The renderer module owns the loading-screen renderer;
// the remaining subsystems below are the ones the per-frame update spine (GameMain) drives.
#include "GameSource/Graphics/BrnRendererModule.h"                       // BrnGraphics::BrnRendererModule
#include "GameSource/Gui/BrnGuiModule.h"                                 // BrnGui::GuiModule (hosts the MovieManager)
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"        // CgsModule::ModuleSingleBuffered (real base)
#include "GameShared/GameClasses/System/Threads/CgsThreadLayout.h"       // CgsSystem::IThreadClass
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"              // CgsModule::IOBufferStack
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"// CgsDev::PerfMonCpu
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h" // CgsDev::DebugManager
#include "GameShared/GameClasses/System/Timer/CgsFrameRate.h"           // CgsSystem::FrameRateManager
#include "GameSource/Game/BrnGlobalCpuMonitors.h"                        // BrnGame::BrnCpuMonitors
#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowController.h" // GameMainFlowController + flow states
#include "SharedClasses/BrnSharedConstants.h"                            // BrnUpdateSet
#include "GameSource/Game/X360/BrnSystemHWX360.h"                        // BrnHW::System360HW (embedded hardware, by value)

// The GameTalk message type used by the RenderMetricsMessageHandler receiver signature.
// A forward declaration suffices for the pointer-parameter method decl; the .cpp includes
// the full SDKs/EA/GameTalk/GameTalk.h.
namespace EA { namespace GameTalk { class GameTalkMessage; } }

// Per-frame IO-buffer payload types the game module pushes/pops on the update IO stacks.
// PLACEHOLDER (empty) definitions so the loading-screen build can allocate them; the real
// layouts are owned by each module's IO TU and replace these when reconstructed.
// (The former ViewIO::InputBuffer placeholder is DELETED per the ODR-trap rule below:
// the REAL CgsGui::ViewIO::InputBuffer now arrives via BrnGuiModule.h ->
// CgsGuiViewModuleIO.h, so the IO stack allocates the real 65KB buffer. The former
// CgsGuiModuleIO placeholders are DELETED the same way: the REAL InputBuffer/OutputBuffer
// arrive via CgsGuiModuleIO.h below -- the controller bridge pushes real GUI events into
// the real 33KB inbound queue now.)
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"
// (The former ModelIO::OutputBuffer placeholder is DELETED per the same ODR-trap rule:
// the REAL CgsGui::ModelIO::OutputBuffer now arrives via BrnGuiModule.h ->
// CgsModelModuleIO.h -- the IO stack allocates the real 86KB buffer.)
namespace BrnDirector { namespace DirectorIO { struct OutputBuffer {}; } }

// Engine module member types - placeholders that derive from the REAL module base
// (CgsModule::ModuleSingleBuffered), so the game module addresses + drives them through the
// real polymorphic module lifecycle (Construct/Prepare/Release/Destruct/Update). The bodies
// of each module are large subsystems reconstructed separately; deriving from the real base
// is the contract that the BrnGameModule cascade funcs depend on. The former stub RWMutex
// clash is resolved (the renderer/game-module now share the one real base + EA::Thread::RWMutex).
// Names/namespaces/order are the real ones (DWARF BrnGameModule.h h:356-368). WorldModule is in
// the global namespace (DWARF BrnWorldModule.h: struct WorldModule : CgsModule::ModuleSingleBuffered).
class WorldModule : public CgsModule::ModuleSingleBuffered {};
// GameDataModule is now the REAL reconstruction (was an empty stub here, which ODR-clashed with the
// real BrnGameDataModule.h -- different sizeof/ctor in different TUs -> the empty ctor got linked,
// leaving mResourceModule unconstructed = null vtable crash). Include the real definition so every TU
// (BrnGameModule + the loading flow) sees ONE GameDataModule.
#include "GameSource/Resource/BrnGameDataModule.h"
// !!! ODR TRAP WARNING !!! The remaining empty stubs below (GameStateModule/DirectorModule/InputModule/
// EffectsModule/RootSoundModule/ReplayModule/BrnNetworkModule/WorldModule) are placeholders so this
// module can embed them by value. When ANY of them is reconstructed for real in its own header, DELETE
// the matching stub here and #include the real header instead -- do NOT leave both. Two different
// definitions of the same class = ODR violation: the linker silently binds the empty do-nothing ctor,
// leaving the member unconstructed (null vtable) -> crash. (That exact bug cost a debugging session on
// GameDataModule; see memory gamedatamodule-scoping "ODR-STUB TRAP".)
// Forward declarations for the controller-bridge family parameter types (declared-only here; the
// bridge bodies in GameBridgeControllerToX.cpp include the real homes). Keeping these as forward
// decls avoids pulling the heavy IO headers into this keystone header.
// CgsInput::InputIO is included for REAL now (the module embeds mPcInputOutputBuffer by
// value -- the PC stand-in for the input module's per-frame output buffer). The extra
// bridge-family parameter types stay forward-declared alongside (harmless re-declaration
// for the ones the real header already defines).
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"
namespace CgsInput { namespace InputIO { struct PadOutputInformation; struct ActionInfo; struct PostWorldInputBuffer; } }
namespace BrnDirector { namespace DirectorIO { struct InputBuffer; } }
namespace BrnWorldIO { struct UpdateInputBuffer; }
namespace BrnGame { struct DebugControllerImage; }
namespace CgsGui { class GuiModule; }
// Replay-bridge family (GameSource/Unity/../Game/GameBridgeReplayToX.cpp) parameter type:
// the committed replay pre-sim OUTPUT buffer (home GameSource/Replays/BrnReplayModuleIO.h).
// Forward-declared here (the bridge body includes the real home) to keep the heavy replay IO
// header out of this keystone header.
namespace BrnReplays { namespace ReplayIO { struct OutputBuffer_PreSim; } }
// Network-bridge family (GameSource/Unity/../Game/GameBridgeNetworkToX.cpp) parameter type:
// the committed network OUTPUT buffer (home GameSource/Network/BrnNetworkModuleIO.h). Forward-
// declared here (the bridge body includes the real home) to keep the heavy network IO header out
// of this keystone header.
namespace BrnNetwork { namespace BrnNetworkModuleIO { struct OutputBuffer; } }
// Sound-bridge family (GameSource/Unity/../Game/GameBridgeSoundToX.cpp) parameter/member types:
// the sound root pre-update OUTPUT buffer (home GameSource/Sound/Module/SharedIO/
// BrnSoundRootSharedIO.h) and the training manager (home GameSource/GameState/TrainingManager/
// BrnTrainingManager.h). Forward-declared; the bridge body includes the real homes.
namespace BrnSound { namespace Module { namespace Io { struct RootPreUpdateOutputBuffer; } } }
namespace BrnGameState { class TrainingManager; }
// (CgsGui::CgsGuiModuleIO::InputBuffer -- the GUI input buffer the ToGui bridge fills -- is the
//  REAL type now, included above via CgsGuiModuleIO.h.)
// GUI-output bridge family (GameSource/Unity/../Game/GameBridgeGUIToX.cpp) parameter types: the
// replay post-sim INPUT buffer (home GameSource/Replays/BrnReplayModuleIO.h) and the sound root
// INPUT buffer (home GameSource/Sound/Module/BrnRootSoundModuleIo.h). Forward-declared here (the
// bridge body includes the real homes) to keep the heavy IO headers out of this keystone header.
namespace BrnReplays { namespace ReplayIO { struct InputBuffer_PostSim; } }
namespace BrnSound { namespace Module { namespace Io { struct RootInputBuffer; } } }
// The event-translating bridges (BridgeGuiToGameState / TranslateGuiEventsToNetworkEvents /
// BridgeGuiToGame) take/return CgsModule::VariableEventQueue<N,16> pointers. Forward-declared
// here (the bridge bodies include the real CgsVariableEventQueue.h) to keep the heavy template
// header out of this keystone header.
namespace CgsModule { template <s32 BUFSIZE, s32 ALIGN> class VariableEventQueue; }

// BrnGameState::GameStateModule is now the REAL (minimal-slice) class -- included below per this
// header's own ODR-trap instruction (the TrainingManager/BurnoutSkillz/ModeManager TUs include the
// real BrnGameStateModule.h; keeping the empty stub here alongside them was the exact silent-ODR
// hazard the warning above describes).
#include "GameSource/GameState/BrnGameStateModule.h"
namespace BrnDirector  { class DirectorModule  : public CgsModule::ModuleSingleBuffered {}; }
namespace CgsInput     { class InputModule     : public CgsModule::ModuleSingleBuffered {}; }
// BrnGui::GuiModule is now the REAL module (hosts the MovieManager) -- included above (BrnGuiModule.h),
// no longer the opaque stub.
namespace BrnEffects   { class EffectsModule   : public CgsModule::ModuleSingleBuffered {}; }
#include "GameSource/Sound/Module/BrnRootSoundModule.h"   // BrnSound::Module::RootSoundModule (real class)
#include "GameSource/Replays/BrnReplayModule.h"   // BrnReplays::ReplayModule (real class -- was an ODR stub)
namespace BrnNetwork   { class BrnNetworkModule : public CgsModule::ModuleSingleBuffered {}; }

namespace BrnGame
{
    // BrnGame::BrnGameModule - the top-level game module. It owns every engine module and is
    // the engine's IThreadClass implementor (the update/dispatch/resource threads call back
    // into it). Reconstructed from the X360 ARTIST build.
    //
    // LAYOUT NOTE: this is the real ~10MB object, but it is populated incrementally - only
    // the members the reconstructed functions actually reference are declared, in their real
    // declaration order. Ranges of not-yet-reached members are marked "[... omitted ...]"
    // and are added as the functions that touch them are reconstructed. Semantic parity (not
    // byte-exact): real names/types/order, but omitted members mean offsets are not byte-exact.
    class BrnGameModule : public CgsModule::ModuleSingleBuffered, public CgsSystem::IThreadClass
    {
    public:
        // The game module owns the GameDataModule; the loading flow (case 8) prepares it through here
        // (and via BrnGame::GetMainGameDataModule()) so there's ONE instance, not a parallel copy.
        BrnResource::GameDataModule& GetGameDataModule() { return mGameDataModule; }
        BrnSound::Module::RootSoundModule& GetSoundModule() { return mSoundModule; }

        // Per-frame spines the in-game flow state drives directly (non-virtual; each returns
        // an int status the void flow-state Update/Render slots discard):
        //   DoUpdate   -- the game module's per-frame update spine  (MainGameFlowStateInGame::Update)
        //   DoDispatch -- the game module's per-frame render-dispatch spine (MainGameFlowStateInGame::Render)
        int DoUpdate();
        int DoDispatch();

        // The per-frame update IO stacks. The scripted module loads (LoadingScriptedState::
        // LoadXxxModule) create their per-frame module IO buffers on these, exactly as the
        // X360 reads them off the game-module global (e.g. LoadSoundModule 0x823E75A8).
        CgsModule::IOBufferStack* GetUpdateInputBufferStack()  { return mpUpdateInputBufferStack; }
        CgsModule::IOBufferStack* GetUpdateOutputBufferStack() { return mpUpdateOutputBufferStack; }

        enum EGameUpdateStage   // h:248
        {
            E_GAMEUPDATESTAGE_PREPARE = 0,
            E_GAMEUPDATESTAGE_MAIN = 1,
            E_GAMEUPDATESTAGE_RELEASE = 2,
        };

        enum EReleaseStage   // h:210 - the resumable Release() stage machine
        {
            E_RELEASESTAGE_START = 0,
            E_RELEASESTAGE_GUI = 1,
            E_RELEASESTAGE_SOUND = 2,
            E_RELEASESTAGE_GAMEDATAMODULE = 3,
            E_RELEASESTAGE_MANAGER = 4,
            E_RELEASESTAGE_HARDWARE = 5,
            E_RELEASESTAGE_NETWORK = 6,
            E_RELEASESTAGE_DONE = 7,
        };

        BrnGameModule();
        ~BrnGameModule();

        void Construct() override;   // @ BrnGameModule.cpp:155 - construct the owned modules

        // @ BrnGameModule.cpp:1047 - tear down the owned modules + the module base. Overrides
        // CgsModule::Module::Destruct. Destructs all 11 engine modules (in the X360's destruct
        // order) then the module base. Not on the boot/loading path (the game doesn't shut down
        // during the loading screen).
        void Destruct() override;

        // @ BrnGameModule.cpp:925 - resumable staged release (counterpart of Prepare). Each call
        // advances meReleaseStage, releasing GUI -> sound -> hardware(+input) -> game-data ->
        // module base -> network; returns false to be re-driven, true at DONE. Not on the
        // boot/loading path.
        bool Release() override;

        // Debug helper: fill liNumBytes at lpDest with the repeating 32-bit pattern luValue.
        void DebugSetMemoryToInt(void* lpDest, s32 liNumBytes, u32 luValue);  // @ BrnGameModule.cpp:3872
        // Debug: clear the whole game-module object then stamp the per-module memory regions
        // with a marker so unwritten reads are caught.
        void DebugMemoryInit(BrnGameModule* lpData);  // @ BrnGameModule.cpp:3916

        // IThreadClass implementation (the engine drives these on their threads).
        bool UpdateThread() override;
        void DispatchThread() override;                               // @ BrnGameModule.cpp:1221
        void ResourceUpdateThread(Mutex* lpMutex) override;
        void OnStartOfUpdateFrame() override;
        void OnEndOfUpdateFrame() override;
        void OnCompletionOfVsyncWait() override;
        void RenderAssert(const AssertData* lpAssertData) override;

    private:
        // @ BrnGameModule.cpp:1845 - the per-frame update spine: latch frame-rate stepping,
        // begin the frame-rate frame, tick the debug manager, then run the active flow
        // state's Update() once per required simulation sub-step (recreating the static IO
        // buffers between steps), and finally its Render(); tear down the per-frame IO
        // buffers. Returns false (the X360 returns 0).
        bool GameMain();

        // GameMain callees + sim-step plumbing (reconstructed).
        void         UpdateRequestDoStepFrame();   // @ BrnGameModule.cpp:4048
        void         CreateStaticIOBuffers();      // @ BrnGameModule.cpp:2497
        void         DestroyStaticIOBuffers();     // @ BrnGameModule.cpp:2515
        BrnUpdateSet ConstructUpdateSetFromFsm();  // @ BrnGameModule.cpp:3542

        // Debug step/play-frame callbacks (registered with the debug menu; the void* is the
        // game-module instance).
        static void StepFrameCB(void* lpContext);  // @ BrnGameModule.cpp:3981
        static void PlayFrameCB(void* lpContext);  // @ BrnGameModule.cpp:3996

        // UpdateThread stage machine targets (reconstructed as their own heavy TUs).
        bool GamePrepare();  // @ BrnGameModule.cpp:1580
        bool GameRelease();  // @ BrnGameModule.cpp:2650

    public:
        // ---- controller-input bridge family (GameSource/Unity/../Game/GameBridgeControllerToX.cpp) --
        // Each per-frame bridge reads player-0's pad record (via CgsInput OutputBuffer::GetPadInfo)
        // and publishes it into a subsystem's input buffer. Signatures recovered from the X360
        // call sites (the leading int args are subsystem InputBuffer* / the input OutputBuffer*).
        // The pad records / GUI events / debug-controller image these touch live in
        // GameBridgeControllerToX.h (several are FLAGGED un-homed placeholders).

        // X360 0x823C0EA0 -- locate player-0's pad record. Returns the read-locked
        // PadOutputInformation* for the player-0 controller port (or null if no controller is
        // assigned), and writes the resolved port into *lpOutPort. Shared helper for the 5 bridges.
        const CgsInput::InputIO::PadOutputInformation* GetPadInfoForPlayer0(
            const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer, s32* lpOutPort);

        // X360 0x823AA580 -- copy the player's 22 pad ActionInfo slots into a debug-controller image.
        void MapActionInfoToDebugController(BrnGame::DebugControllerImage* lpImage,
                                            const CgsInput::InputIO::ActionInfo* lpActionInfo);

        // X360 0x823C0F70 -- publish player-0 controller state into the Director input buffer.
        void BridgeControllerToDirector(BrnDirector::DirectorIO::InputBuffer* lpDirectorInput,
                                        const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer);

        // X360 0x823CD890 -- publish player-0 controller state into the World update input buffer.
        void BridgeControllerToWorld(BrnWorldIO::UpdateInputBuffer* lpWorldInput,
                                     const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer);

        // X360 0x823CD738 -- publish player-0 controller state into the GameState PreWorld input
        // buffer (+ merge the bind/unbind result queues). FLAG: ledger dest is
        // GameSource/GameState/BrnGameStateModuleIO.h, but as a BrnGameModule method it co-locates
        // with its siblings + the shared GetPadInfoForPlayer0 helper in GameBridgeControllerToX.cpp.
        void BridgeControllerToGameState(BrnGameState::GameStateModule* lpGameStateModule,
                                         const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer,
                                         s32 liActionContext);

        // X360 0x823E6B18 -- synthesise GUI events from player-0 (and, when no player-0 controller is
        // assigned, the cross-pad menu-accept scan) and push them through the GUI module's
        // AddGuiEvent<T> into the GUI module INPUT buffer (the mangled name pins the third
        // AddGuiEvent arg as CgsGuiModuleIO::InputBuffer*).
        void BridgeControllerToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInputBuffer,
                                   const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer);

        // X360 0x823C0AE8 -- merge the game-state output's input-bind / input-unbind REQUEST queues
        // into the input module's post-world input buffer (PostBindRequest / PostUnbindRequest per
        // queued request), stamping miInputModuleState with the bind (3) / unbind (6) sentinel when
        // either queue produced work. Called by DoUpdate_InputPostWorld. Home
        // GameSource/Game/GameBridgeGameStateToX.cpp.
        void BridgeGameStateToController(BrnGameState::GameStateModule* lpGameStateOutput,
                                         CgsInput::InputIO::PostWorldInputBuffer* lpPostWorldInput);

        // X360 0x823E1C38 -- walk the game-state output's TakedownEvent output queue and push one
        // GUI takedown event per record through the CgsGui GUI module (this + 7252512). A record
        // whose race-car index matches the runner index, or when the "soft takedown display" flag
        // bit is clear, becomes a BrnGui::GuiTakedownEvent; otherwise a BrnGui::GuiSoftTakedownEvent.
        // Called by BridgeGameStateToGui. Home GameSource/Game/GameBridgeGameStateToX.cpp.
        void TranslateTakedownsToGuiEvents(
            CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
            const void* lpTakedownQueue,
            s32 liRunnerActiveRaceCarIndex);

        // X360 0x823DCA10 -- the game->GUI flow-FSM bridge: when the main flow requested a
        // GUI FSM stage (miGuiFsmStage 1..5), post the matching GuiEventRunFsm record(s)
        // (event 144, 24 bytes) into the GUI module INPUT buffer's inbound queue, then park
        // the stage at 6 (idle) and clear the phase-complete flag:
        //   1 -> BrnVideoFsm (HUD)      2 -> BrnLegalFsm (HUD)     3 -> BrnCmpLdFsm (HUD)
        //   4 -> BrnBFProFsm (HUD)      5 -> BrnScreenFsm@LOADING (SCREEN) + BrnFBFsm (HUD)
        void BridgeGameToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInputBuffer);

        // X360 0x823CB758 -- the GUI->game out-event consumer: walk the GUI module's out
        // events and latch the flow commands (19/20 loading screen, 70 phase complete,
        // 71 pre-accept/resume-world, 90 profile-first, 189 sign-out, 86/87/89 quit,
        // 138/589/590 render modes). FLAG PC-ABI adapter: the console form reads the GUI
        // module OUTPUT buffer's out-event queue; the PC module publishes the same queue
        // via BrnGui::GuiModule::GetGuiOutQueue().
        void BridgeGuiToGame(CgsModule::VariableEventQueue<18432, 16>* lpGuiOutQueue);

        // The main-flow states' pending GUI FSM stage request (the X360 +2523537 byte the
        // MainGameFlowState OnEnters write; BridgeGameToGui consumes it).
        void RequestGuiFsmStage(s32 liStage) { miGuiFsmStage = liStage; }

        // The GUI phase-complete flag (X360 +10094152; command 70). Stays latched until
        // the next BridgeGameToGui stage post clears it -- the X360 lifecycle.
        bool IsGuiPhaseComplete() const { return mbGuiPhaseComplete; }

        // The pre-accept flag (X360 +10094153; command 71 -- "resume the world load while
        // the accept dwell plays"). Read-and-clear, as MainGameFlowStateStartScreen does.
        bool ConsumeGuiPreAccept()
        {
            const bool lbSet = mbGuiPreAccept;
            mbGuiPreAccept = false;
            return lbSet;
        }

        // This sub-step's GUI module INPUT buffer (live between CreateStaticIOBuffers /
        // DestroyStaticIOBuffers; the flow states' initial-FSM post reaches it here).
        CgsGui::CgsGuiModuleIO::InputBuffer* GetGuiInputBuffer() { return mpGuiInputBuffer; }

        // ---- replay-output bridge family (GameSource/Unity/../Game/GameBridgeReplayToX.cpp) -------
        // The mirror of the controller bridges: each reads the replay module's pre/post-sim OUTPUT
        // buffer and republishes it into a subsystem's INPUT buffer.
        //
        // X360 0x823E7210 -- snapshot the replay status interface into a GuiReplayStatusEvent (type
        // 514) pushed through the GUI module, then bulk-append the replay output buffer's GUI event
        // queue into the GUI input buffer's inbound queue. lpGuiInput is the CgsGui GUI input buffer
        // (placeholder type); lpReplayOutput is the committed replay pre-sim output buffer.
        void BridgeReplayToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
                               const BrnReplays::ReplayIO::OutputBuffer_PreSim* lpReplayOutput);

        // ---- GUI-output bridge family (GameSource/Unity/../Game/GameBridgeGUIToX.cpp) -------------
        // Each per-frame bridge walks the GUI output buffer's out-event queue (a
        // VariableEventQueue<18432,16> @ +0x814 of CgsGui::CgsGuiModuleIO::OutputBuffer) and
        // re-publishes the queued GUI events into a downstream subsystem's INPUT buffer.
        //
        // X360 0x823CCA00 -- copy the replay-related GUI events (ids 349 / 525..533 / 594) into the
        // replay module's post-sim GUI event queue, and register the case-595 serialiser with the
        // replay request interface.
        void BridgeGuiToReplay_PostSim(BrnReplays::ReplayIO::InputBuffer_PostSim* lpReplayModuleInputBuffer,
                                       const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer);

        // X360 0x823C0A58 -- hand the GUI output buffer's out-event queue to the sound root input
        // buffer (SetGuiEventQueue) so the sound module can drain GUI events.
        void BridgeGuiToSound(BrnSound::Module::Io::RootInputBuffer* lpSoundModuleInputBuffer,
                              const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer);

        // X360 0x823DDB78 -- drain the GUI output buffer's out-event queue (VariableEventQueue<18432,16>)
        // and translate each recognised GUI event into the matching game-state event, AddEvent'd into the
        // game-state module's post-world input GameEventQueue (VariableEventQueue<1536,16>). A handful of
        // events also drive the training manager (RequestTraining / OnEnableTrainingTips). Returns the
        // final queue-walk status. Called by DoUpdate_GameStatePostWorld / LoadingScriptedState::Update.
        int BridgeGuiToGameState(BrnGameState::GameStateModule* lpGameStateInput,
                                 const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer);

        // X360 0x823DEEB8 -- drain a GUI event queue (VariableEventQueue<18432,16>) and translate each
        // network-bound GUI event into the matching network IN-event, AddEvent'd into the network module's
        // in-event queue (VariableEventQueue<14000,16>). Returns the final queue-walk status. Called by
        // DoUpdate_NetworkPostSim.
        int TranslateGuiEventsToNetworkEvents(CgsModule::VariableEventQueue<14000, 16>* lpNetworkInputQueue,
                                              const CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue);

        // ---- network-output bridge family (GameSource/Unity/../Game/GameBridgeNetworkToX.cpp) -----
        // The mirror of the controller/replay bridges for the network module: each reads the network
        // module's OUTPUT buffer (BrnNetwork::BrnNetworkModuleIO::OutputBuffer + its interfaces) and
        // republishes it into the GUI + game-state subsystems. lpGuiBuffer is the CgsGui GUI IO buffer
        // pointer threaded to CgsGui::GuiModule::AddGuiEvent (placeholder type -> void*).
        //
        // X360 0x823E9518 -- top-level per-frame network->GUI bridge: run both translators below, bulk-
        // append the network GUI event queue into the GUI input queue, then synthesise the three player-
        // snapshot GUI events (player-list / player-status / lobby-player-list). Called by DoUpdate_GUI /
        // LoadingScriptedState::Update.
        int BridgeNetworkToGui(void* lpGuiBuffer,
                               const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutput);

        // X360 0x823DF9E0 -- drain the network event queue (VariableEventQueue<14000,16>) and translate
        // each recognised network event into the matching game-state event, AddEvent'd into the PreWorld
        // input buffer's game-event queue (VariableEventQueue<1536,16>). Called by BridgeNetworkToGameState.
        int TranslateNetworkEventsToGameEvents(BrnGameState::GameStateModule* lpGameStateModule,
                                               const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutput);

        // X360 0x823E0900 -- drain the same network event queue and translate each GUI-bound network
        // event into the matching GUI event, pushed through the GUI module (this + 7252512).
        int TranslateNetworkEventsToGuiEvents(void* lpGuiBuffer,
                                              const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutput);

        // X360 0x823DF938 -- walk the network output's live-revenge update interface and push one
        // BrnGui::GuiLiveRevengeUpdateEvent per record through the GUI module.
        int TranslateNetworkInterfaceToGuiEvents(void* lpGuiBuffer, const void* lpNetworkToGuiInterface);

    private:
        // Case-52 helper for TranslateNetworkEventsToGuiEvents: scoreboard-response heading sub-switch
        // (category/variation/index) -- copy the per-name string list into the matching scoreboard event
        // (with the CgsStringUtils "String too long" guard) and push it. Not a distinct X360 function
        // (inlined into 0x823E0900); factored out here for the switch's readability.
        void TranslateScoreboardResponse(void* lpGuiBuffer, const unsigned char* lpRecord);
    public:

        // ---- sound-output bridge family (GameSource/Unity/../Game/GameBridgeSoundToX.cpp) ---------
        // X360 0x823C63C0 (DWARF BrnGameModule.h:799 / GameBridgeSoundToX.cpp:103) -- walk the sound
        // root pre-update output's audio-effects message queue and notify the training manager of
        // each finished voiceover. The family's other bridges (BridgeSoundToResource :41 /
        // BridgeSoundToGuiPreUpdate :134 / ...) are grown here when reconstructed.
        void BridgeSoundToTraining(BrnSound::Module::Io::RootPreUpdateOutputBuffer* lpSoundOutputBuffer);

        // ---- X360 hardware / boot-legal query helpers (BrnGameModule.cpp) ----------------
        // X360 0x823A8B38 -- disk-error worker thread body: raise the system dirty-disc
        // error UI for dwUserIndex; never returns. Static (a thread proc; no `this`).
        static void DiskErrorThreadProc(unsigned long dwUserIndex);

        // X360 0x823C0268 -- return the embedded hardware object (the launch/soft-reboot
        // data block). The X360 forms this + 0x9A11C0 == &mHardware.
        BrnHW::System360HW* GetSoftRebootData();

        // X360 0x823C0288 -- forward to the hardware object's invite-reboot query.
        // (X360 BOOL == int; the returned bool widens faithfully.)
        int HasGameBeenRebootedDueToInvite();

        // X360 0x823C0278 -- the soft-reboot flag (reads mHardware.mPad94, the launch-data
        // flag byte at mHardware + 0x94 == game-module + 0x9A1254).
        int HasGameBeenSoftRebooted();

        // X360 0x823A9030 -- GameTalk "StopRenderMetrics" receiver: walk the message keys
        // and clear miRenderMetricsRequested on the StopRenderMetrics key. Static (the
        // context module is delivered by pointer-to-pointer). Same receiver shape as
        // BrnDirector::Camera::BehaviourRenderMetrics::GameTalkMessageReceiver.
        static void RenderMetricsMessageHandler(EA::GameTalk::GameTalkMessage* lpMessage,
                                                BrnGameModule** lppModule);
    private:

        // ---- members (real order; off-path ranges omitted; see LAYOUT NOTE) -------------
        // ---- the 11 engine modules (real names/types/order; all derive from the module base) --
        BrnRendererModule mRenderModule;                             // h:356
        WorldModule mWorldModule;                                    // h:357
        BrnResource::GameDataModule mGameDataModule;                 // h:358
        BrnGameState::GameStateModule mGameStateModule;              // h:359
        BrnDirector::DirectorModule mDirectorModule;                 // h:360
        CgsInput::InputModule         mInputModule;                  // h:361
        BrnGui::GuiModule             mGuiModule;                    // h:362
        BrnEffects::EffectsModule     mEffectsModule;                // h:363
        BrnSound::Module::RootSoundModule mSoundModule;              // h:364
        BrnReplays::ReplayModule      mReplayModule;                 // h:365
        // [h:366-367: omitted]
        BrnNetwork::BrnNetworkModule  mNetworkModule;                // h:368
        CgsModule::IOBufferStack* mpUpdateInputBufferStack;          // h:373
        CgsModule::IOBufferStack* mpUpdateOutputBufferStack;         // h:374
        // [h:375-379: resource buffer stacks + gamedata module buffers - omitted]
        BrnGame::BrnCpuMonitors mCpuMonitors;                        // h:384
        CgsDev::DebugManager    mDebugManager;                       // h:387
        // [h:388-404: debug font, map-file reader, juice, mbHasGameTerminated - omitted]
        bool mbSimPaused;                                            // h:405
        bool mbDiskError;                                            // h:406
        // [h:407-411: disk-error/controller/game-start/over/prev-stalled flags - omitted]
        bool mbStalled;                                              // h:412
        bool mbRequestDoStepFrame;                                   // h:413
        bool mbRequestDoPlayFrame;                                   // h:414
        // [h:415-453: streaming flags, input-bind state, gui-flow, prepare stages - omitted]
        EReleaseStage    meReleaseStage;                             // h:455 (Release() stage machine)
        // [h:456-457: meGamePrepareStage, meGameReleaseStage - omitted]
        EGameUpdateStage meGameUpdateStage;                          // h:458
        s32 miNumSimFramesRequired;                                  // h:459
        BrnGameMainFlowController::GameMainFlowController mMainFlowStateMachine; // h:462
        // [h:465-471: receiver queue, game/sim timers, timer interfaces - omitted]
        // X360 reads two frame-timing fields in the timer-status region and multiplies them
        // to form the debug-manager delta time; reconstructed as these two members until the
        // Timer subsystem is reconstructed.
        f32 mfDebugUpdateDeltaSeconds;
        f32 mfDebugUpdateTimeScale;
        CgsSystem::FrameRateManager      mFrameRateManager;          // h:472
        CgsSystem::EFrameRateManagerType meFrameRateManagerType;     // h:473
        // [h:474: frame-rate type request interface - omitted]
        s8   mi8FrameRateMinSteps;                                   // h:475
        s8   mi8FrameRateMaxSteps;                                   // h:476
        s8   mi8ActualFrameRateMinStepsThisFrame;                    // h:477
        s8   mi8ActualFrameRateMaxStepsThisFrame;                    // h:478
        bool mbSteppingFrames;                                       // h:479
        bool mbDoStep;                                               // h:480
        bool mbStopStepping;                                         // h:481
        // [h:482-490: force-shutdown, debug-framerate stats - omitted]
        CgsGui::CgsGuiModuleIO::InputBuffer*   mpGuiInputBuffer;     // h:493
        CgsGui::ViewIO::InputBuffer*           mpGuiViewInputBuffer; // h:494
        CgsGui::ModelIO::OutputBuffer*         mpGuiModelOutputBuffer;// h:495
        CgsGui::CgsGuiModuleIO::OutputBuffer*  mpGuiOutputBuffer;    // h:496
        BrnDirector::DirectorIO::OutputBuffer* mpDirectorOutputBuffer;// h:497
        // [remaining members - omitted]

        // ---- controller-bridge inputs (real names; off-path absolute offsets noted) -------------
        // These are the game-module fields the BridgeControllerTo* family reads each frame. They sit
        // far past the boot-path members above (X360 absolute offsets in comments); declared here in
        // the omitted tail per the LAYOUT NOTE (semantic parity, not byte-exact).
        // ---- the GUI flow-FSM bridge state (X360-attested offsets) -----------------------
        s32  miGuiFsmStage;             // @ +10094148 (1..5 = pending RunFsm post; 6 = idle)
        bool mbGuiPhaseComplete;        // @ +10094152 (command 70 -- the flow-advance flag)
        bool mbGuiPreAccept;            // @ +10094153 (command 71 -- resume-world-load)

        s32  miInputModuleState;        // @ +10094136 (==4 means input module ready / player-0 assigned)
        s32  miPlayer0ControllerPort;   // @ +10094140 (asserted <= CgsInput::KU_NUMBER_OF_PADS)
        s32  miSecondaryControllerPort; // @ +10094144 (the rumble/debug-controller read port)
        bool mbGuiAcceptsControllerInput;// @ +10094266 (gates the ToGui change-car / menu-accept events)
        bool mbGuiSuppressMenuAccept;    // @ +10095408 (when set, the menu-accept synthesis is skipped)
        // The ToGui menu-repeat / language-cycle clock pair: an integer-seconds word (the X360
        // widens it u32->f64 with the LODWORD/HIDWORD idiom) plus a fractional-seconds float
        // added straight in. Fed once per update frame (FLAG PC time source: the console's
        // words ride the game module's own timer pass, unreconstructed).
        s32  miLanguageCycleTimerLo;     // @ +10095388 (whole seconds)
        f32  mfLanguageCycleTimerFrac;   // @ +10095392 (fractional seconds, added as float)

        // [PC stand-in] The input module's per-frame OUTPUT buffer the controller bridges read.
        // On the X360 this comes off the module scheduler's IO stacks and is filled by the
        // input module's own pass (InputPads::Update -> binding tables); that pass is
        // unreconstructed, so the game module owns one buffer here and the InputPadsPC
        // platform leaf fills the player-0 record each update frame.
        CgsInput::InputIO::OutputBuffer mPcInputOutputBuffer;
        // NOTE: the CgsGui::GuiModule event sink the ToGui bridge targets is embedded at X360
        // +7252512; its AddGuiEvent<T> body never reads `this`, so the bridge calls it without
        // the embed until the CgsGui module object is constructed on PC (GuiFsmController-flow
        // follow-on; see CgsGuiModule.h).

        // The training manager the sound bridge notifies (X360 this + 6769456 == 0x674B30, inside
        // the game-state region). Held by pointer per the mpCgsGuiModule precedent so the keystone
        // header need not embed the manager by value. FLAG: real layout embeds it @ +6769456.
        BrnGameState::TrainingManager* mpTrainingManager; // @ +6769456

        // Takedown-display flag word (X360 64-bit field @ this + 6762752 == 0x673100, inside the
        // game-state region). TranslateTakedownsToGuiEvents tests bit 33 (mask 0x0000000200000000,
        // the X360 `li r,1; extldi r,r,64,33` rotate): when set the record becomes a SOFT takedown
        // event, otherwise a hard one. FLAG: modelled as a logical member (semantic parity, not
        // byte-exact) per the mpCgsGuiModule/mpTrainingManager precedent.
        u64 mu64TakedownDisplayFlags;    // @ +6762752 (0x673100)

        // Render-metrics-requested flag the GameTalk StopRenderMetrics receiver clears
        // (X360 word store @ this + 0x9A1060). Mirrors the sibling's
        // BehaviourRenderMetrics::miRenderMetricsRequested (s32).
        s32 miRenderMetricsRequested;    // @ +10096736 (0x9A1060)

        // The embedded Xbox-360 hardware-abstraction object (launch data, command line,
        // Massive memory sub-system, invite-reboot / soft-reboot flags). GetSoftRebootData
        // returns &mHardware, HasGameBeenRebootedDueToInvite forwards to
        // mHardware.HasGameBeenRebootedDueToInvite(), HasGameBeenSoftRebooted reads
        // mHardware.mPad94.
        BrnHW::System360HW mHardware;    // @ +10097088 (0x9A11C0); mPad94 @ +10097236 (0x9A1254)
    };

    // operator++(EReleaseStage&, int) @ 0x823A8AD8 (X360 ARTIST), defined in BrnGameModule.cpp.
    // POST-increment for the BrnGameModule::EReleaseStage stage machine (the trailing dummy int is
    // the post-fix marker): saves the old value, advances the stage, asserts the advanced value has
    // not run past E_RELEASESTAGE_DONE (=7), and returns the OLD value. Called by
    // BrnGame::BrnGameModule::Release to step meReleaseStage. Declared here near the enum (the enum
    // is BrnGameModule::EReleaseStage, public) so all Release-path users of the operator see it.
    BrnGameModule::EReleaseStage operator++(BrnGameModule::EReleaseStage& leStage, int);

    // The game's single BrnGameModule (the X360 game-module global off_830102D0). Defined in
    // BrnMain.cpp next to GetMainGameDataModule/GetMainSoundModule; the loading flow reads the
    // update IO stacks through it, as the X360 loads do off the global.
    BrnGameModule* GetMainGameModule();
}
