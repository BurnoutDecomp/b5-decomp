#pragma once

#include "types.hpp"

// Real member types (reconstructed). The renderer module owns the loading-screen renderer;
// the remaining subsystems below are the ones the per-frame update spine (GameMain) drives.
#include "GameSource/Graphics/BrnRendererModule.h"                       // BrnGraphics::BrnRendererModule
#include "GameSource/Gui/BrnGuiModule.h"                                 // BrnGui::GuiModule (hosts the MovieManager)
#include "GameShared/GameClasses/Module/CgsModuleSingleBufferedTemplate.h"
#include "GameSource/Game/BrnGameModuleIO.h"
#include "GameSource/Game/BrnAutoTestManager.h"
#include "GameShared/GameClasses/Development/MapFile/Reader/CgsMapFileReaderMinimalMemory.h"
#include "GameShared/GameClasses/System/Threads/CgsThreadLayout.h"       // CgsSystem::IThreadClass
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"              // CgsModule::IOBufferStack
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"// CgsDev::PerfMonCpu
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h" // CgsDev::DebugManager
#include "GameShared/GameClasses/System/Timer/CgsFrameRate.h"           // CgsSystem::FrameRateManager
#include "GameShared/GameClasses/System/Timer/CgsFrameRateTypeRequestInterface.h"
#include "GameSource/Graphics/BrnRendererModuleIO.h"
#include "GameShared/GameClasses/System/Timer/CgsTimer.h"               // CgsSystem::Timer (the game/sim pair, h:466-467)
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"// CgsSystem::TimerStatusInterface (h:468)
#include "GameSource/Game/BrnGlobalCpuMonitors.h"                        // BrnGame::BrnCpuMonitors
#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowController.h" // GameMainFlowController + flow states
#include "SharedClasses/BrnSharedConstants.h"                            // BrnUpdateSet
#include "GameSource/Game/X360/BrnSystemHWX360.h"                        // BrnHW::System360HW (embedded hardware, by value)
#include "GameSource/Game/BrnDispatchThreadInputBuffer.h"                // BrnGame::DispatchThreadInputBufferManager (by value, h:531)

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
// (The former DirectorIO::OutputBuffer placeholder -- an EMPTY `struct OutputBuffer {}` -- is
// DELETED per the same ODR-trap rule. It meant the update output stack allocated a ZERO-BYTE
// "Director" buffer while the real 1828-byte one (with the published camera at mCameraOutput)
// was defined in the director's own TU: two definitions of one type, and the game module got
// the empty one. The REAL buffer now arrives here.)
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIOOutputBuffer.hpp"

// Engine module member types - placeholders that derive from the REAL module base
// (CgsModule::ModuleSingleBuffered), so the game module addresses + drives them through the
// real polymorphic module lifecycle (Construct/Prepare/Release/Destruct/Update). The bodies
// of each module are large subsystems reconstructed separately; deriving from the real base
// is the contract that the BrnGameModule cascade funcs depend on. The former stub RWMutex
// clash is resolved (the renderer/game-module now share the one real base + EA::Thread::RWMutex).
// Names/namespaces/order are the real ones (DWARF BrnGameModule.h h:356-368). WorldModule is in
// the global namespace (DWARF BrnWorldModule.h: struct WorldModule : CgsModule::ModuleSingleBuffered).
// WorldModule is now the REAL reconstruction (world-render campaign mount 2026-07-26; was an empty
// stub here -- deleted per the ODR-TRAP rule below, exactly like GameDataModule). NAMESPACE
// RECONCILE: the DWARF declares WorldModule in the GLOBAL namespace (BrnWorldModule.h:124); the
// reconstruction homes it in BrnWorld. The using-declaration is the minimal-churn reconcile: the
// game module addresses the ONE real type under the DWARF's global name without renaming the
// class or moving its home file.
#include "GameSource/World/BrnWorldModule.h"
using BrnWorld::WorldModule;   // DWARF: global-namespace WorldModule (BrnWorldModule.h:124)
// GameDataModule is now the REAL reconstruction (was an empty stub here, which ODR-clashed with the
// real BrnGameDataModule.h -- different sizeof/ctor in different TUs -> the empty ctor got linked,
// leaving mResourceModule unconstructed = null vtable crash). Include the real definition so every TU
// (BrnGameModule + the loading flow) sees ONE GameDataModule.
#include "GameSource/Resource/BrnGameDataModule.h"
// !!! ODR TRAP WARNING !!! The remaining empty stubs below (GameStateModule/DirectorModule/
// EffectsModule/RootSoundModule/ReplayModule/BrnNetworkModule) are placeholders so this
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
namespace BrnWorldIO { struct UpdateInputBuffer; struct UpdateOutputBuffer; struct DispatchOutputBuffer; }
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
// The network post-sim INPUT buffer the GameState/World -> Network bridges fill (same home).
namespace BrnNetwork { namespace BrnNetworkModuleIO { struct PostSimulationInputBuffer; } }
// The network output's GUI interface and the scoreboard heading-list OUT record (homes
// SharedIO/BrnNetworkToGuiIOInterfaces.h and SharedIO/BrnNetworkOutScoreboardHeadingList.h).
namespace BrnNetwork { namespace BrnNetworkModuleIO { struct NetworkToGuiInterface; struct NetworkOutScoreboardHeadingList; } }
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
// DoUpdate_Sound @0x823DCEC0 parameter types (phase C4b) -- the sound root OUTPUT buffer,
// the replay pre-sim OUTPUT buffer, and the effects OUTPUT buffer. Forward-declared; the
// leg body includes the real homes.
namespace BrnSound { namespace Module { namespace Io { struct RootOutputBuffer; } } }
namespace BrnReplays { namespace ReplayIO { struct OutputBuffer_PreSim; } }
namespace BrnEffects { namespace EffectsIO { struct OutputBuffer; } }
// DoUpdate_GameStatePostWorld parameter type: the replay post-sim OUTPUT buffer (same home).
namespace BrnReplays { namespace ReplayIO { struct OutputBuffer_PostSim; } }
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
// BrnDirector::DirectorModule is now the REAL module (the director spine: MainDirector ->
// Arbitrator -> BehaviourManager -> the camera behaviours -> CameraFinaliser -> the output
// buffer). It was an ODR stub here exactly like GameStateModule/WorldModule were; the real
// header is included instead, per this header's own ODR-TRAP instruction above.
#include "GameSource/Director/BrnDirectorModule.h"
// CgsInput::InputModule is now the REAL module (FX-RUMBLE3 2026-09-24, crash-parity G10-D4): the
// pads, their jolt / rumble tables and PreWorldUpdate -> ProcessRumbleRequests. It was an empty ODR
// stub here (`class InputModule : public ModuleSingleBuffered {}`) exactly like the GameState /
// Director / Effects modules above were, so mInputModule had no PreWorldUpdate to call and the
// game state's rumble requests had no consumer.
#include "GameShared/GameClasses/Input/CgsInputModule.h"
// BrnGui::GuiModule is now the REAL module (hosts the MovieManager) -- included above (BrnGuiModule.h),
// no longer the opaque stub.
// BrnEffects::EffectsModule is now the REAL module (the effects spine: the per-car
// ActiveRaceCarData + its four state machines -> HandleWheels -> the particle module's
// TrailSystem). It was an ODR stub here exactly like GameStateModule / DirectorModule /
// GuiModule / RootSoundModule / ReplayModule were, and while it was, mEffectsModule was a
// 1-byte hollow shell: DoUpdate_Effects could not have called Update() on it at all.
#include "GameSource/Effects/EffectsModule.h"
// EffectsIO::InputBuffer / OutputBuffer are POINTER-ONLY here (a parameter and a member
// pointer), so they stay forward-declared. ⛔ Do NOT include the real headers from this
// keystone: BrnEffectsModuleIO_InputBuffer.h -> BrnWorldModuleIO.h ->
// BrnPropEntityModuleIO.h -> BrnGuiEventTypeDefs.h, and the canonical GUI event structs
// there collide (C2011) with ~24 opaque ODR FORKS that GameBridgeNetworkToX.h and
// GameBridgeReplayToX.h still carry. Those forks are a real latent defect -- they are
// invisible only because no TU pulls both in -- but retiring them is the network/replay
// bridges own change, not this one. Measured 2026-09-02.
namespace BrnEffects { namespace EffectsIO { struct InputBuffer; struct DispatchInputBuffer; } }
#include "GameSource/Sound/Module/BrnRootSoundModule.h"   // BrnSound::Module::RootSoundModule (real class)
#include "GameSource/Replays/BrnReplayModule.h"   // BrnReplays::ReplayModule (real class -- was an ODR stub)
#include "GameSource/Replays/Serialisers/BrnReplayDirectorBridgeSerialiser.h"
#include "GameSource/Replays/Serialisers/BrnReplayGameModuleSerialiser.h"
#include "GameSource/Network/BrnNetworkModule.h"

// BrnGameModule.cpp:137 (DecFIGS), byte_82FAE28D (ARTIST). NetworkPreSim publishes
// the online state used by QueryRequestDoStepFrame before it locks the input buffer.
extern bool _gbHACKIsOnline;

namespace BrnGame
{
    // BrnGameModule.cpp:92 (DecFIGS) / byte_82FAE28A (ARTIST). PVSDebugComponent registers
    // this exact game-level flag as "Show stall message"; DoUpdate_GameStatePreWorld reads it
    // while the streaming-pause latch is active.
    extern bool sbShowStreamStallMessage;

