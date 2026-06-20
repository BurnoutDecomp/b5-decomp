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

// Per-frame IO-buffer payload types the game module pushes/pops on the update IO stacks.
// PLACEHOLDER (empty) definitions so the loading-screen build can allocate them; the real
// layouts are owned by each module's IO TU and replace these when reconstructed.
namespace CgsGui { namespace CgsGuiModuleIO { struct InputBuffer {}; struct OutputBuffer {}; }
                   namespace ViewIO         { struct InputBuffer {}; }
                   namespace ModelIO        { struct OutputBuffer {}; } }
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
namespace BrnResource  { class GameDataModule : public CgsModule::ModuleSingleBuffered {}; }
namespace BrnGameState { class GameStateModule : public CgsModule::ModuleSingleBuffered {}; }
namespace BrnDirector  { class DirectorModule  : public CgsModule::ModuleSingleBuffered {}; }
namespace CgsInput     { class InputModule     : public CgsModule::ModuleSingleBuffered {}; }
// BrnGui::GuiModule is now the REAL module (hosts the MovieManager) -- included above (BrnGuiModule.h),
// no longer the opaque stub.
namespace BrnEffects   { class EffectsModule   : public CgsModule::ModuleSingleBuffered {}; }
namespace BrnSound { namespace Module { class RootSoundModule : public CgsModule::ModuleSingleBuffered {}; } }
namespace BrnReplays   { class ReplayModule    : public CgsModule::ModuleSingleBuffered {}; }
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
    };

    // operator++(EReleaseStage&, int) @ 0x823A8AD8 (X360 ARTIST), defined in BrnGameModule.cpp.
    // POST-increment for the BrnGameModule::EReleaseStage stage machine (the trailing dummy int is
    // the post-fix marker): saves the old value, advances the stage, asserts the advanced value has
    // not run past E_RELEASESTAGE_DONE (=7), and returns the OLD value. Called by
    // BrnGame::BrnGameModule::Release to step meReleaseStage. Declared here near the enum (the enum
    // is BrnGameModule::EReleaseStage, public) so all Release-path users of the operator see it.
    BrnGameModule::EReleaseStage operator++(BrnGameModule::EReleaseStage& leStage, int);
}
