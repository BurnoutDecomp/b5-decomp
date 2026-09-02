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
// !!! ODR TRAP WARNING !!! The remaining empty stubs below (GameStateModule/DirectorModule/InputModule/
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
namespace BrnWorldIO { struct UpdateInputBuffer; struct UpdateOutputBuffer; }
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
// DoUpdate_Sound @0x823DCEC0 parameter types (phase C4b) -- the sound root OUTPUT buffer,
// the replay pre-sim OUTPUT buffer, and the effects OUTPUT buffer. Forward-declared; the
// leg body includes the real homes.
namespace BrnSound { namespace Module { namespace Io { struct RootOutputBuffer; } } }
namespace BrnReplays { namespace ReplayIO { struct OutputBuffer_PreSim; } }
namespace BrnEffects { namespace EffectsIO { struct OutputBuffer; } }
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
namespace CgsInput     { class InputModule     : public CgsModule::ModuleSingleBuffered {}; }
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
namespace BrnEffects { namespace EffectsIO { struct InputBuffer; } }
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
        // The world module (the loading flow's LoadWorldModule drives its Prepare with
        // the update IO stacks -- X360 vtable +68 dispatch @0x823E72F0).
        WorldModule& GetWorldModule() { return mWorldModule; }
        // The game-state module. Same shape as the two above: the console reaches its OUTPUT
        // buffer through DoUpdate's LockBuffersForIO set, but DoUpdate is a PC-platform leaf, so
        // the live world drive (DriveWorldUpdateFrame) needs it to run BridgeGameStateToWorld.
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

        // Per-frame spines the in-game flow state drives directly (non-virtual; each returns
        // an int status the void flow-state Update/Render slots discard):
        //   DoUpdate   -- the game module's per-frame update spine  (MainGameFlowStateInGame::Update)
        //   DoDispatch -- the game module's per-frame render-dispatch spine (MainGameFlowStateInGame::Render)
        int DoUpdate();
        int DoDispatch();

        // @ 0x823EE4D8 -- the SOUND pre-update leg of the per-frame cascade (both the full
        // spine and LoadingScriptedState::Update @0x823F2714 call it, ahead of the network /
        // game-state / world drives). Console body, statement for statement:
        //   StartMonitor(mCpuMonitors.miUT_Sound); StartMonitor(miUT_SoundUpdate);
        //   mSoundModule.PreUpdate(lpStack, lpPreUpdateOut);
        //   { W(guiIn) + R(preUpdateOut) lock pair (console helper sub_823B6FE0):
        //     guiIn->GetGuiEvents()->Append<256,16>(preUpdateOut's GuiOut queue);
        //     BridgeSoundToTraining(preUpdateOut); unlock pair }
        //   StopMonitor(miUT_SoundUpdate); StopMonitor(miUT_Sound);
        // The stack argument is the update-OUTPUT stack (only forwarded into PreUpdate,
        // which carves its "SoundLogicPreUpdateOutput" scratch from it).
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
                            BrnWorldIO::UpdateOutputBuffer* lpWorldUpdateOutputBuffer,
                            CgsMemory::LinearMalloc* lpWorldFrameAllocator,
                            BrnUpdateSet lUpdateSet);

        // The per-frame DIRECTOR leg (2026-07-29, DJ fly-by campaign). Creates this
        // sub-step's director INPUT + scene-query IO buffers on the update stacks and runs the
        // console's own three-pass order around the caller's GUI update:
        //   PreSceneQueryUpdate @0x8225C768   (the cameras may ISSUE scene queries)
        //   Update              @0x82275300   (consume last frame's answers, drive the
        //                                      arbitrator, finalise and PUBLISH the camera)
        //   PostGuiUpdate       @0x82250DD0   (post-GUI latch)
        // The OUTPUT buffer is the frame's mpDirectorOutputBuffer (CreateStaticIOBuffers), so
        // the published camera survives into DoDispatch, which runs before it is destroyed.
        // lbPostGui selects the third pass; the first two run together on the pre-GUI call.
        void DoUpdate_Director(bool lbPostGui);

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
        void UpdateTimers();

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

        // ⭐ THIS SUB-STEP'S WORLD UPDATE OUTPUT BUFFER (2026-08-01, camera wave).
        // The console's DoUpdate @0x823F0AF8 creates ONE BrnWorldIO::UpdateOutputBuffer per
        // sub-step, near the top, and threads the same pointer through DoUpdate_World,
        // DoUpdate_InputPostWorld, DoUpdate_Director, DoUpdate_Effects, ... destroying it at
        // the very bottom. On this build the world leg was owned by the flow states'
        // DriveWorldUpdateFrame, which created the buffer, drove the world into it and
        // DESTROYED it inside one call -- so no later leg could ever read what the world
        // published. Hoisted to the same per-sub-step lifetime as the GUI/director buffers
        // (CreateStaticIOBuffers / DestroyStaticIOBuffers) so the director's own leg can.
        // Fold into the real DoUpdate cascade when the module scheduler moves under the game
        // module's own spines.
        BrnWorldIO::UpdateOutputBuffer* GetWorldUpdateOutputBuffer() { return mpWorldUpdateOutputBuffer; }
        BrnEffects::EffectsIO::OutputBuffer* GetEffectsOutputBuffer() { return mpEffectsOutputBuffer; }
        BrnEffects::EffectsModule& GetEffectsModule() { return mEffectsModule; }
        // The PC pad fill's output buffer (driving-input wave 2026-08-11): the live world
        // drive (DriveWorldUpdateFrame) needs it to run BridgeControllerToWorld, exactly as
        // GetWorldUpdateOutputBuffer above serves the same caller.
        CgsInput::InputIO::OutputBuffer* GetPcInputOutputBuffer() { return &mPcInputOutputBuffer; }

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

        // The per-frame debug overlay drive (X360 @0x823BCB88, called from DoDispatch @0x823DC458):
        // gate on the debug font + the published debug render buffers, keep the fps ring/average,
        // queue the build-info/fps/memory readouts, then DebugManager::Render. The console signature
        // takes the RendererIO OUTPUT buffer; on this build the dispatch IO pair does not exist yet
        // (boot audit F-P2-4) so the method reads the same state directly - see the body's FLAG.
        void DebugManagerRender();

        // IThreadClass implementation (the engine drives these on their threads).
        bool UpdateThread() override;
        void DispatchThread() override;                               // @ BrnGameModule.cpp:1221
        void ResourceUpdateThread(Mutex* lpMutex) override;
        void OnStartOfUpdateFrame() override;
        void OnEndOfUpdateFrame() override;
        void OnCompletionOfVsyncWait() override;
        void RenderAssert(const AssertData* lpAssertData) override;

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

        // ⭐ ADDITIVE (faithful-audio-engine phase C4; header-only inlines). The console
        // computes RootSoundModule::Update's two f32 time steps inline in each spine --
        // LoadingScriptedState::Update @0x823F2AB0-C4 and DoUpdate_Sound @0x823DCEC0 both
        // load gameTimer(gm+0x9A0AD4)/simTimer(gm+0x9A0AF0) fields [+0x10]*[+0xC]
        // (mfScaleCurrent * mfRate) straight off the module. Exposed BY NAME so the live
        // PC spine reads the same pair without a console byte offset.
        const CgsSystem::Timer& GetGameTimer() const { return mGameTimer; }
        const CgsSystem::Timer& GetSimTimer() const  { return mSimTimer; }

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

        // ---- ⚠️ FLAG PC quality-of-life: the dispatch camera latch --------------------
        // NOT X360 functions. The renderer can now draw frames on which NO simulation
        // sub-step ran (see mi8FrameRateMinSteps), and on such a frame the per-sub-step IO
        // buffers DoDispatch used to read the camera out of do not exist -- they are created
        // and destroyed inside the sub-step loop. So the director's published camera is
        // latched into the game module at the end of every sub-step, one tick deep, and
        // DoDispatch renders the blend of the last two.
        //
        // That is both halves of the same problem solved by one mechanism: it is what keeps
        // a zero-step frame from falling back to the world module's tour camera (a visible
        // jump), and it is what makes a 60 Hz camera read as continuous motion at 144 Hz.
        // DELETE-WHEN: never, while the simulation is decoupled from the render rate.
        void LatchDispatchCamera();
        const BrnDirector::Camera::Camera* GetInterpolatedDispatchCamera();

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

        // ⭐⭐ [gateui] X360 0x823E9CE0 -- the GAME-ACTION -> GUI-EVENT translator: drain the
        // game-state output buffer's GameActionQueue (VariableEventQueue<13312,16>) and post
        // the matching BrnGui event for each recognised action. Signature from the asm
        // prologue (r3 = this, r4 = lpGuiInput, r5 = lpGameStateOutput); the two null asserts
        // are GameBridgeGameStateToX.cpp:754/755. Called by BridgeGameStateToGui @0x823EE880
        // (call site @0x823EF22C).
        // ⚠️ PARTIAL RECONSTRUCTION -- only the three STUNT-COLLECTIBLE arms (58/59/60) are
        // reproduced. The full body is a ~700-case switch; see the FLAG on the body.
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

        // [FLAG PC stand-in] Publish the car-select screen's car list to the GUI as events
        // 406 + 412 -- the two records BridgeGameStateToGui's action-182/184 cases carry on the
        // console. Full provenance + the DELETE-WHEN condition are on the body in
        // BrnGameModule.cpp; called from the per-sub-step GUI publish block in Update.
        void PublishCarSelectionToGui();

        // X360 0x823CB758 -- the GUI->game out-event consumer: walk the GUI module's out
        // events and latch the flow commands (19/20 loading screen, 70 phase complete,
        // 71 pre-accept/resume-world, 90 profile-first, 189 sign-out, 86/87/89 quit,
        // 138/589/590 render modes). FLAG PC-ABI adapter: the console form reads the GUI
        // module OUTPUT buffer's out-event queue; the PC module publishes the same queue
        // via BrnGui::GuiModule::GetGuiOutQueue().
        void BridgeGuiToGame(CgsModule::VariableEventQueue<18432, 16>* lpGuiOutQueue);

        // ⭐ X360 0x823CBF70 -- the GUI->DIRECTOR out-event consumer. Walks the same GUI
        // out-event queue BridgeGuiToGame walks and raises the matching published flag on the
        // director's INPUT buffer, which MainDirector::PostGuiUpdate then folds into the
        // director GameState. This is the ONLY route from the front-end to the director:
        // commands 476/477/478 are the new-profile-intro and the GAME-INTRO FLY-BY start/stop.
        // Called by DoUpdate_DirectorPostGUI @0x823DCE38, between LockBuffersForIO and
        // DirectorModule::PostGuiUpdate. FLAG PC-ABI adapter, same one BridgeGuiToGame
        // carries: the console form takes the GUI module OUTPUT buffer and calls
        // CgsGuiModuleIO::OutputBuffer::GetOutEventQueu on it; the PC module publishes that
        // same queue directly via BrnGui::GuiModule::GetGuiOutQueue().
        void BridgeGuiToDirector(BrnDirector::DirectorIO::InputBuffer* lpDirectorInput,
                                 CgsModule::VariableEventQueue<18432, 16>* lpGuiOutQueue);

        // ⭐ X360 0x823E3AB0 -- the WORLD->DIRECTOR seam, the ONLY caller of
        // DirectorIO::InputBuffer::SetRaceCarInfo in the whole image and therefore the only
        // way a race car's pose reaches the director's cameras. Signature recovered from the
        // ASM (r3/r4/r5 only -- IDA's 16-parameter prototype is stack-slot noise); the DWARF
        // home is GameSource/Game/GameBridgeWorldToX.cpp, where the body lives.
        // Called by DoUpdate_Director @0x823E8DE0 with this sub-step's director INPUT buffer
        // and the world UPDATE OUTPUT buffer.
        void BridgeWorldToDirector(BrnDirector::DirectorIO::InputBuffer* lpDirectorInput,
                                   const BrnWorldIO::UpdateOutputBuffer* lpWorldOutput);

        // ⭐⭐ [gateui] X360 0x823E5368 -- the WORLD->GAME-STATE seam, the hop that carries the
        // world's per-frame game events (including event 111 E_EVENT_RECORD_PROP_HIT, the
        // smash-gate / billboard feed) into the game state. Signature from the asm prologue
        // (r3 = this, unused; r4 = the post-world INPUT buffer, write-locked; r5 = the world
        // UPDATE OUTPUT buffer, read-locked). Console home + body:
        // GameSource/Game/GameBridgeWorldToX.cpp, where the ten console legs, the two landed
        // here, and the eight parks (each with its blocker) are enumerated.
        // Called by DoUpdate_GameStatePostWorld @0x823E92A8 inside its LockBuffersForIO
        // bracket -- an entry point this build does not have yet, so this function has no PC
        // call site; see the banner on the body.
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
        // This sub-step's GUI module OUTPUT buffer (same lifetime; the sound spine's
        // BridgeGuiToSound source -- faithful-audio-engine phase C4).
        CgsGui::CgsGuiModuleIO::OutputBuffer* GetGuiOutputBuffer() { return mpGuiOutputBuffer; }
        // This sub-step's DIRECTOR OUTPUT buffer (CreateStaticIOBuffers lifetime; the console
        // holds it as the persistent member gm+0x9A0BDC and DoUpdate passes it into
        // DoUpdate_Sound's SetCameraInput leg -- phase C4b).
        BrnDirector::DirectorIO::OutputBuffer* GetDirectorOutputBuffer() { return mpDirectorOutputBuffer; }
        // The console reaches the GUI module through the module scheduler (`*off_830102D0 +
        // 0x6EAA20`, the raw member address, in LoadGUIModule @0x823EF310 and everywhere else
        // on the loading path). The PC has no scheduler, so the loading-screen state reaches
        // it by name -- it is the same object at the same place in the module list.
        BrnGui::GuiModule& GetGuiModule() { return mGuiModule; }
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

        // X360 0x823C54D8 [P1 sim-pause] -- walk the game-state output buffer's game-action
        // queue under its read lock: action 86 sets mbSimPaused and STOPS the sim timer
        // (the console's `*(gm+0x9A0621)=1; *(gm+0x9A0B08)=0` pair -- 0x9A0B08 is
        // simTimer.mbRunning); action 87 clears mbSimPaused and restarts it. The frozen
        // world IS the stopped sim timer: every sim timestep derives from it. Called by
        // DoUpdate_GameStatePreWorld after the game-state pre-world leg.
        void CheckGameActions(BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput);

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
        // ⭐ 2026-08-16 (boot audit F-P0-10). TUB's WinMain @0x79D580 latches "-skipvideos"
        // off the command line into the game module right after Construct. The latch had
        // been gated as "its boot-video consumer is not in this layout yet" -- it is now
        // (BrnGui::BootVideos short-circuits to DONE the same way it does for a soft
        // reboot), so the flag is real.
        void SetSkipVideos(bool lbSkipVideos) { mbSkipVideos = lbSkipVideos; }
        bool GetSkipVideos() const            { return mbSkipVideos; }

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
        // h:388 -- the DEBUG FONT handle, X360 gm+0x99F150 (10482000). GamePrepare's id-5
        // acquire stores {mpResourceMemory, mpSourceEntry} straight into it (@0x823EFFD4/D8)
        // and then hands THIS OBJECT to DebugManager::SetDebugFont, so it has to outlive the
        // pass -- it is the module's member on the console for exactly that reason.
        CgsResource::SafeResourceHandle<CgsResource::Font> mDebugFont;   // h:388 (gm+0x99F150)
        // [h:389-404: map-file reader, juice, mbHasGameTerminated - omitted]
        bool mbSimPaused;                                            // h:405
        bool mbDiskError;                                            // h:406
        // [h:407-411: disk-error/controller/game-start/over/prev-stalled flags - omitted]
        bool mbStalled;                                              // h:412
        bool mbRequestDoStepFrame;                                   // h:413
        bool mbRequestDoPlayFrame;                                   // h:414
        // [h:415-453: streaming flags, input-bind state, gui-flow, prepare stages - omitted]
        EReleaseStage    meReleaseStage;                             // h:455 (Release() stage machine)
        EGamePrepareStage meGamePrepareStage;                        // h:456 (X360 gm+10094164)
        // [h:457: meGameReleaseStage - omitted]
        EGameUpdateStage meGameUpdateStage;                          // h:458
        s32 miNumSimFramesRequired;                                  // h:459
        BrnGameMainFlowController::GameMainFlowController mMainFlowStateMachine; // h:462
        // [h:465: receiver queue - omitted]
        // ---- the game/sim timer pair + their published snapshot -------------------------
        // X360 gm+10095316 / gm+10095344 / gm+10095372. Construct @0x823C9EA8 Clears the
        // interface, Prepares BOTH timers with 1/refreshRate and sets each one running
        // (`*(gm+10095340) = 1` / `*(gm+10095368) = 1` == Timer::mbRunning @+24); UpdateTimers
        // @0x823BCFD0 ticks both once per sim sub-step; BridgeTimers @0x823BD150 snapshots
        // them into the interface and copies all 48 bytes into the director input buffer.
        // THIS PAIR IS THE FRAME TIMESTEP the whole camera/behaviour middle runs on
        // (TimerStatus::GetCurrentTimeStep == Timer::mfRate * Timer::mfScaleCurrent).
        CgsSystem::Timer                mGameTimer;                  // h:466  gm+10095316
        CgsSystem::Timer                mSimTimer;                   // h:467  gm+10095344
        CgsSystem::TimerStatusInterface mTimerStatusInterface;       // h:468  gm+10095372
        // ⭐ gm+10095420 -- THE PER-SUB-STEP REQUEST ACCUMULATOR, and the single choke point
        // every slow-motion producer in the game funnels through. DWARF
        // BrnGameModule.h:281 `TimerRequestInterface mTimerRequestInterface;`; the X360
        // addresses it at gm+0x9A0B3C == 10095420, i.e. directly after mTimerStatusInterface,
        // and treats it as ONE 16-byte TimerRequestInterface (UpdateTimers @0x823BCFD0
        // Appends into +0 and +8 and then hands the same base to ApplyToTimers). It used to be
        // elided as "[h:469-471: the two TimerRequests + the request interface - omitted]";
        // that reading counted the interface's own two sub-members as siblings.
        CgsSystem::TimerRequestInterface mTimerRequestInterface;     // h:281  gm+10095420
        CgsSystem::FrameRateManager      mFrameRateManager;          // h:472
        CgsSystem::EFrameRateManagerType meFrameRateManagerType;     // h:473
        // ⚠️ FLAG PC quality-of-life (no console counterpart -- the console reads this rate
        // out of the validated display mode). The FIXED simulation rate, in Hz: the size of
        // one simulation step, the divisor the frame-rate manager counts real elapsed time
        // against, and the base of every timestep downstream. See the banner at its use site
        // in BrnGameModule::Construct.
        static const u32 KU_SIMULATION_RATE_HZ = 60u;
        // [h:474: frame-rate type request interface - omitted]
        s8   mi8FrameRateMinSteps;                                   // h:475
        s8   mi8FrameRateMaxSteps;                                   // h:476
        s8   mi8ActualFrameRateMinStepsThisFrame;                    // h:477
        s8   mi8ActualFrameRateMaxStepsThisFrame;                    // h:478
        bool mbSteppingFrames;                                       // h:479
        bool mbDoStep;                                               // h:480
        bool mbStopStepping;                                         // h:481
        // The debug fps-readout state DebugManagerRender @0x823BCB88 keeps (h:482-490, the
        // "debug-framerate stats"; X360 offsets gm+0x9A0B85..0x9A0BC8). The ring holds the last
        // three frame timestamps, so the current fps is measured over a 3-frame window; every 60
        // seconds the accumulated per-frame fps collapses into the "over last minute" average.
        // [FLAG] mbDebugSimulationIsRealtime's writer (the console flips it when the sim drops out
        // of real time) is not yet recovered; Construct seeds it true, so the readout shows the
        // plain "%d fps" line.
        bool mbDebugSimulationIsRealtime;                            // gm+0x9A0B85
        u8   mu8DebugFpsFrameStampIndex;                             // gm+0x9A0B9C (mod 3 cursor)
        u32  mau32DebugFpsFrameStamps[3];                            // gm+0x9A0BA0 (u64 on X360; the PC timer is u32)
        u32  mu32DebugFpsAverageWindowStamp;                         // gm+0x9A0BB8 (u64 on X360)
        f32  mfDebugFpsAccumulator;                                  // gm+0x9A0BC0
        f32  mfDebugFpsAverage;                                      // gm+0x9A0BC4
        f32  mfDebugFpsSampleCount;                                  // gm+0x9A0BC8
        CgsGui::CgsGuiModuleIO::InputBuffer*   mpGuiInputBuffer;     // h:493
        CgsGui::ViewIO::InputBuffer*           mpGuiViewInputBuffer; // h:494
        CgsGui::ModelIO::OutputBuffer*         mpGuiModelOutputBuffer;// h:495
        CgsGui::CgsGuiModuleIO::OutputBuffer*  mpGuiOutputBuffer;    // h:496
        BrnDirector::DirectorIO::OutputBuffer* mpDirectorOutputBuffer;// h:497
        // [FLAG PC placement] this sub-step's world UPDATE OUTPUT buffer. On the console it is
        // a LOCAL of DoUpdate @0x823F0AF8 (created near the top, threaded through every leg,
        // destroyed at the bottom); DoUpdate is a PC-platform leaf here, so it lives with the
        // other per-sub-step static IO buffers instead. Same lifetime either way.
        // See GetWorldUpdateOutputBuffer() above.
        BrnWorldIO::UpdateOutputBuffer*        mpWorldUpdateOutputBuffer;
        // ⭐ THIS SUB-STEP'S EFFECTS OUTPUT BUFFER. The console's DoUpdate @0x823F0AF8
        // creates ONE EffectsIO::OutputBuffer near the top (pseudocode :387) and destroys it
        // at the very bottom (:1159), threading it through DoUpdate_Effects and (as a
        // lock-only participant) DoUpdate_Sound. DoUpdate is a PC leaf here, so it lives with
        // the other per-sub-step buffers in CreateStaticIOBuffers.
        BrnEffects::EffectsIO::OutputBuffer*   mpEffectsOutputBuffer;
        // ---- ⚠️ FLAG PC quality-of-life: the dispatch camera latch (no console members) ---
        // The last two simulation ticks' worth of the director's published camera, plus the
        // frame-local blend of them. See LatchDispatchCamera / GetInterpolatedDispatchCamera.
        // Latched by value because the buffer they are read out of only lives for the
        // sub-step that produced it.
        BrnDirector::Camera::Camera mPreviousTickCamera;
        BrnDirector::Camera::Camera mCurrentTickCamera;
        BrnDirector::Camera::Camera mInterpolatedCamera;
        bool                        mbCurrentTickCameraValid;
        bool                        mbPreviousTickCameraValid;
        // The double-buffered dispatch-thread input pair (X360 module +10097064): the update
        // side writes it (BridgeGuiToGame's loading-screen commands, IsStalled/IsDiskError,
        // brightness/contrast), OnEndOfUpdateFrame swaps it, and the dispatch/render side
        // reads it (BrnRendererModule::Render's per-frame AddCommand forward).
        BrnGame::DispatchThreadInputBufferManager mDispatchThreadInputBufferManager; // h:531
        // The calibration settings the game module OWNS and BridgeGuiToGame @0x823CB758 publishes
        // into the dispatch write buffer every update frame (its unconditional tail:
        // LockForWrite / SetBrightness / SetContrast / SetCalibrationUnfriendlyEnablePostFx /
        // SetCalibrationTextureHandle / UnlockForWrite). DWARF BrnGameModule.h:513/:514/:517
        // (X360 module +10096740 / +10096744 / +10096748). Construct @0x823C9EA8 seeds them
        // 50 / 50 / true -- the game's own default slider position (BrnGuiOptionsDataProfile's
        // KI_DEFAULT_BRIGHTNESS/CONTRAST), which is exactly what makes the post-fx composite's
        // brightness/contrast constants NEUTRAL (setting*0.01-0.5 == 0, setting*0.01+0.5 == 1).
        // Landed 2026-08-15 when the composite went live and read 0/0 off the buffer: the picture
        // came out at contrast 0.5 / brightness -0.5 -- black -- because nothing published these.
        s32  miBrightness;                          // h:513  (X360 +10096740)
        s32  miContrast;                            // h:514  (X360 +10096744)
        bool mbEnableCalibrationUnfriendlyPostFx;   // h:517  (X360 +10096748)
        // The GamePrepare response queue (X360 gm+10094268; its miCount is the
        // gm+10094276 the stage machine tests and its miStartOffset the gm+10094280
        // the event walk adds to the base). The X360 capacity is not recoverable from
        // the pseudocode -- GamePrepare receives at most 3 LoadBundleResponses plus the
        // 6 acquire responses, so the same <1024,16> instantiation WorldModule uses
        // covers it with headroom.
        CgsModule::EventReceiverQueue<1024, 16> mGamePrepareReceiverQueue;
        // [remaining members - omitted]

        // ---- controller-bridge inputs (real names; off-path absolute offsets noted) -------------
        // These are the game-module fields the BridgeControllerTo* family reads each frame. They sit
        // far past the boot-path members above (X360 absolute offsets in comments); declared here in
        // the omitted tail per the LAYOUT NOTE (semantic parity, not byte-exact).
        // ---- the GUI flow-FSM bridge state (X360-attested offsets) -----------------------
        s32  miGuiFsmStage;             // @ +10094148 (1..5 = pending RunFsm post; 6 = idle)
        bool mbGuiPhaseComplete;        // @ +10094152 (command 70 -- the flow-advance flag)
        bool mbSkipVideos;              // TUB WinMain's "-skipvideos" latch (boot audit F-P0-10)
        // X360 gm+0x9A0630 -- the reusable loading-screen allocator the renderer lends us
        // through RendererIO::OutputBuffer each GamePrepare pass (boot audit F-P2-4). Null
        // until the first pass runs.
        CgsMemory::LinearMalloc* mpReusableLoadingScreenAllocator;
        bool mbGuiPreAccept;            // @ +10094153 (command 71 -- resume-world-load)
        // [FLAG PC bring-up] (no console member): true while the DIRECTOR is the thing driving
        // the world camera -- i.e. from the frame the GUI's game-intro fly-by request reaches
        // the director until the arbitrator has finished unwinding its attract states.
        // DoDispatch routes the director's published camera to the world ONLY while this is
        // set; outside it the bring-up tour camera keeps the world alive. Set in
        // DoUpdate_Director. DELETE with GenerateDispatchListsBringUp.
        bool mbDirectorCameraLive;


        // @ +10094132 (0x9A0634). The console's own latch, written as the FIRST statement of
        // BridgeWorldToDirector @0x823E3AB0:
        //   mbPlayerCarCrashing = active->IsPlayerCarActive() && active->IsPlayerCarCrashing()
        // It has no reconstructed reader yet; published anyway because dropping a store the
        // bridge makes every frame is a silent divergence, and the field is one byte.
        bool mbPlayerCarCrashing;

        // [FLAG PC drive point] (no console member): latched once BrnGui::WorldDataController::
        // Prepare reports done, so ResourceUpdateThread stops re-entering it. On the console
        // the equivalent latch is GuiModule::Prepare's own stage word reaching 15.
        bool mbWorldDataPrepared;
        // [event-starts wave 2026-08-27] The done latch for the controller's SECOND acquire
        // machine (WorldDataController::Prepare2 @0x82516CB8 -- mpProgressionData / mpStreetData).
        // Separate from mbWorldDataPrepared because the two machines complete independently and,
        // on this build, one of them never completes at all. See the ⛔ at the driver's call site
        // for why they are pumped mutually exclusively.
        bool mbWorldData2Prepared = false;

        // [FLAG PC stand-in] (no console member): the GUI events 406 + 412 have gone out for the
        // car-select screen currently on the flow. Cleared when that screen unsubscribes, so a
        // re-entry republishes. See PublishCarSelectionToGui.
        bool mbCarSelectionPublished;

        // [FLAG PC bring-up] (no console member): one GUI out-event 192
        // (GuiEventActivateCarSelect) seen by BridgeGuiToGame's channel-40 walk, held until the
        // sim spine's car-select leg can hand it to the extracted ProcessGameEvents case-94 arm
        // under the game-state module's own output-buffer lock. [0] = the action word,
        // [1] = the car-select type word -- in the console's own payload order.
        // DELETE-WHEN BridgeGuiToGameState has a caller and ProcessGameEvents drains a real
        // post-world input buffer.
        bool mbCarSelectActivatePending;
        s32  maiPendingCarSelectActivate[2];

        // The two per-frame flags BridgeGameStateToSound @0x823CDE50 ORs into the
        // sound UpdateInfo byte (X360 +10094119 / +10094120; writers un-decoded --
        // they stay false, publishing 0, the boot-state value). Named by offset
        // pending a richer caller signature (phase C3b).
        bool mbField10094119;
        bool mbField10094120;

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

    // ⚠️ FLAG PC quality-of-life -- no console counterpart. The simulation-pacing switch,
    // read from config.ini `[Display] DecoupleSimulation` by BrnMain's LoadConfig and applied
    // by BrnGameModule::Construct (the module is a static, so its constructor runs first and
    // cannot see the setting).
    //   true  -- fixed 60 Hz simulation, free-running renderer, interpolated display poses.
    //   false -- the console's arrangement: exactly one simulation step per rendered frame.
    // It exists so the two can be A/B'd in a single edit rather than a rebuild.
    extern bool gbDecoupleSimulationFromRenderRate;
}