    // BrnGame::BrnGameModule - the top-level game module. It owns every engine module and is
    // the engine's IThreadClass implementor (the update/dispatch/resource threads call back
    // into it). Reconstructed from the X360 ARTIST build.
    //
    // The native declaration order is complete here. Main's exact9A1300 size/offset
    // gates remain mandatory; contained module layouts are being recovered separately.
    class BrnGameModule : public CgsModule::ModuleSingleBufferedTemplate<
        BrnGameModuleIO::InputBuffer, BrnGameModuleIO::OutputBuffer>, public CgsSystem::IThreadClass
    {
    public:
        // ARTIST827E5F60/74: first member after the actual640-byte template
        // base and IThreadClass, naturally aligned at648.
        AutoTestManager mAutoTestManager;

        // The game module owns the GameDataModule; the loading flow (case 8) prepares it through here
        // (and via BrnGame::GetMainGameDataModule()) so there's ONE instance, not a parallel copy.
        BrnResource::GameDataModule& GetGameDataModule() { return mGameDataModule; }
        BrnGameMainFlowController::GameMainFlowController& GetMainFlowStateMachine()
        { return mMainFlowStateMachine; }
        bool IsGameOverRequested() const { return mbGameOver; }
        void ClearGameOver() { mbGameOver = false; }
        bool GuiAcceptsControllerInput() const { return mMainFlowStateMachine.IsInGameState(); }
        // The world module (the loading flow's LoadWorldModule drives its Prepare with
        // the update IO stacks -- X360 vtable +68 dispatch @0x823E72F0).
        WorldModule& GetWorldModule() { return mWorldModule; }
        BrnGameState::GameStateModule& GetGameStateModule() { return mGameStateModule; }
        // The dispatch-thread input pair (the flow states post the boot loading-screen
        // command onto its write buffer, as the X360 InitialLoadingScreen::Update does
        // through the module global @0x823EF688).
        BrnGame::DispatchThreadInputBufferManager& GetDispatchThreadInputBufferManager()
        { return mDispatchThreadInputBufferManager; }
        BrnSound::Module::RootSoundModule& GetSoundModule() { return mSoundModule; }
        // The director module (the loading flow's LoadDirectorModule drives its staged Prepare
        // at E_LOADINGSTAGE_DIRECTORMODULE; the per-frame spine then drives its
        // PreSceneQueryUpdate/Update/PostGuiUpdate).
        BrnDirector::DirectorModule& GetDirectorModule() { return mDirectorModule; }
        // The network module (LoadingScriptedState::LoadNetworkModule drives its staged Prepare at
        // E_LOADINGSTAGE_NETWORK; the per-frame spine drives ProcessBeforeSimulation /
        // ProcessAfterSimulation through DoUpdate_NetworkPreSim / DoUpdate_NetworkPostSim).
        BrnNetwork::BrnNetworkModule& GetNetworkModule() { return mNetworkModule; }
        CgsInput::InputModule& GetInputModule() { return mInputModule; }   // LoadingScriptedState::LoadControllerModule

        // Per-frame spines the in-game flow state drives directly (non-virtual; each returns
        // an int status the void flow-state Update/Render slots discard):
        //   DoUpdate   -- the game module's per-frame update spine  (MainGameFlowStateInGame::Update)
        //   DoDispatch -- the game module's per-frame render-dispatch spine (MainGameFlowStateInGame::Render)
        void DoUpdate();
        void DoDispatch();

        void DoPreUpdate_Sound(CgsModule::IOBufferStack* lpUpdateOutputBufferStack,
                               BrnSound::Module::Io::RootPreUpdateOutputBuffer* lpSoundPreUpdateOutputBuffer,
                               CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInputBuffer);

        // @ 0x823DCEC0 -- the SOUND leg of the full per-frame cascade (DoUpdate's post-world
        // position: after DoUpdate_Effects, before DoUpdate_ReplaysPostSim). The console body
        // (11 params + this; full decode progress/scratch_dossiers/doupdate_sound_0x823DCEC0.md):
        // perfmon pair; self-carve a RootInputBuffer "Sound" off the INPUT stack; under the
        // console's six-buffer lock helper (W rootIn + R director/world/gamestate/gui/replays)
        // plus the interleaved effects read lock (a lock-only participant nothing reads):
        // BridgeWorldToSound -> SetCameraInput(director camera output) -> BridgeGameStateToSound
        // -> BridgeGuiToSound -> SetReplayStatusInterface; all locks released, then
        // RootSoundModule::Update (the same timer products as the loading spine); stops;
        // destroy the self-carved buffer. NO resource forward here -- the caller drains the
        // root output in its own tail.
        void DoUpdate_Sound(CgsModule::IOBufferStack* lpInputBufferStack,
                            CgsModule::IOBufferStack* lpOutputBufferStack,
                            BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutputBuffer,
                            BrnWorldIO::UpdateOutputBuffer* lpWorldOutputBuffer,
                            BrnDirector::DirectorIO::OutputBuffer* lpDirectorOutputBuffer,
                            BrnReplays::ReplayIO::OutputBuffer_PreSim* lpReplaysPreSimOutputBuffer,
                            BrnSound::Module::Io::RootOutputBuffer* lpSoundOutputBuffer,
                            CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer,
                            BrnEffects::EffectsIO::OutputBuffer* lpEffectsOutputBuffer,
                            BrnUpdateSet leUpdateSet);

        // ARTIST 0x823DD2A0: PreSim bridge. The X360 call/callee use five explicit
        // arguments; the older DecFIGS declaration carries one extra output-pointer parameter.
        void DoUpdate_ReplaysPreSim(CgsModule::IOBufferStack* lpUpdateInputBufferStack,
                                    CgsModule::IOBufferStack* /*lpUpdateOutputBufferStack*/,
                                    const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutputBuffer,
                                    BrnReplays::ReplayIO::OutputBuffer_PreSim* lpReplaysPreSimOutputBuffer,
                                    BrnUpdateSet lUpdateSet);

        // ARTIST 0x823DD408: PostSim bridge; assembles the five producer request interfaces,
        // appends GUI events, then dispatches the typed replay PostSim virtual.
        void DoUpdate_ReplaysPostSim(CgsModule::IOBufferStack* lpUpdateInputBufferStack,
                                     CgsModule::IOBufferStack* /*lpUpdateOutputBufferStack*/,
                                     const BrnWorldIO::UpdateOutputBuffer* lpWorldOutputBuffer,
                                     const BrnEffects::EffectsIO::OutputBuffer* lpEffectsOutputBuffer,
                                     const BrnSound::Module::Io::RootOutputBuffer* lpSoundOutputBuffer,
                                     BrnDirector::DirectorIO::OutputBuffer* lpDirectorOutputBuffer,
                                     const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer,
                                     BrnReplays::ReplayIO::OutputBuffer_PostSim* lpReplaysPostSimOutputBuffer,
                                     BrnUpdateSet lUpdateSet);

        // @ 0x823E8BD0 -- the WORLD leg of the per-frame update cascade: create this
        // frame's BrnWorldIO::UpdateInputBuffer on the update input stack, stage the
        // controller / network / game-state / GUI / replay / sound state into it, then
        // dispatch the world module (UpdateForBootUpVideo @0x827CFDE0 when the update set
        // carries 0x20, otherwise Update @0x827D63E8 = vtable +76 with the world frame
        // allocator), read the world-entity status back out of the output buffer and
        // destroy the input buffer. The world UPDATE OUTPUT buffer is owned by the caller
        // (DoUpdate), as is the update set.
        void DoUpdate_World(CgsModule::IOBufferStack* lpUpdateInputBufferStack,
                            CgsModule::IOBufferStack* lpUpdateOutputBufferStack,
                            const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer,
                            const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutputBuffer,
                            const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutputBuffer,
                            const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer,
                            const BrnReplays::ReplayIO::OutputBuffer_PreSim* lpReplayOutputBuffer,
                            BrnSound::Module::Io::RootPreUpdateOutputBuffer* lpSoundPreUpdateOutputBuffer,
                            BrnWorldIO::UpdateOutputBuffer* lpWorldUpdateOutputBuffer,
                            BrnUpdateSet lUpdateSet);

        // The game-state post-world leg of the update step. Stages a
        // GameStateModuleIO::PostWorldInputBuffer "GameStatePostWorld" on the update INPUT stack,
        // and under LockBuffersForIO (the buffer write-locked; the input, world, GUI, replay
        // post-sim and director outputs read-locked) fills it: BridgeGuiToGameState, then
        // BridgeWorldToGameState unless update-set bit 0x20 is set, the director's pause request
        // as game event 43, the replay post-sim game events, the pause-button latch as game event
        // 10, and the picture-paradise camera flag. With the locks released,
        // GameStateModule::PostWorldUpdate runs unless bit 0x20 is set or the disk has failed;
        // then the buffer is destroyed.
        void DoUpdate_GameStatePostWorld(CgsModule::IOBufferStack* lpUpdateInputBufferStack,
                                         CgsModule::IOBufferStack* lpUpdateOutputBufferStack,
                                         const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer,
                                         const BrnWorldIO::UpdateOutputBuffer* lpWorldUpdateOutputBuffer,
                                         const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer,
                                         const BrnReplays::ReplayIO::OutputBuffer_PostSim* lpReplaysPostSimOutputBuffer,
                                         const BrnDirector::DirectorIO::OutputBuffer* lpDirectorOutputBuffer,
                                         BrnUpdateSet lUpdateSet);

        // ARTIST 823E8DE0 / 823DCE38, with caller-owned IO lifetimes.
        void DoUpdate_Director(CgsModule::IOBufferStack* lpInputStack,
            CgsModule::IOBufferStack* lpOutputStack,
            BrnDirector::DirectorIO::InputBuffer* lpDirectorInput,
            const CgsInput::InputIO::OutputBuffer* lpInputOutput,
            const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput,
            const BrnWorldIO::UpdateOutputBuffer* lpWorldOutput,
            const BrnReplays::ReplayIO::OutputBuffer_PreSim* lpReplayOutput,
            BrnDirector::DirectorIO::OutputBuffer* lpDirectorOutput,
            BrnUpdateSet lUpdateSet);
        void DoUpdate_DirectorPostGUI(CgsModule::IOBufferStack* lpInputStack,
            CgsModule::IOBufferStack* lpOutputStack,
            BrnDirector::DirectorIO::InputBuffer* lpDirectorInput,
            const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput,
            BrnDirector::DirectorIO::OutputBuffer* lpDirectorOutput);

        void DoUpdate_GUI(CgsModule::IOBufferStack* lpInputStack,
            CgsModule::IOBufferStack* lpOutputStack,
            CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
            CgsGui::ViewIO::InputBuffer* lpViewInput,
            const CgsInput::InputIO::OutputBuffer* lpInputOutput,
            const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutput,
            const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput,
            const BrnWorldIO::UpdateOutputBuffer* lpWorldOutput,
            const BrnReplays::ReplayIO::OutputBuffer_PreSim* lpReplayOutput,
            CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput,
            CgsGui::ModelIO::OutputBuffer* lpModelOutput,
            BrnDirector::DirectorIO::OutputBuffer* lpDirectorOutput,
            BrnSound::Module::Io::RootOutputBuffer* lpSoundOutput,
            BrnResource::GameDataIO::InputBuffer* lpResourceInput,
            const BrnResource::GameDataIO::OutputBuffer* lpResourceOutput,
            BrnUpdateSet lUpdateSet, u32 luStartPressedPort);
        void UpdateFrameRateType(const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput);

        // @0x823DD0A8 -- the EFFECTS leg of the per-frame cascade (DoUpdate's post-world
        // position: after DoUpdate_DirectorPostGUI, before DoUpdate_Sound). Register decode
        // of the console prologue, NOT the Hex-Rays parameter list (which invents 34):
        //   r4  inStack   r5  outStack   r6  input(pad) out   r7  gameState out
        //   r8  world out r9  director out  r10 replays pre-sim out
        //   [sp+0x54] sound pre-update out   [sp+0x5C] effects out
        //   [sp+0x67] the suspend bool       [sp+0x6E] the update set (u16)
        // Body: perfmon; self-carve an EffectsIO::InputBuffer "Effects" off the INPUT stack;
        // LockBuffersForIO (sub_823B78A0: W effectsIn + R director/input/world/gameState/
        // replays/soundPreUpdate); SetTimerStatusInterface(gm+0x9A0B0C); SetCameraInput(the
        // director's camera output); pad 0 button bit 1 -> EffectsModule::RestartEffects;
        // BridgeEntityToEffects; append the game-state action queue; SetReplayStatusInterface;
        // SetAudioEffectsMessageQueue(soundPreUpdate + 0x2A0); store the suspend bool at
        // +0xE494; UnlockBuffersForIO (sub_823B7A10); then the module's vtable +0x44 Update.
        void DoUpdate_Effects(CgsModule::IOBufferStack* lpInputBufferStack,
                              CgsModule::IOBufferStack* lpOutputBufferStack,
                              const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer,
                              BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutputBuffer,
                              BrnWorldIO::UpdateOutputBuffer* lpWorldOutputBuffer,
                              BrnDirector::DirectorIO::OutputBuffer* lpDirectorOutputBuffer,
                              BrnReplays::ReplayIO::OutputBuffer_PreSim* lpReplaysPreSimOutputBuffer,
                              BrnSound::Module::Io::RootPreUpdateOutputBuffer* lpSoundPreUpdateOutputBuffer,
                              BrnEffects::EffectsIO::OutputBuffer* lpEffectsOutputBuffer,
                              bool lbSuspendEffects,
                              BrnUpdateSet leUpdateSet);

        // @0x823CDF00 -- the WORLD -> EFFECTS bridge, called by DoUpdate_Effects inside the
        // effects input buffer's write lock. Six installs plus the eight-slot boost copy;
        // the update set's bit 0x100 selects the REPLAY active-race-car interface.
        void BridgeEntityToEffects(const BrnWorldIO::UpdateOutputBuffer* lpWorldOutputBuffer,
                                   BrnEffects::EffectsIO::InputBuffer* lpEffectsInputBuffer,
                                   BrnUpdateSet leUpdateSet);

        // @0x823BCFD0 -- tick the game and sim timers once per sim sub-step. The console
        // first drains the game-state and director TimerRequests queues through
        // TimerRequestInterface::ApplyToTimers; both producers are un-staged on this build,
        // so only the two Timer::Update calls (the tail of the console body) are live here.
        void UpdateTimers(const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput,
            const BrnDirector::DirectorIO::OutputBuffer* lpDirectorOutput);

        // @0x823BD150 -- snapshot the two timers into mTimerStatusInterface and copy the
        // whole 48-byte interface into the director's input buffer under its write lock.
        // This is what gives the camera behaviours a NON-ZERO frame timestep.
        void BridgeTimers(BrnDirector::DirectorIO::InputBuffer* lpDirectorInput);

        // The per-frame update IO stacks. The scripted module loads (LoadingScriptedState::
        // LoadXxxModule) create their per-frame module IO buffers on these, exactly as the
        // X360 reads them off the game-module global (e.g. LoadSoundModule 0x823E75A8).
        // The scripted load's stage 8 prepares this module (ReplayModule::Prepare @0x82652768):
        // it is what acquires the linear region every BaseSerialiser's buffers are carved from.
        BrnReplays::ReplayModule& GetReplayModule() { return mReplayModule; }
        CgsModule::IOBufferStack* GetUpdateInputBufferStack()  { return mpUpdateInputBufferStack; }
        CgsModule::IOBufferStack* GetUpdateOutputBufferStack() { return mpUpdateOutputBufferStack; }
        BrnResource::GameDataIO::InputBuffer* GetGameDataModuleInputBuffer() { return mpGameDataModuleInputBuffer; }
        BrnResource::GameDataIO::OutputBuffer* GetGameDataModuleOutputBuffer() { return mpGameDataModuleOutputBuffer; }

        // ARTIST823DCD78. Dormant until the complete native GUI PreWorld
        // override and its validators/Profile/movie dependencies are composed.
        void DoUpdate_GuiPreWorld(CgsModule::IOBufferStack* lpInputBufferStack,
            CgsModule::IOBufferStack* lpOutputBufferStack,
            CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput,
            CgsGui::ModelIO::OutputBuffer* lpModelOutput,
            BrnResource::GameDataIO::InputBuffer* lpGameDataInput);

        BrnEffects::EffectsModule& GetEffectsModule() { return mEffectsModule; }

        enum EGameUpdateStage   // h:248
        {
            E_GAMEUPDATESTAGE_PREPARE = 0,
            E_GAMEUPDATESTAGE_MAIN = 1,
            E_GAMEUPDATESTAGE_RELEASE = 2,
        };

        // h:456 - the resumable GamePrepare() stage machine (X360 gm+10094164). The
        // stage VALUES are the X360 switch labels of 0x823EFBD0: 0/1 queue the three
        // one-time LoadBundle requests, 2 waits for all three completions and posts the
        // six global-texture acquires, 3 waits for those and runs
        // BrnRendererModule::PrepareAgain, 4 runs the GameState module's game-prepare.
        enum EGamePrepareStage
        {
            E_GAMEPREPARESTAGE_START        = 0,
            E_GAMEPREPARESTAGE_LOADBUNDLES  = 1,
            E_GAMEPREPARESTAGE_WAITBUNDLES  = 2,
            E_GAMEPREPARESTAGE_WAITACQUIRES = 3,
            E_GAMEPREPARESTAGE_GAMESTATE    = 4,
        };

        enum EInputBindState
        {
            E_INPUTBINDSTATE_UNBOUND = 0,
            E_INPUTBINDSTATE_PRESS_START = 1,
            E_INPUTBINDSTATE_START_PRESSED = 2,
            E_INPUTBINDSTATE_BIND_REQUESTED = 3,
            E_INPUTBINDSTATE_BOUND = 4,
            E_INPUTBINDSTATE_GAME_OVER = 5,
            E_INPUTBINDSTATE_UNBIND_REQUESTED = 6
        };
        enum EGuiFlowStart
        {
            E_GUIFLOW_INITIALLOAD = 0, E_GUIFLOW_MARKETING = 1,
            E_GUIFLOW_START = 2, E_GUIFLOW_COMPLETELOADING = 3,
            E_GUIFLOW_MEMCARD = 4, E_GUIFLOW_INGAME = 5, E_GUIFLOW_MAX = 6
        };
        enum EGameReleaseStage
        {
            E_GAMERELEASESTAGE_START = 0,
            E_GAMERELEASESTAGE_BURNOUTRESOURCEMODULE = 1,
            E_GAMERELEASESTAGE_NETWORK = 2,
            E_GAMERELEASESTAGE_SOUND_MODULE = 3,
            E_GAMERELEASESTAGE_DIRECTORMODULE = 4,
            E_GAMERELEASESTAGE_GUIMODULE = 5,
            E_GAMERELEASESTAGE_CONTROLLERMODULE = 6,
            E_GAMERELEASESTAGE_WORLDMODULE = 7,
            E_GAMERELEASESTAGE_RENDERMODULE = 8,
            E_GAMERELEASESTAGE_GAMESTATEMODULE = 9,
            E_GAMERELEASESTAGE_REPLAYMODULE = 10,
            E_GAMERELEASESTAGE_DONE = 11
        };

        enum EPrepareStage
        {
            E_PREPARESTAGE_START = 0,
            E_PREPARESTAGE_HARDWARE = 1,
            E_PREPARESTAGE_MANAGER = 2,
            E_PREPARESTAGE_GAMEDATAMODULE = 3,
            E_PREPARESTAGE_RENDERMODULE = 4,
            E_PREPARESTAGE_HARDWAREMEMORY = 5,
            E_PREPARESTAGE_THREADS = 6,
            E_PREPARESTAGE_DONE = 7
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

        void CheckClassSizes(); // ARTIST823BD4E0: native footprint diagnostic.
        void Construct() ;   // @ BrnGameModule.cpp:155 - construct the owned modules

        // ARTIST 823DB848 / vtable +0x40. This declaration deliberately requires
        // the genuine five-stack preparation body; Prepare() on the base is not
        // a replacement for the game module's staged native preparation.
        virtual bool Prepare(CgsModule::IOBufferStack* lpUpdateInput,
                             CgsModule::IOBufferStack* lpUpdateOutput,
                             CgsModule::IOBufferStack* lpResourceInput,
                             CgsModule::IOBufferStack* lpResourceOutput,
                             CgsModule::IOBufferStack* lpDispatch);

        // DecFIGS BrnGameModule.h:1464; ARTIST main reads mbHasGameTerminated
        // at +0x9A0620. The entry/layout gates still require the complete native object.
        bool HasGameTerminated();
        void CheckDiskError(const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput);

        // @ BrnGameModule.cpp:1047 - tear down the owned modules + the module base. Overrides
        // CgsModule::Module::Destruct. Destructs all 11 engine modules (in the X360's destruct
        // order) then the module base. Not on the boot/loading path (the game doesn't shut down
        // during the loading screen).
        void Destruct() ;

        // @ BrnGameModule.cpp:925 - resumable staged release (counterpart of Prepare). Each call
        // advances meReleaseStage, releasing GUI -> sound -> hardware(+input) -> game-data ->
        // module base -> network; returns false to be re-driven, true at DONE. Not on the
        // boot/loading path.
        bool Release() ;

        // Debug helper: fill liNumBytes at lpDest with the repeating 32-bit pattern luValue.
        static void DebugSetMemoryToInt(void* lpDest, s32 liNumBytes, u32 luValue);  // @ BrnGameModule.cpp:3872
        // Debug: clear the whole game-module object then stamp the per-module memory regions
        // with a marker so unwritten reads are caught.
        static void DebugMemoryInit(BrnGameModule* lpData);  // @ BrnGameModule.cpp:3916; ARTIST r3 is lpData, no this

        // The per-frame debug overlay drive (X360 @0x823BCB88, called from DoDispatch @0x823DC458):
        // gate on the debug font + the published debug render buffers, keep the fps ring/average,
        // queue the build-info/fps/memory readouts, then record through the
        // fresh RendererIO output while the original caller holds its read lock.
        void DebugManagerRender(RendererIO::OutputBuffer* lpRendererOutput);

        // IThreadClass implementation (the engine drives these on their threads).
        virtual void Update();                                      // ARTIST 0x823C5480
        bool UpdateThread() ;
        void DispatchThread() ;                               // @ BrnGameModule.cpp:1221
        void ResourceUpdateThread(Mutex* lpMutex) ;
        void OnStartOfUpdateFrame() ;
        void OnEndOfUpdateFrame() ;
        void OnCompletionOfVsyncWait() ;
        void RenderAssert(const AssertData* lpAssertData) ;

        // ⭐ ADDITIVE 2026-08-09 (feed wave; header-only inline, no out-of-line symbol).
        // The console reads this member as the raw `gm+10095372` when it stages the world
        // update input buffer's timer status inside DoUpdate_World @0x823E8BD0
        // (`UpdateInputBuffer::SetTimerStatusInterface(worldIn, gm+10095372)`). Exposed BY NAME
        // so the live PC world drive can make the same call without a console byte offset --
        // the x64 layout does not put this member at 10095372.
        const CgsSystem::TimerStatusInterface* GetTimerStatusInterface() const
        {
            return &mTimerStatusInterface;
        }

        // @0x823BD420. Public because the console calls it from FOUR sites, and one of them
        // is outside this class: LoadingScriptedState::Update's spine (@0x823F26D8) derives
        // the frame's update set from the flow state before deciding whether to drive the
        // world at all (the others are DoDispatch, ConstructUpdateSet and DoUpdate).
        BrnUpdateSet ConstructUpdateSetFromFsm();
        // @0x823DCB40 -- combine the FSM set with this frame's replay, input,
        // network, GUI-model, disk-error, and world-streaming state.
        BrnUpdateSet ConstructUpdateSet(
            const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateModuleOutputBuffer,
            const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkModuleOutputBuffer,
            const CgsInput::InputIO::OutputBuffer* lpInputModuleOutputBuffer,
            const CgsGui::ModelIO::OutputBuffer* lpGuiModuleModelOutputBuffer,
            const BrnReplays::ReplayIO::OutputBuffer_PreSim* lpReplayOutputBuffer);

        const CgsSystem::Timer& GetGameTimer() const { return mGameTimer; }
        const CgsSystem::Timer& GetSimTimer() const  { return mSimTimer; }

    private:
        bool IsEventInGuiQueue(s32 liEventTypeToFind,
                               const CgsGui::ModelIO::OutputBuffer::GuiEventQueue* lpEventQueue);
        bool IsEventInGuiQueue(s32 liEventTypeToFind,
                               const CgsGui::GuiEventQueueSmall* lpEventQueue);

        // @ BrnGameModule.cpp:1845 - the per-frame update spine: latch frame-rate stepping,
        // begin the frame-rate frame, tick the debug manager, then run the active flow
        // state's Update() once per required simulation sub-step (recreating the static IO
        // buffers between steps), and finally its Render(); tear down the per-frame IO
        // buffers. Returns false (the X360 returns 0).
        bool GameMain();

        // GameMain callees + sim-step plumbing (reconstructed).
        void         QueryRequestDoStepFrame(const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer);
        void         UpdateRequestDoStepFrame();   // @ BrnGameModule.cpp:4048
        void         CreateStaticIOBuffers();      // @ BrnGameModule.cpp:2497
        void         DestroyStaticIOBuffers();     // @ BrnGameModule.cpp:2515

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

        // X360 0x823CDD20 -- copy this frame's renderer-produced handles (the GDL
        // DispatchFrame, the shader-constants frame, the four world effects frames, the
        // blobby-shadow buffer, the corona submission interface, the frame camera and the
        // render switches) out of the renderer OUTPUT buffer and into the world's dispatch
        // INPUT buffer. Run by DoDispatch @0x823DC458 right after BrnRendererModule::Update;
        // it is what gives WorldModule::GenerateDispatchLists a frame to stamp
        // DRAWRENDERABLE commands into. Home TU: GameBridgeRendererToX.cpp.
        void BridgeRendererToWorld(BrnWorldIO::DispatchInputBuffer* lpWorldDispatchInput,
                                   RendererIO::OutputBuffer* lpRendererOutput);
        // ARTIST823C1168; caller holds output read/input write locks.
        void BridgeRendererToEffects(BrnEffects::EffectsIO::DispatchInputBuffer* lpEffectsInput,
                                     RendererIO::OutputBuffer* lpRendererOutput);
        void BridgeRendererToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
                                 RendererIO::OutputBuffer* lpRendererOutput);

        // ARTIST 823CD738: the caller owns and write-locks the staged buffer.
        void BridgeControllerToGameState(BrnGameState::GameStateModuleIO::PreWorldInputBuffer* lpGameStateInput,
                                         const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer,
                                         bool lbIsPlaying);

        // X360 0x823E6B18 -- synthesise GUI events from player-0 (and, when no player-0 controller is
        // assigned, the cross-pad menu-accept scan) and push them through the GUI module's
        // AddGuiEvent<T> into the GUI module INPUT buffer (the mangled name pins the third
        // AddGuiEvent arg as CgsGuiModuleIO::InputBuffer*).
        void BridgeControllerToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInputBuffer,
                                   const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer);

        // X360 0x823C0AE8 -- caller holds PostWorldInput for write and GameState output for read.
        // Copies the typed GameStateToController bind/unbind requests into PostWorldInput and
        // stamps miInputModuleState with the bind (3) / unbind (6) sentinel.
        void BridgeGameStateToController(CgsInput::InputIO::PostWorldInputBuffer* lpPostWorldInput,
            const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput);
        // X360 0x823BD248 -- caller holds this input-module output buffer for read.
        void BridgeInputToGame(const CgsInput::InputIO::OutputBuffer* lpInputModuleOutput);
        void HandleInputBindResult(const CgsInput::InputIO::BindResult& lrResult);
        void HandleInputUnbindResult(const CgsInput::InputIO::UnBindResult& lrResult);

        // X360 0x823E1C38 -- walk the game-state output's TakedownEvent output queue and push one
        // GUI takedown event per record through the CgsGui GUI module (this + 7252512). A record
        // becomes a BrnGui::GuiTakedownEvent when its VICTIM index equals the player's active
        // race-car index, or when the "soft takedown display" flag bit is clear; otherwise a
        // BrnGui::GuiSoftTakedownEvent. (The body compares the record's +0x04 field, meVictimIndex,
        // against the third argument -- which the caller loads from the scoring output interface's
        // mePlayerRaceCarIndex, so it is the PLAYER index, not a runner index.)
        // ARTIST 0x823EE880; original wrapper shared by normal GUI and scripted loading.
        void BridgeGameStateToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
                                  const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput);

        // ARTIST 0x823E1D90, promoted from the file-local bring-up seat.
        void TranslateGuiInterfaceToGuiEvents(
            CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
            const BrnGameState::GameStateModuleIO::GameStateToGuiInterface* lpGameToGuiInterface);

        // Called by BridgeGameStateToGui. Home GameSource/Game/GameBridgeGameStateToX.cpp.
        void TranslateTakedownsToGuiEvents(
            CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
            const CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>* lpTakedownQueue,
            s32 liPlayerActiveRaceCarIndex);

        // ⭐⭐ [gateui] X360 0x823E9CE0 -- the GAME-ACTION -> GUI-EVENT translator: drain the
        // game-state output buffer's GameActionQueue (VariableEventQueue<13312,16>) and post
        // the matching BrnGui event for each recognised action. Signature from the asm
        // prologue (r3 = this, r4 = lpGuiInput, r5 = lpGameStateOutput); the two null asserts
        // are GameBridgeGameStateToX.cpp:754/755. Called by BridgeGameStateToGui @0x823EE880
        // (call site @0x823EF22C).
        // PARTIAL RECONSTRUCTION -- ARTIST has 188 unique explicit action IDs.
        // This source checkpoint represents 142; omitted IDs remain unresolved.
        // Keep the per-arm inventory with GameBridgeGameStateToX_StuntGuiEvents.cpp.
        // Home GameSource/Game/GameBridgeGameStateToX.cpp.
        void TranslateGameActionsToGuiEvents(
            CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
            const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput);

        // ⭐⭐⭐ [showtime-score wave 2026-08-29] X360 0x823E1988 -- the SHOWTIME slice of the
        // game-action -> GUI-event translation, and the ONLY producer in the image of every
        // score-bearing showtime GUI event (396 / 394 / 399 / 401 / 400 / 393 / 397 / 402).
        // Signature from the asm prologue: r3 = this, r4 = the action type, r5 = the action
        // record, r6 = the GUI input buffer. Called from TranslateGameActionsToGuiEvents'
        // jump-table arm for "cases 127,128,139-144" (@0x823ED714), which mode-gates the whole
        // call on the SCORING output interface's meGameModeType being 2 or 16.
        // Home GameSource/Game/GameBridgeGameStateToX_ShowtimeGuiEvents.cpp.
        void TranslateShowtimeActionToGuiEvent(
            s32 liActionType,
            const CgsModule::Event* lpAction,
            CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput);

        // ⭐ [gateui] X360 0x823AA4A8 -- map a gameplay-side StuntElementType onto the GUI's
        // BrnGui::StuntType. IDENTITY for 0/1/2; anything >= 3 fires the streamed assert
        // "Invalid Stunt Enum : <v>" (GameBridgeGameStateToX.cpp:404) and returns 3
        // (E_STUNTTYPE_COUNT, which the analyzer's own bound assert then catches). A real X360
        // symbol and a non-static member (r3 = this, never read). Home
        // GameSource/Game/GameBridgeGameStateToX.cpp.
        BrnGui::StuntType MapStuntEnumsFromGameplayToGui(u32 luGameplayStuntType) const;

        // X360 0x823DCA10 -- the game->GUI flow-FSM bridge: when the main flow requested a
        // GUI FSM stage (miGuiFsmStage 1..5), post the matching GuiEventRunFsm record(s)
        // (event 144, 24 bytes) into the GUI module INPUT buffer's inbound queue, then park
        // the stage at 6 (idle) and clear the phase-complete flag:
        //   1 -> BrnVideoFsm (HUD)      2 -> BrnLegalFsm (HUD)     3 -> BrnCmpLdFsm (HUD)
        //   4 -> BrnBFProFsm (HUD)      5 -> BrnScreenFsm@LOADING (SCREEN) + BrnFBFsm (HUD)
        void BridgeGameToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInputBuffer);

        // X360 BridgeDirectorToGui @0x823DD5C0 (home GameBridgeDirectorToX.cpp): the director
        // output's camera post-FX requests -> GUI events 495/496/498/499, the director's
        // enumeration request byte -> 500. Called under the GUI-input write lock with the
        // director output read-locked.
        void BridgeDirectorToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInputBuffer,
                                 const BrnDirector::DirectorIO::OutputBuffer* lpDirectorOutputBuffer);

        // ---- the world -> GUI vehicle-data bridge family (home GameBridgeWorldToGui.cpp) --
        // X360 BridgeWorldToGui @0x823EDD50 (called from DoUpdate_GUI @0x823F0758): the four
        // sub-bridges (VehicleData @0x823E5768, RouteInformation, TrafficAndPropData,
        // ImpactInformation) + the collision-world request event. PARTIAL SLICE on this
        // build: the VEHICLE-DATA leg's boost-info post (GUI event 206 -> the HUD boost bar)
        // is live; the rest is FLAG-deferred in the TU.
        void BridgeWorldToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInputBuffer,
                              const BrnWorldIO::UpdateOutputBuffer* lpWorldOutputBuffer);
        void BridgeWorldVehicleDataToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInputBuffer,
                                         const BrnWorldIO::UpdateOutputBuffer* lpWorldOutputBuffer);
        // The sat-nav route reply: every E_OWNER_GUI route response in the world output becomes a
        // route record handed to the GUI module's tracker (GuiTracker::RecEvent, 211). Body in
        // GameBridgeWorldToGui.cpp.
        void BridgeWorldRouteInformationToGui(const BrnWorldIO::UpdateOutputBuffer* lpWorldOutput);
        // X360 @0x823E5560 (DWARF BrnGameModule.h:820): the world output's GUI queue -> the GUI
        // input -- 512 traffic pool emptied, 208 score targets / 210 overhead signs (an empty record
        // when the mode is not in progress), 209 removed traffic (also handed to
        // CrashModeScoring::DealWithRemovedTraffic), 592.
        void BridgeWorldTrafficAndPropDataToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
                                                const BrnWorldIO::UpdateOutputBuffer* lpWorldOutput);
        // X360 @0x823E6A80: every world game event of id 31 (VehicleImpactEvent -- trading
        // paint / nudge / slam / shunt between race cars) is forwarded to the GUI as
        // GuiImpactEvent (365, 12 bytes) -- the only producer of the HUD's impact hint.
        void BridgeWorldImpactInformationToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInputBuffer,
                                               const BrnWorldIO::UpdateOutputBuffer* lpWorldOutputBuffer);

        // GUI/world -> GameData and GUI -> world staging bridges (source homes
        // GameBridgeGUIToX.cpp and GameBridgeWorldToX.cpp); bodied in the
        // _wBT_01 partfiles beside those homes.
        void BridgeGuiToResource(BrnResource::GameDataIO::InputBuffer* lpGDMInput,
                                 const CgsGui::ModelIO::OutputBuffer* lpModelOutput,
                                 const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput);
        void BridgeGuiToWorld(BrnWorldIO::UpdateInputBuffer* lpWorldInput,
                              const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer);
        void BridgeWorldToResource(BrnResource::GameDataIO::InputBuffer* lpGDMInput,
                                   const BrnWorldIO::UpdateOutputBuffer* lpWorldOutput);

        // ⭐ X360 0x823E3AB0 -- the WORLD->DIRECTOR seam, the ONLY caller of
        // DirectorIO::InputBuffer::SetRaceCarInfo in the whole image and therefore the only
        // way a race car's pose reaches the director's cameras. Signature recovered from the
        // ASM (r3/r4/r5 only -- IDA's 16-parameter prototype is stack-slot noise); the DWARF
        // home is GameSource/Game/GameBridgeWorldToX.cpp, where the body lives.
        // Called by DoUpdate_Director @0x823E8DE0 with this sub-step's director INPUT buffer
        // and the world UPDATE OUTPUT buffer.
        void BridgeWorldToDirector(BrnDirector::DirectorIO::InputBuffer* lpDirectorInput,
                                   const BrnWorldIO::UpdateOutputBuffer* lpWorldOutput);

        // The WORLD->GAME-STATE seam: copies every per-frame world product the post-world half
        // of the game state reads (crash events, game events, traffic responses, contact spy,
        // race-car / trigger / AI / vehicle interfaces, the mode manager's route answers) into
        // the post-world input buffer (write-locked) from the world update output (read-locked).
        // `this` is unused. Called by DoUpdate_GameStatePostWorld inside its LockBuffersForIO
        // bracket. Body and leg list: GameSource/Game/GameBridgeWorldToX.cpp.
        void BridgeWorldToGameState(
            BrnGameState::GameStateModuleIO::PostWorldInputBuffer* lpGameStateInput,
            const BrnWorldIO::UpdateOutputBuffer* lpWorldOutput);

        // ⭐⭐ X360 0x823CD170 -- the GAME-STATE->DIRECTOR seam. Its Append of the game-state
        // output buffer's game-action queue into the director input buffer's own queue is the
        // ONLY producer of the actions MainDirector::ProcessInputQueue drains, i.e. the only
        // route by which the junkyard / car-select ladder can ever start. Console home
        // GameSource/Game/GameBridgeGameStateToX.cpp; the body sits in BrnGameModule.cpp with
        // the other DoUpdate_Director bridges (the BridgeGuiToDirector precedent).
        // Called by DoUpdate_Director @0x823E8DE0 on the PRE-GUI pass with this sub-step's
        // director INPUT buffer and the game-state module's OUTPUT buffer.
        void BridgeGameStateToDirector(
            BrnDirector::DirectorIO::InputBuffer* lpDirectorInput,
            const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput);

        // ⭐⭐ X360 0x823E1890 -- the GAME-STATE->WORLD seam. Twelve transfers, no branches: it
        // Appends the game-state output's GAME-ACTION queue into the world update input buffer's
        // own queue (plus the takedown queue, both trigger interfaces, the race-distance and the
        // two scoring snapshots, the payback pair and the controller-active flag).
        //
        // The game-action Append is the ONLY route by which a game action reaches the world at
        // all -- WorldModule::HandleGameActions and WorldModule::BridgeInputToEntityModules both
        // drain UpdateInputBuffer::GetGameActionQueue(), and nothing else ever fills it. That is
        // what makes it the consumer side of CarSelectManager's ResetPlayerCarAction (type 0):
        // the record that PLACES THE PLAYER'S CAR.
        // Console home GameSource/Game/GameBridgeGameStateToX.cpp (body there, with its siblings);
        // called by DoUpdate_World @0x823E8BD0 inside the world input buffer's write lock.
        void BridgeGameStateToWorld(
            BrnWorldIO::UpdateInputBuffer* lpWorldInput,
            const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput);

        // The main-flow states' pending GUI FSM stage request (the X360 +2523537 byte the
        // MainGameFlowState OnEnters write; BridgeGameToGui consumes it).
        void RequestGuiFsmStage(s32 liStage) { miGuiFsmStage = static_cast<EGuiFlowStart>(liStage); }

        // The GUI phase-complete flag (X360 +10094152; command 70). Stays latched until
        // the next BridgeGameToGui stage post clears it -- the X360 lifecycle.
        bool IsGuiPhaseComplete() const { return mbGuiPhaseComplete; }
        // X360 gm+10094117 (0x9A0625): MainGameFlowStateMemoryCard::Update @0x823F2F98 sets it on
        // the flow's GUI-phase-complete exit; DoUpdate_GameStatePreWorld @0x823EE0E8 consumes it
        // (clear + game event 8 -> GameStateModule::WaitForStreaming).
        void RequestStreamingWait() { mbRequestStreamingWait = true; }

        // The pre-accept flag (X360 +10094153; command 71 -- "resume the world load while
        // the accept dwell plays"). Read-and-clear, as MainGameFlowStateStartScreen does.
        bool IsGuiPreAcceptRequested() const { return mbGuiPreAccept; }
        void ClearGuiPreAccept() { mbGuiPreAccept = false; }

        // This sub-step's GUI module INPUT buffer (live between CreateStaticIOBuffers /
        // DestroyStaticIOBuffers; the flow states' initial-FSM post reaches it here).
        CgsGui::CgsGuiModuleIO::InputBuffer* GetGuiInputBuffer() { return mpGuiInputBuffer; }
        // This sub-step's GUI module OUTPUT buffer (same lifetime; the sound spine's
        // BridgeGuiToSound source -- faithful-audio-engine phase C4).
        CgsGui::CgsGuiModuleIO::OutputBuffer* GetGuiOutputBuffer() { return mpGuiOutputBuffer; }
        CgsGui::ModelIO::OutputBuffer* GetGuiModelOutputBuffer() { return mpGuiModelOutputBuffer; }
        CgsGui::ViewIO::InputBuffer* GetGuiViewInputBuffer() { return mpGuiViewInputBuffer; }
        CgsSystem::Timer& GetGameTimer() { return mGameTimer; }
        // This sub-step's DIRECTOR OUTPUT buffer (CreateStaticIOBuffers lifetime; the console
        // holds it as the persistent member gm+0x9A0BDC and DoUpdate passes it into
        // DoUpdate_Sound's SetCameraInput leg -- phase C4b).
        BrnDirector::DirectorIO::OutputBuffer* GetDirectorOutputBuffer() { return mpDirectorOutputBuffer; }
        // The console reaches the GUI module through the module scheduler (`*off_830102D0 +
        // 0x6EAA20`, the raw member address, in LoadGUIModule @0x823EF310 and everywhere else
        // on the loading path). The PC has no scheduler, so the loading-screen state reaches
        // it by name -- it is the same object at the same place in the module list.
        BrnGui::GuiModule& GetGuiModule() { return mGuiModule; }
        BrnRendererModule& GetRenderModule() { return mRenderModule; }
        CgsDev::DebugManager& GetDebugManager() { return mDebugManager; }
        CgsSystem::ThreadLayout& GetThreadLayout() { return mThreadLayout; }
        bool GetIsDiskError() const { return mbDiskError; }
        // gm+0x9A0630 -- the allocator BrnRendererModule::Update lends us each GamePrepare
        // pass; the loading flow's world drive FreeAll's it and passes it to the world
        // virtual (boot audit F-P2-4 / F-P6-12). Null until the first not-done pass.
        CgsMemory::LinearMalloc* GetReusableLoadingScreenAllocator() { return mpReusableLoadingScreenAllocator; }

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

        // ARTIST823CB758/823CBF70 consume the actual supplied GUI output
        // under the caller's read lock, and obtain its native queue themselves.
        void BridgeGuiToGame(const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput);
        void BridgeGuiToDirector(BrnDirector::DirectorIO::InputBuffer* lpDirectorInput,
                                 const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput);

        // X360 0x823C0A58 -- hand the GUI output buffer's out-event queue to the sound root input
        // buffer (SetGuiEventQueue) so the sound module can drain GUI events.
        void BridgeGuiToSound(BrnSound::Module::Io::RootInputBuffer* lpSoundModuleInputBuffer,
                              const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer);

        // X360 0x823DDB78 -- drain the GUI output buffer's out-event queue (VariableEventQueue<18432,16>)
        // and translate each recognised GUI event into the matching game-state event, AddEvent'd into the
        // game-state module's post-world input GameEventQueue (VariableEventQueue<1536,16>). A handful of
        // events also drive the training manager (RequestTraining / OnEnableTrainingTips). Returns the
        // final queue-walk status. Called by DoUpdate_GameStatePostWorld / LoadingScriptedState::Update.
        // [P1 sim-pause] SIGNATURE ADAPTED to take the out-event queue directly (the
        // BridgeGuiToDirector precedent: the PC GuiModule owns the queue, not a console
        // CgsGuiModuleIO::OutputBuffer), and the walk carries the same channel-40 PC-ABI
        // adapter that bridge documents.
        int BridgeGuiToGameState(BrnGameState::GameStateModule* lpGameStateInput,
                                 const CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue);
        // Original typed target; required by the temporary partial-loading route.
        int BridgeGuiToGameState(BrnGameState::GameStateModuleIO::PostWorldInputBuffer* lpGameStateInput,
                                 const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput);

        // X360 0x823C54D8 [P1 sim-pause] -- walk the game-state output buffer's game-action
        // queue under its read lock: action 86 sets mbSimPaused and STOPS the sim timer
        // (the console's `*(gm+0x9A0621)=1; *(gm+0x9A0B08)=0` pair -- 0x9A0B08 is
        // simTimer.mbRunning); action 87 clears mbSimPaused and restarts it. The frozen
        // world IS the stopped sim timer: every sim timestep derives from it. Called by
        // DoUpdate_GameStatePreWorld after the game-state pre-world leg.
        void CheckGameActions(BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput);

        // X360 0x823DEEB8 -- drain a GUI event queue (VariableEventQueue<18432,16>) and translate each
        // network-bound GUI event into the matching network IN-event, AddEvent'd into the network module's
        // in-event queue (VariableEventQueue<14000,16>). Called by DoUpdate_NetworkPostSim.
        void TranslateGuiEventsToNetworkEvents(CgsModule::VariableEventQueue<14000, 16>* lpNetworkInputQueue,
                                              const CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue);

        // ---- network-output bridge family (GameSource/Unity/../Game/GameBridgeNetworkToX.cpp) -----
        // The mirror of the controller/replay bridges for the network module: each reads the network
        // module's OUTPUT buffer (BrnNetwork::BrnNetworkModuleIO::OutputBuffer + its interfaces) and
        // republishes it into the GUI + game-state subsystems. lpGuiInput is the GUI input buffer the
        // translated GUI events are queued into.
        //
        // X360 0x823E9518 -- top-level per-frame network->GUI bridge: run both translators below, bulk-
        // append the network GUI event queue into the GUI input queue, then synthesise the three player-
        // snapshot GUI events (player-list / player-status / lobby-player-list). Called by DoUpdate_GUI /
        // LoadingScriptedState::Update.
        void BridgeNetworkToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
                                const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutput);

        // X360 0x823DF9E0 -- drain the network event queue (VariableEventQueue<14000,16>) and translate
        // each recognised network event into the matching game-state event, AddEvent'd into the PreWorld
        // input buffer's game-event queue (VariableEventQueue<1536,16>). Called by BridgeNetworkToGameState.
        void TranslateNetworkEventsToGameEvents(BrnGameState::GameStateModuleIO::PreWorldInputBuffer* lpGameStateInput,
                                                const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutput);

        // X360 0x823E0900 -- drain the same network event queue and translate each GUI-bound network
        // event into the matching GUI event, pushed through the GUI module (this + 7252512).
        void TranslateNetworkEventsToGuiEvents(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
                                               const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutput);

        // X360 0x823DF938 -- walk the network output's live-revenge update interface and push one
        // BrnGui::GuiLiveRevengeUpdateEvent per record through the GUI module.
        void TranslateNetworkInterfaceToGuiEvents(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
                                                  const BrnNetwork::BrnNetworkModuleIO::NetworkToGuiInterface* lpNetworkToGuiInterface);

        // ---- the input pre-world leg (FX-RUMBLE3 2026-09-24, crash-parity G10-D4) ---------------
        // X360 0x823C5650 (DWARF BrnGameModule.h:550, BrnGameModuleUpdateFunctions.cpp:45; callers
        // DoUpdate @0x823F0AF8 and LoadingScriptedState::Update @0x823F22D8). The FIRST leg of every
        // update step: an IOHelper carves the "InputPreWorld" PreWorldInputBuffer off the INPUT stack,
        // the game state's rumble requests are bridged into it under its write lock, and the input
        // module's PreWorldUpdate (vtable slot 17) drains them onto the pads. Returns the port START
        // was pressed on (4 == none), for DoUpdate_InputPostWorld's bind.
        u32 DoUpdate_InputPreWorld(CgsModule::IOBufferStack* lpInputBufferStack,
                                   CgsModule::IOBufferStack* lpOutputBufferStack,
                                   CgsInput::InputIO::OutputBuffer* lpInputModuleOutput);
        // ARTIST 0x823C59E8: helper buffer is allocated on the input stack; the third/fourth
        // arguments are the const GameState output and const World output read during this leg.
        void DoUpdate_InputPostWorld(CgsModule::IOBufferStack* lpInputBufferStack,
                                     CgsModule::IOBufferStack* lpOutputBufferStack,
                                     const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput,
                                     const BrnWorldIO::UpdateOutputBuffer* lpWorldOutput,
                                     u32 liStartPressedPort);
        // ARTIST 0x823BCF40 / 0x823BD390.
        void PrepareInitialInputMapping(CgsInput::InputIO::PostWorldInputBuffer* lpPostWorldInput);
        void BindInputToPort(s32 liPort, CgsInput::InputIO::PostWorldInputBuffer* lpPostWorldInput);

        // ARTIST 823EE0E8: original per-step input staging and whole owner update.
        void DoUpdate_GameStatePreWorld(CgsModule::IOBufferStack* lpInputBufferStack,
                                       CgsModule::IOBufferStack* lpOutputBufferStack,
                                       const CgsInput::InputIO::OutputBuffer* lpInputModuleOutput,
                                       const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutput,
                                       BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput,
                                       BrnUpdateSet lUpdateSet);

        // ---- the network legs of the per-frame cascade and their bridges ------------------------
        // DoUpdate_NetworkPreSim (called by DoUpdate): perf monitors; carve a
        // PreSimulationInputBuffer "NetworkPreSim" off the INPUT stack; under the W(pre-sim input) +
        // R(input output) lock pair stage player 0's controller port and pad-idle flag, the game
        // timer status and the system-menu flag; mNetworkModule.ProcessBeforeSimulation(both stacks,
        // the buffer, lpNetworkOutputBuffer, lUpdateSet), whose result is latched; destroy the buffer.
        void DoUpdate_NetworkPreSim(CgsModule::IOBufferStack* lpInputBufferStack,
                                    CgsModule::IOBufferStack* lpOutputBufferStack,
                                    const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer,
                                    BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutputBuffer,
                                    BrnUpdateSet lUpdateSet);

        // DoUpdate_NetworkPostSim (called by DoUpdate): perf monitors; carve a
        // PostSimulationInputBuffer "NetworkPostSim" off the INPUT stack; under the W(post-sim
        // input) + R(game-state / world / GUI outputs) locks run BridgeWorldToNetwork (skipped when
        // lUpdateSet carries 0x20), BridgeGameStateToNetwork, append the GUI out-event queue into the
        // buffer's GUI queue and TranslateGuiEventsToNetworkEvents; unlock;
        // mNetworkModule.ProcessAfterSimulation(both stacks, the buffer, lUpdateSet); destroy it.
        void DoUpdate_NetworkPostSim(CgsModule::IOBufferStack* lpInputBufferStack,
                                     CgsModule::IOBufferStack* lpOutputBufferStack,
                                     const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutputBuffer,
                                     const BrnWorldIO::UpdateOutputBuffer* lpWorldOutputBuffer,
                                     const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer,
                                     BrnUpdateSet lUpdateSet);

        // Called by DoUpdate_World: TranslateNetworkEventsToWorld, then append the network output's
        // vehicle driver-input and vehicle-input interfaces into the world input and install its
        // crash and traffic network interfaces.
        void BridgeNetworkToWorld(BrnWorldIO::UpdateInputBuffer* lpWorldInput,
                                  const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutput);

        // Called first by BridgeNetworkToWorld: walk the network output's event queue and hand the
        // world-bound events on -- car colour / paint finish, lost / regained contact and car-select
        // status into the world input's per-car arrays, local-player-left / local-player-disconnected
        // and restart-traffic onto the world input's game-action queue.
        void TranslateNetworkEventsToWorld(BrnWorldIO::UpdateInputBuffer* lpWorldInput,
                                           const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutput);

        // Called by DoUpdate_GameStatePreWorld: append the network output's game-event and takedown
        // queues into the pre-world input, merge its NetworkToGameStateInterface, install the player
        // status and player-results interfaces, then TranslateNetworkEventsToGameEvents.
        void BridgeNetworkToGameState(BrnGameState::GameStateModuleIO::PreWorldInputBuffer* lpGameStateInput,
                                      const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutput);

        // Called by DoUpdate_NetworkPostSim: walk the game-state output's game-action queue and
        // translate each network-bound action into the post-sim input's network queue.
        void BridgeGameStateToNetwork(BrnNetwork::BrnNetworkModuleIO::PostSimulationInputBuffer* lpNetworkInput,
                                      const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput);

        // Called by DoUpdate_NetworkPostSim: copy the world output's player controls, vehicle output,
        // active-race-car, traffic and crash network interfaces into the post-sim input, then walk
        // the vehicle output's event queue and, for the player car's crash events, queue a network
        // event onto the post-sim network-event queue.
        void BridgeWorldToNetwork(BrnNetwork::BrnNetworkModuleIO::PostSimulationInputBuffer* lpNetworkInput,
                                  const BrnWorldIO::UpdateOutputBuffer* lpWorldOutput);

    private:
        // Case-52 helper for TranslateNetworkEventsToGuiEvents: scoreboard-response heading sub-switch
        // (category/variation/index) -- copy the per-name string list into the matching scoreboard event
        // (with the CgsStringUtils "String too long" guard) and push it. Not a distinct X360 function
        // (inlined into 0x823E0900); factored out here for the switch's readability.
        void TranslateScoreboardResponse(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
                                         const BrnNetwork::BrnNetworkModuleIO::NetworkOutScoreboardHeadingList* lpHeadingList);
    public:

        // ---- sound-output bridge family (GameSource/Unity/../Game/GameBridgeSoundToX.cpp) ---------
        // X360 0x823C63C0 (DWARF BrnGameModule.h:799 / GameBridgeSoundToX.cpp:103) -- walk the sound
        // root pre-update output's audio-effects message queue and notify the training manager of
        // each finished voiceover. The family's other bridges (BridgeSoundToResource :41 /
        // BridgeSoundToGuiPreUpdate :134 / ...) are grown here when reconstructed.
        void BridgeSoundToTraining(BrnSound::Module::Io::RootPreUpdateOutputBuffer* lpSoundOutputBuffer);
        // @ 0x823CDC98 (GameBridgeSoundToX.cpp:81; phase C3). The sound -> world
        // pre-update bridge: ONE append -- the world input's audio-car-loaded
        // queue takes the sound pre-update block's queue (asserts cpp:83/:84).
        void BridgeSoundToWorld(BrnWorldIO::UpdateInputBuffer* lpWorldInputBuffer,
                                BrnSound::Module::Io::RootPreUpdateOutputBuffer* lpSoundOutputBuffer);

        // @ 0x823CD580 (phase C3b). The world -> sound input bridge: the vehicle-data
        // copy (the update-set bit 0x100 picks the REPLAY active-race-car source), the
        // seven interface installs, and the three queue appends (audio-car-loaded /
        // game events / prop-became-physical / prop-update notifications).
        void BridgeWorldToSound(BrnSound::Module::Io::RootInputBuffer* lpSoundInputBuffer,
                                const BrnWorldIO::UpdateOutputBuffer* lpWorldOutputBuffer,
                                BrnUpdateSet leUpdateSet);

        // @ 0x823C11F8. THE WORLD -> EFFECTS DISPATCH BRIDGE, and the ONLY producer of the
        // effects side's white level anywhere in the image: it is the only xref of
        // BrnEffects::EffectsIO::DispatchInputBuffer::SetWhiteLevel @0x823BAB40. Four legs, all
        // read out of the world's dispatch OUTPUT buffer -- key-light direction, key-light
        // colour, average irradiance colour, white level. Called once, from DoDispatch
        // @0x823DC458, next to BridgeRendererToEffects @0x823C1168 (which carries the frames and
        // the env map and does NOT touch the white level -- the two were confused for three
        // commits; see the body's banner). Home: GameBridgeWorldToX.cpp.
        void BridgeWorldToEffects_Dispatch(BrnEffects::EffectsIO::DispatchInputBuffer* lpEffectsInputBuffer,
                                           const BrnWorldIO::DispatchOutputBuffer* lpWorldOutputBuffer);

        // @ 0x823CDE50 (phase C3b). The game-state -> sound input bridge: the 13312
        // game-action queue append, the game-mode/scoring/online-scoring interface
        // installs, and the UpdateInfo byte (the two +1009411x flags OR'd).
        void BridgeGameStateToSound(BrnSound::Module::Io::RootInputBuffer* lpSoundInputBuffer,
                                    const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutputBuffer);

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
        // ARTIST records eight128-byte render-metrics samples. The vectors give
        // the original16-byte alignment; the twelve scalar monitors end at116.
        struct RenderMetricsSample
        {
            Vector3 mCameraPosition;
            Vector3 mLookAt;
            u32 muIP;
            char macBuildDate[16];
            char macBuildTime[16];
            f32 mfCPUDispatchThread;
            f32 mfCPUShadowmapNear;
            f32 mfCPUShadowmapFar;
            f32 mfCPUEnvmap;
            f32 mfCPUPreZ;
            f32 mfCPUWorldOpaque;
            f32 mfCPUWorldTransparent;
            f32 mfGPUShadowmap;
            f32 mfGPUEnvmap;
            f32 mfGPUPreZ;
            f32 mfGPUWorldOpaque;
            f32 mfGPUWorldTransparent;
        };

        // Genuine contained types, in the native constructor's declaration order.
        // Their independent layout recovery remains necessary for the hard Main gate.
        BrnRendererModule mRenderModule;                              //4400
        WorldModule mWorldModule;                                    //10E80
        BrnResource::GameDataModule mGameDataModule;                   //5F4B00
        BrnGameState::GameStateModule mGameStateModule;                //669500
        BrnDirector::DirectorModule mDirectorModule;                   //6B0B10
        CgsInput::InputModule mInputModule;                           //6E9630
        BrnGui::GuiModule mGuiModule;                                 //6EAA20
        BrnEffects::EffectsModule mEffectsModule;                     //878700
        BrnSound::Module::RootSoundModule mSoundModule;                //8A7D00
        BrnReplays::ReplayModule mReplayModule;                       //8BD300
        BrnNetwork::BrnNetworkModule mNetworkModule;                   //8C3600
        CgsModule::IOBufferStack* mpUpdateInputBufferStack;            //996F00
        CgsModule::IOBufferStack* mpUpdateOutputBufferStack;           //996F04
        CgsModule::IOBufferStack* mpResourceInputBufferStack;          //996F08
        CgsModule::IOBufferStack* mpResourceOutputBufferStack;         //996F0C
        BrnResource::GameDataIO::InputBuffer* mpGameDataModuleInputBuffer;  //996F10
        BrnResource::GameDataIO::OutputBuffer* mpGameDataModuleOutputBuffer;//996F14
        BrnCpuMonitors mCpuMonitors;                                  //996F18
        CgsDev::DebugManager mDebugManager;                           //996FB8
        CgsResource::SafeResourceHandle<CgsResource::Font> mDebugFont;  //99F150
        CgsDev::MapFile::MinimalMemoryReader mMapFileReader;           //99F158
        s32 miWorldModuleUpdateTrace;                                 //9A061C
        bool mbHasGameTerminated;                                    //9A0620
        bool mbSimPaused;
        bool mbDiskError;
        bool mbSimPausedBeforeDiskError;
        bool mbControllerDisconnected;
        bool mbRequestStreamingWait;                                 //native mbGameStart
        bool mbGameOver;
        bool mbPrevStalled;
        bool mbStalled;
        bool mbRequestDoStepFrame;
        bool mbRequestDoPlayFrame;
        bool mbWorldStreamingRequestedStall;
        bool mbStreamingStalled;
        bool mbIsLoadingScreenVisible;                               //9A062D
        CgsMemory::LinearMalloc* mpReusableLoadingScreenAllocator;     //9A0630
        bool mbPlayerCarCrashing;
        bool mbOnline;
        bool mbIsSysMenuShowing;
        EInputBindState miInputModuleState;                           //9A0638
        u32 miPlayer0ControllerPort;
        u32 miSecondaryControllerPort;
        EGuiFlowStart miGuiFsmStage;                                  //9A0644
        bool mbGuiPhaseComplete;
        bool mbGuiPreAccept;
        EPrepareStage mePrepareStage;                                //9A064C
        EReleaseStage meReleaseStage;
        EGamePrepareStage meGamePrepareStage;
        EGameReleaseStage meGameReleaseStage;
        EGameUpdateStage meGameUpdateStage;
        s32 miNumSimFramesRequired;
        BrnGameMainFlowController::GameMainFlowController mMainFlowStateMachine;//9A0664
        CgsModule::EventReceiverQueue<1024,16> mGamePrepareReceiverQueue;//9A06BC
        CgsSystem::Timer mGameTimer;                                 //9A0AD4
        CgsSystem::Timer mSimTimer;
        CgsSystem::TimerStatusInterface mTimerStatusInterface;
        CgsSystem::TimerRequestInterface mTimerRequestInterface;
        CgsSystem::FrameRateManager mFrameRateManager;                //9A0B50
        CgsSystem::EFrameRateManagerType meFrameRateManagerType;
        CgsSystem::FrameRateTypeRequestInterface mFrameRateTypeRequestInterface;
        s8 mi8FrameRateMinSteps;
        s8 mi8FrameRateMaxSteps;
        s8 mi8ActualFrameRateMinStepsThisFrame;
        s8 mi8ActualFrameRateMaxStepsThisFrame;
        bool mbSteppingFrames;
        bool mbDoStep;
        bool mbStopStepping;
        bool mbForceShutdown;
        u8 mu8DebugFpsFrameStampIndex;                                //9A0B9C
        u64 mau64DebugFpsFrameStamps[3];
        u64 mu64DebugFpsAverageWindowStamp;
        f32 mfDebugFpsAccumulator;
        f32 mfDebugFpsAverage;
        f32 mfDebugFpsSampleCount;
        CgsGui::CgsGuiModuleIO::InputBuffer* mpGuiInputBuffer;         //9A0BCC
        CgsGui::ViewIO::InputBuffer* mpGuiViewInputBuffer;
        CgsGui::ModelIO::OutputBuffer* mpGuiModelOutputBuffer;
        CgsGui::CgsGuiModuleIO::OutputBuffer* mpGuiOutputBuffer;
        BrnDirector::DirectorIO::OutputBuffer* mpDirectorOutputBuffer;
        bool mbTakeMetrics;                                         //9A0BE0
        bool mbSendMetrics;
        Matrix44Affine mRecordMetricsCamMatrix;
        RendererIO::ExternallyVisiblePerformanceMonitors mRendererModulePerfmonsForREADONLYAccess;
        RenderMetricsSample maRenderMetricsSamples[8];               //9A0C60
        u32 muRenderMetricsCount;                                    //9A1060
        s32 miBrightness;
        s32 miContrast;
        bool mbEnableCalibrationUnfriendlyPostFx;
        bool mbControllerEnterWithCircle;
        BrnReplays::DirectorBridgeSerialiser mDirectorBridgeSerialiser;//9A1070
        BrnReplays::GameModuleSerialiser mGameModuleSerialiser;
        CgsSystem::ThreadLayout mThreadLayout;                        //9A1128
        DispatchThreadInputBufferManager mDispatchThreadInputBufferManager;
        EA::Thread::Thread mErrorThread;
        BrnHW::System360HW mHardware;                                 //9A11C0
        f32 mfLookbackHoldTimer;
        bool mbTakeScreenShots;
        u64 muCatchupStartTime;                                      //9A12B8 (DWARF name)
        // Original Construct explicitly clears both additional64-bit timing fields;
        // their Xbox-specific declaration spellings are unavailable.
        u64 mTimingValue12C0;
        u64 mTimingValue12C8;

    };

    // operator++(EReleaseStage&, int) @ 0x823A8AD8 (X360 ARTIST), defined in BrnGameModule.cpp.
    // POST-increment for the BrnGameModule::EReleaseStage stage machine (the trailing dummy int is
    // the post-fix marker): saves the old value, advances the stage, asserts the advanced value has
    // not run past E_RELEASESTAGE_DONE (=7), and returns the OLD value. Called by
    // BrnGame::BrnGameModule::Release to step meReleaseStage. Declared here near the enum (the enum
    // is BrnGameModule::EReleaseStage, public) so all Release-path users of the operator see it.
    BrnGameModule::EReleaseStage operator++(BrnGameModule::EReleaseStage& leStage, int);

}

// ARTIST off_830102D0; DecFIGS GameSource/Main/BrnMain.cpp names gpBurnoutGame.
// The native entry publishes this exact process global after placement construction.
extern BrnGame::BrnGameModule* gpBurnoutGame;
