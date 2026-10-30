#include <xtl.h>
#include "GameShared/GameClasses/Development/PerfMon/Gpu/CgsPerfMonGpu.h"
#ifdef AddMonitor
#undef AddMonitor
#endif
#include "SharedClasses/DataLists/VehicleListEntry.h"
#include "GameSource/Gui/Events/BrnGuiPFXEvents.h"                     // BrnGui::GuiPFXHookEnumeration (case 501 size pin)
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/Game/BrnGameModule.hpp"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "GameSource/Input/GameInputActions.h" // E_GAMEINPUTACTIONS_START (X360 action slot 8)
#include "GameSource/Resource/BrnResourceAllocator.h"
#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/System/CgsHardwareInit.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CgsDev::Assert
#include "SDKs/EA/GameTalk/GameTalk.h"               // EA::GameTalk::GameTalkMessage (RenderMetricsMessageHandler)
#include "GameSource/Gui/BrnGuiPerfmons.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"      // BrnGui::GuiAudioTriggerEvent (GUI-out event 201) + GuiEventProgressionProfileData (350)
#include "GameSource/Game/GameBridgeGameStateToX.h"  // BrnGame::BridgeGameStateToGui_EventStatus (the GUI-leg status seam)
#include "GameSource/GameState/Progression/BrnProfile.h" // BrnProgression::Profile (the action-193 payload's first word)

#include <cstring>   // memset
#include <string.h>  // _stricmp (RenderMetricsMessageHandler; MSVC canonical, not declared by <cstring>)
#include <cstddef>   // offsetof -- the 545/546 payload layout pins in BridgeGuiToGame
#include "rw/math/vpu/matrix44affine_operation.h"

#include "GameShared/GameClasses/Module/CgsModuleIOHelper.h"       // [FX-RUMBLE3] CgsModule::IOHelper (DoUpdate_InputPreWorld's "InputPreWorld")
#include "GameShared/GameClasses/Gui/CgsGuiModule.h" // CgsGui::GuiModule::AddGuiEvent (the world-load report below)
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h" // CgsGui::GuiEventTimeInfo (the per-frame GUI timestep)
#include "GameSource/Game/BrnLoadingScreenRenderer.h" // BrnGame::ELoadingScreenCommand (BridgeGuiToGame's command slot)
#include "GameSource/Resource/BrnGameDataModuleIO.h" // GameDataIO::InputBuffer/OutputBuffer (GamePrepare's request bracket)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h" // CgsResource::Events::AcquireResourceResponse (GamePrepare's acquire drain)
#include "rw/rwcore_structs.h"                       // rw::ResourceAllocatorRegistry::GetDefaultAllocator (the debug-font texture state)
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h" // RegisterFunction (the Debug/Sim actions)
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/Graphics/BrnRendererModuleIO.h"    // RendererIO::InputBuffer/OutputBuffer (GamePrepare's renderer pair)
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"          // DirectorIO::InputBuffer (DoUpdate_Director)
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIOOutputBuffer.hpp" // DirectorIO::OutputBuffer::GetCameraOutput (DoUpdate_Sound)
#include "GameSource/Replays/BrnReplayModuleIO.h"                            // ReplayIO::OutputBuffer_PreSim::GetStatusInterface (DoUpdate_Sound)
#include "GameSource/Effects/SharedIO/BrnEffectsModuleIO_OutputBuffer.h"     // EffectsIO::OutputBuffer (DoUpdate_Sound lock-only participant)
#include "GameSource/World/BrnWorldModuleIO_DispatchInputBuffer.h"
#include "GameSource/World/BrnWorldModuleIO_DispatchOutputBuffer.h"
#include "GameSource/Effects/SharedIO/BrnEffectsModuleIO_DispatchInputBuffer.h"
#include "GameSource/Effects/SharedIO/BrnEffectsModuleIO_InputBuffer.h"
#include "GameSource/Sound/Module/SharedIO/BrnPreUpdateSharedIo.h"   // AudioEffectsMessageQueue (the empty-queue construct in DoUpdate_Effects)      // EffectsIO::InputBuffer (DoUpdate_Effects / BridgeEntityToEffects)
#include "GameSource/GameState/BrnGameStateModuleIO.h" // GameStateModuleIO::OutputBuffer (BridgeGameStateToDirector)
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIOSceneQuery.h" // DirectorIO::SceneQuery{Input,Output}Buffer
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"   // SceneManagerIO::InputBuffer_Query / OutputBuffer (the external query leg)
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"            // CgsModule::LockBuffersForIO / UnlockBuffersForIO
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"       // [DIAG BRN_CAMCOLLIDE_DIAG] Camera::VehicleInfo
#include "vendor/renderware/collision/CollisionVolume.hpp"          // [DIAG BRN_CAMCOLLIDE_DIAG] rw::collision::SphereVolume
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface + BrnPhysics::Vehicle::RaceCarState (DoDispatch's TempRaceCarStateCache seam)
// The native timer API returns the complete 64-bit time-base value.
#include "GameShared/GameClasses/System/Timer/CgsTimeUtils.h"

namespace BrnGame
{
    // File-scope globals the X360 GameMain references:
    //   byte_82FAEB90  - the debug force-assert toggle (_lbForceAssert); fires a guarded
    //                    assert when set so a build can be forced to break into the assert path.
    //   dword_83084890 - a gate cleared while the simulation sub-steps run and set once they
    //                    complete; the dispatch/render side reads it. File-scope here, promoted
    //                    to a shared global when its other readers are reconstructed.
    static bool         _lbForceAssert = false;
    static volatile s32 sbSimUpdateComplete = 0;
    bool                sbShowStreamStallMessage = false; // byte_82FAE28A
    // Mutable UI latch from ARTIST byte_82FAE28C. Its original symbol and startup writer are
    // unresolved; findinit finds only the BridgeGuiToGame read/clear sites.
    static bool         sbForceQuitToStartMenu;
    // ARTIST 0x823BCF40 passes the exact 28-row console table at 0x82CDBEB8 to
    // PostMappingRequest(port=-1). These bytes match the recovered rows in CgsInputPadsPC.cpp;
    // keep them typed as the ActionMapping records consumed by the real IO.
    static const CgsInput::InputIO::ActionMapping KA_CONSOLE_DEFAULT_GAME_INPUT_MAPPING[28] =
    {
        { { 37, 41, 58, 14 } }, { { 38, 42, 60, 15 } },
        { { 39, 43, 59, 16 } }, { { 40, 44, -1, 17 } },
        { { 8, 45, -1, -1 } }, { { 50, 46, -1, 36 } },
        { { 13, 47, -1, 34 } }, { { 12, 48, -1, 35 } },
        { { 3, 49, -1, 19 } }, { { 11, 53, 50, 21 } },
        { { 2, 51, -1, 20 } }, { { 5, 52, -1, 18 } },
        { { 6, 54, -1, 24 } }, { { 7, 55, -1, 25 } },
        { { 1, 56, -1, 22 } }, { { 0, 57, -1, 23 } },
        { { -1, 41, -1, 26 } }, { { -1, 42, -1, 27 } },
        { { -1, 43, -1, 28 } }, { { -1, 44, -1, 29 } },
        { { -1, -1, -1, 30 } }, { { -1, -1, -1, 31 } },
        { { -1, -1, -1, 32 } }, { { -1, -1, -1, 33 } },
        { { 0, 41, -1, 30 } }, { { 1, 42, -1, 31 } },
        { { -1, 43, -1, 32 } }, { { -1, 44, -1, 33 } }
    };
    static bool sbInitialInputMappingPosted = false; // ARTIST byte_82FAE28B, BSS-zeroed.
    static char sacMapFileName[512]; // ARTIST82FAE990.
    // The three native data homes below are only named by address in ARTIST.
    // These exact initialization/store sites have no other exported references.
    s32 siNativeValue82CDBEB4 = -1;
    s32 siNativeValue82CDBF28 = -1;
    u8 suNativeValue82FAE280;
    static bool sbDiskErrorThreadStarted; // ARTIST byte_82FAE988, set once after Begin.

    // ARTIST827E5F00: only the actual member constructors run here. Construct()
    // performs the explicit game-state initialization after native allocation.
    BrnGameModule::BrnGameModule()
    {
    }

    // ARTIST823DB848: resumable native startup, consuming main's five stacks.
    // Full object/member ABI remains guarded by the entry/layout checks.
    bool BrnGameModule::Prepare(CgsModule::IOBufferStack* lpUpdateInput,
                                CgsModule::IOBufferStack* lpUpdateOutput,
                                CgsModule::IOBufferStack* lpResourceInput,
                                CgsModule::IOBufferStack* lpResourceOutput,
                                CgsModule::IOBufferStack* lpDispatch)
    {
        switch (mePrepareStage)
        {
        case E_PREPARESTAGE_START:
            mbHasGameTerminated = false;
        case E_PREPARESTAGE_HARDWARE:
            mePrepareStage = E_PREPARESTAGE_HARDWARE;
            if (!mHardware.Prepare())
                return false;
            mAutoTestManager.Init(this);
        case E_PREPARESTAGE_MANAGER:
            mePrepareStage = E_PREPARESTAGE_MANAGER;
            if (!CgsModule::ModuleSingleBuffered::Prepare())
                break;
            mpResourceInputBufferStack = lpResourceInput;
            mpUpdateInputBufferStack = lpUpdateInput;
            mpUpdateOutputBufferStack = lpUpdateOutput;
            mpResourceOutputBufferStack = lpResourceOutput;
            mpResourceInputBufferStack->CreateIOBuffer(&mpGameDataModuleInputBuffer, "GameData");
            mpResourceOutputBufferStack->CreateIOBuffer(&mpGameDataModuleOutputBuffer, "GameData");
            mDispatchThreadInputBufferManager.Construct(lpDispatch);
        case E_PREPARESTAGE_GAMEDATAMODULE:
            mePrepareStage = E_PREPARESTAGE_GAMEDATAMODULE;
            if (!mGameDataModule.Prepare(mpResourceInputBufferStack, mpResourceOutputBufferStack,
                    mpGameDataModuleInputBuffer, mpGameDataModuleOutputBuffer))
                break;
        case E_PREPARESTAGE_RENDERMODULE:
            {
                mePrepareStage = E_PREPARESTAGE_RENDERMODULE;
                const BrnResource::GameDataIO::OutputBuffer* const lpOutput = mpGameDataModuleOutputBuffer;
                lpOutput->LockForRead();
                const bool lbPrepared = mRenderModule.Prepare(lpOutput->GetAllocatorList());
                lpOutput->UnlockForRead();
                if (!lbPrepared)
                    break;
                D3DDevice_ReleaseThreadOwnership(gpD3DDevice);
            }
        case E_PREPARESTAGE_HARDWAREMEMORY:
            {
                mePrepareStage = E_PREPARESTAGE_HARDWAREMEMORY;
                const BrnResource::GameDataIO::OutputBuffer* const lpOutput = mpGameDataModuleOutputBuffer;
                lpOutput->LockForRead();
                const bool lbPrepared = mHardware.PrepareMassiveMemory(lpOutput->GetAllocatorList()) != 0;
                lpOutput->UnlockForRead();
                if (!lbPrepared)
                    break;
            }
        case E_PREPARESTAGE_THREADS:
            mePrepareStage = E_PREPARESTAGE_THREADS;
            mThreadLayout.Begin(this, mCpuMonitors.miUT_Resource,
                mCpuMonitors.miUT_ThreadSync, mCpuMonitors.miUT_WaitOnDispatch);
            mThreadLayout.SetFrameRate(CgsSystem::E_FRAMERATE_60HZ);
            mThreadLayout.SetResourcePercentageOfFrame(50);
            mDispatchThreadInputBufferManager.GetWriteBuffer()->ShowLoadingScreen();
            if (!mFrameRateManager.Prepare())
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("mFrameRateManager.Prepare()",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnGameModule.cpp", 788);
                CgsDev::Assert::EndAssert();
            }
        case E_PREPARESTAGE_DONE:
            EA::GameTalk::GameTalkManager::RegisterMessageHandler(
                EA::GameTalk::GameTalkManager::GetInstance(),
                reinterpret_cast<void (*)(EA::GameTalk::GameTalkMessage*, void*)>(RenderMetricsMessageHandler),
                "Camera", this);
            mePrepareStage = E_PREPARESTAGE_START;
            meReleaseStage = E_RELEASESTAGE_START;
            mbIsSysMenuShowing = false;
            return true;
        default:
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("0",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnGameModule.cpp", 808);
            CgsDev::Assert::EndAssert();
            break;
        }
        if (static_cast<s32>(mePrepareStage) >= E_PREPARESTAGE_GAMEDATAMODULE)
            CheckDiskError(mpGameDataModuleOutputBuffer);
        return false;
    }

    bool BrnGameModule::HasGameTerminated()
    {
        return mbHasGameTerminated;
    }

    // ARTIST823BC798. Hold the actual resource output read lock through the
    // one-shot dirty-disc worker creation, including its original user selection.
    void BrnGameModule::CheckDiskError(const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput)
    {
        lpGameDataOutput->LockForRead();
        if (lpGameDataOutput->GetFileSystemStatusInterface()->GetDiskErrorOccured())
        {
            const u32 luUserIndex = miInputModuleState ? static_cast<u32>(miPlayer0ControllerPort) : 0;
            if (!sbDiskErrorThreadStarted)
            {
                EA::Thread::ThreadParameters lParameters;
                lParameters.mbSuspended = false;
                lParameters.mnPriority = 0;
                lParameters.mnProcessor = 2;
                lParameters.mpName = "DispatchThread";
                mErrorThread.Begin(reinterpret_cast<EA::Thread::RunnableFunction>(DiskErrorThreadProc),
                    reinterpret_cast<void*>(static_cast<uintptr_t>(luUserIndex)), &lParameters,
                    EA::Thread::Thread::GetGlobalRunnableFunctionUserWrapper());
                sbDiskErrorThreadStarted = true;
            }
        }
        lpGameDataOutput->UnlockForRead();
    }

    BrnGameModule::~BrnGameModule()
    {
    }

    // @ BrnGameModule.cpp:155 (X360 0x823C9EA8) - construct the game module: base, debug
    // systems, the CPU monitor registrations, then EVERY engine module (X360 order), then the
    // flow state machine. Sites the X360 body has that are still gated on unreconstructed
    // members/subsystems are flagged inline.
    // ARTIST823BD4E0: the75 constant footprint records emitted by that binary.
    // These original constants do not certify the current C++ class layouts;
    // Main and DebugMemoryInit retain their strict native size/offset gates.
    void BrnGameModule::CheckClassSizes()
    {
        struct SizeRecord { const char* mpcIndent; const char* mpcType; u32 muSize; };
        static const SizeRecord kaRecords[] =
        {
            {"", "BrnGame::BrnGameModule", 10097408u},
            {"  ", "WorldModule", 6175872u},
            {"    ", "BrnWorld::WorldEntityModule", 31216u},
            {"      ", "BrnWorld::PVSModule", 8128u},
            {"    ", "BrnTraffic::TrafficEntityModule", 497664u},
            {"    ", "BrnWorld::RaceCarEntityModule", 100352u},
            {"      ", "BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface", 10480u},
            {"      ", "BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface", 2416u},
            {"      ", "BrnWorld::ActiveRaceCar::RenderParams", 5280u},
            {"    ", "BrnWorld::PropEntityModule", 865280u},
            {"    ", "BrnWorld::TriggerEntityModule", 66208u},
            {"    ", "BrnWorld::CrashModule", 5800u},
            {"    ", "BrnAI::AIModule", 322848u},
            {"      ", "BrnAI::RouteMapModule", 26144u},
            {"      ", "BrnAI::AStar", 25464u},
            {"    ", "BrnPhysics::PhysicsModule", 433216u},
            {"      ", "CgsPhysics::PhysicsSimulationModule", 18544u},
            {"      ", "BrnPhysics::Vehicle::VehicleManager", 172624u},
            {"      ", "BrnPhysics::MappingManager", 524296u},
            {"      ", "BrnPhysics::Deformation::DeformationManager", 76752u},
            {"        ", "BrnPhysics::Deformation::DeformableObject", 26496u},
            {"        ", "BrnPhysics::Deformation::DeformationInputInterface", 5072u},
            {"        ", "BrnPhysics::Deformation::DeformationOutputInterface", 10992u},
            {"      ", "BrnPhysics::Props::PropManager", 25984u},
            {"    ", "CgsSceneManager::SceneManagerModule", 3835264u},
            {"      ", "CgsSceneManager::SpatialPartitionManager", 24u},
            {"      ", "CgsSceneManager::OverlapGenerationModule", 902928u},
            {"      ", "CgsSceneManager::OverlapCullingModule", 443040u},
            {"      ", "CgsSceneManager::FineIntersectionTestModule", 366624u},
            {"      ", "CgsSceneManager::EntityManager", 1201024u},
            {"      ", "CgsSceneManager::VolumeManager", 915104u},
            {"        ", "CgsSceneManager::LooseOctree", 592768u},
            {"        ", "CgsSceneManager::LooseOctreeNode", 96u},
            {"        ", "CgsSceneManager::SpatialPartitionEntity", 4u},
            {"  ", "BrnRendererModule", 51840u},
            {"  ", "BrnResource::GameDataModule", 477696u},
            {"    ", "BrnResource::ChallengeListEntry", 216u},
            {"    ", "CgsResource::ResourceModule", 361088u},
            {"      ", "CgsResource::PoolModule", 107008u},
            {"        ", "CgsResource::Pool", 464u},
            {"        ", "CgsResource::Entry", 88u},
            {"        ", "CgsResource::BaseResourcePtr", 32u},
            {"      ", "CgsResource::BundleLoaderModule", 180480u},
            {"    ", "CgsAttribSys::AttribSysModule", 624u},
            {"  ", "BrnGameState::GameStateModule", 292368u},
            {"    ", "BrnGameState::TakedownManager", 712u},
            {"    ", "BrnGameState::MugshotManager", 112u},
            {"    ", "BrnGameState::PaybackManager", 640u},
            {"    ", "BrnGameState::GameStateInviteManager", 2096u},
            {"    ", "BrnGameState::ModeManager", 38192u},
            {"    ", "BrnGameState::FlybyManager", 672u},
            {"    ", "BrnGameState::TriggerQueryManager", 1920u},
            {"    ", "BrnGameState::DriveThruManager", 2400u},
            {"    ", "BrnGameState::NetworkRoundManager", 312u},
            {"    ", "BrnProgression::ProgressionManager", 133536u},
            {"  ", "CgsInput::InputModule", 5096u},
            {"  ", "BrnGui::GuiModule", 1629392u},
            {"    ", "CgsGui::ModelModule", 111064u},
            {"    ", "CgsGui::ViewModule", 167088u},
            {"    ", "CgsGui::GuiEventQueue", 18448u},
            {"    ", "BrnFlapt::FlaptManager", 2256u},
            {"    ", "BrnGui::CustomRendererManager", 128208u},
            {"    ", "BrnGui::GuiTracker", 413936u},
            {"    ", "BrnGui::BrnBaseFlow", 66124u},
            {"    ", "BrnGui::BrnHudFlow", 66180u},
            {"    ", "BrnGui::BrnOverlayFlow", 66184u},
            {"    ", "BrnGui::BrnScreenFlow", 66368u},
            {"  ", "CgsDev::DebugManager", 33176u},
            {"      ", "CgsSceneManager::SceneManagerDebugComponent", 96u},
            {"  ", "BrnEffects::EffectsModule", 194048u},
            {"    ", "BrnParticle::ParticleModule", 178176u},
            {"  ", "BrnSound::Module::RootSoundModule", 87552u},
            {"    ", "CgsSound::Playback::Module::Module", 10000u},
            {"    ", "BrnSound::Module::SoundLogicModule", 79488u},
            {"  ", "BrnNetwork::BrnNetworkModule", 866560u},
        };
        u32 lxFilters = CgsDev::Message::gxMessageFilterFlags;
        for (u32 luIndex = 0; luIndex < sizeof(kaRecords) / sizeof(kaRecords[0]); ++luIndex)
        {
            if ((lxFilters & 1u) != 0)
            {
                CgsDev::StrStreamBase& lStream = *CgsDev::Log::gpDebugPrint;
                lStream << "Size of \t";
                lStream << kaRecords[luIndex].mpcIndent;
                lStream << kaRecords[luIndex].mpcType;
                lStream << "\t\t";
                lStream << kaRecords[luIndex].muSize << "\n";
                lxFilters = CgsDev::Message::gxMessageFilterFlags;
            }
        }
    }

    void BrnGameModule::Construct()
    {
        CgsModule::ModuleSingleBuffered::Construct();
        CheckClassSizes();
        mePrepareStage = E_PREPARESTAGE_START;
        meReleaseStage = E_RELEASESTAGE_DONE;
        meGamePrepareStage = E_GAMEPREPARESTAGE_START;
        meGameReleaseStage = E_GAMERELEASESTAGE_DONE;
        meGameUpdateStage = E_GAMEUPDATESTAGE_PREPARE;
        mHardware.mePrepareStage = BrnHW::System360HW::E_PREPARESTAGE_START;
        mHardware.meReleaseStage = BrnHW::System360HW::E_PREPARESTAGE_DONE;
        mHardware.mbEngineModulesLoaded = false;
        CgsDev::DebugManagerConstructParameters lParams = CgsDev::DebugManagerConstructParameters::DEFAULT;
        lParams.miPerfMonCpuCount = 768;
        lParams.miPerfMonLogBufferSize = 0;
        lParams.miFunctionPoolSize = 120;
        lParams.miVariablePoolSize = 400;
        lParams.miVariableMetadataPoolSize = 500;
        lParams.miConsoleLineCount = 15;
        lParams.mpRwAllocator = BrnResource::GetDebugAllocator();
        mDebugManager.Construct(&lParams);
        {
            CgsDev::DebugInterface lDebug;
            lDebug.RegisterVariable(&sbForceQuitToStartMenu, "", "Force Quit to Start Menu");
        }
        CgsDev::Assert::gAssertManager.SetThreadLayout(&mThreadLayout);
        CgsCore::SPrintf(sacMapFileName, sizeof(sacMapFileName), "%sMAP_ARTIST.BIN",
            CgsSystem::HardwareInit::GetFOPENDirectory());
        if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
            *CgsDev::Log::gpDebugPrint << "File to open " << sacMapFileName << "\n";
        mMapFileReader.SetAsyncronousMode(false);
        CgsDev::Assert::gAssertManager.SetMapFileReader(&mMapFileReader, sacMapFileName);
        mCpuMonitors.Construct();
        {
            typedef CgsDev::PerfMonCpuPage EPage;
            mCpuMonitors.miUT_TotalUpdate      = CgsDev::PerfMonCpu::AddMonitor("UT: Total simulation",          (EPage)0,  false, 210.0f, false);
            mCpuMonitors.miUT_EachUpdate       = CgsDev::PerfMonCpu::AddMonitor("UT: Each sim step",             (EPage)0,  false,  70.0f, true);
            mCpuMonitors.miUT_NetworkAIRaceCar = CgsDev::PerfMonCpu::AddMonitor("      Network + AI + Racecar",  (EPage)0,  false,  10.0f, true);
            mCpuMonitors.miUT_Network          = CgsDev::PerfMonCpu::AddMonitor("         Network",              (EPage)0,  false,   3.0f, true);
            mCpuMonitors.miUT_AI               = CgsDev::PerfMonCpu::AddMonitor("         AI",                   (EPage)0,  false,   5.0f, true);
            mCpuMonitors.miUT_RaceCar          = CgsDev::PerfMonCpu::AddMonitor("         RaceCar",              (EPage)0,  false,   3.0f, true);
            mCpuMonitors.miUT_GameState        = CgsDev::PerfMonCpu::AddMonitor("      GameState",               (EPage)0,  false,   3.0f, true);
            mCpuMonitors.miUT_Replay           = CgsDev::PerfMonCpu::AddMonitor("      Replay",                  (EPage)0,  false,   3.0f, true);
            mCpuMonitors.miUT_GUI              = CgsDev::PerfMonCpu::AddMonitor("      GUI",                     (EPage)0,  false,   2.0f, true);
            mCpuMonitors.miUT_Director         = CgsDev::PerfMonCpu::AddMonitor("      Director",                (EPage)0,  false,   5.0f, true);
            mCpuMonitors.miUT_Sound            = CgsDev::PerfMonCpu::AddMonitor("      Sound",                   (EPage)0,  false,   5.0f, true);
            mCpuMonitors.miUT_Effects          = CgsDev::PerfMonCpu::AddMonitor("      Effects",                 (EPage)0,  false,   0.5f, true);
            mCpuMonitors.miUT_Traffic          = CgsDev::PerfMonCpu::AddMonitor("      Traffic",                 (EPage)0,  false,   8.0f, true);
            mCpuMonitors.miUT_Triggers         = CgsDev::PerfMonCpu::AddMonitor("      Triggers",                (EPage)0,  false,   0.5f, true);
            mCpuMonitors.miUT_CrashManager     = CgsDev::PerfMonCpu::AddMonitor("      CrashManager",            (EPage)0,  false,   1.0f, true);
            mCpuMonitors.miUT_Physics          = CgsDev::PerfMonCpu::AddMonitor("      Physics",                 (EPage)0,  false,  28.0f, true);
            mCpuMonitors.miUT_World            = CgsDev::PerfMonCpu::AddMonitor("      World",                   (EPage)0,  false,   4.0f, true);
            mCpuMonitors.miUT_Resource         = CgsDev::PerfMonCpu::AddMonitor("UT: ResourceSystem",            (EPage)0,  false,   5.0f, false);
            mCpuMonitors.miUT_RenderAll        = CgsDev::PerfMonCpu::AddMonitor("UT: Render",                    (EPage)0,  false,  15.0f, false);
            mCpuMonitors.miUT_FrustumTesting   = CgsDev::PerfMonCpu::AddMonitor("      FrustumTests",            (EPage)0,  false,   2.0f, false);
            mCpuMonitors.miUT_RenderMainScreen = CgsDev::PerfMonCpu::AddMonitor("      RenderMainScreen",        (EPage)0,  false,   7.0f, false);
            mCpuMonitors.miUT_RenderShadowMap  = CgsDev::PerfMonCpu::AddMonitor("      RenderShadows",           (EPage)0,  false,   1.5f, false);
            mCpuMonitors.miUT_RenderEnvMap     = CgsDev::PerfMonCpu::AddMonitor("      RenderEnvMap",            (EPage)0,  false,   1.5f, false);
            mCpuMonitors.miUT_RenderFX         = CgsDev::PerfMonCpu::AddMonitor("      RenderEffects",           (EPage)0,  false,   1.0f, false);
            mCpuMonitors.miUT_RenderGUI        = CgsDev::PerfMonCpu::AddMonitor("      RenderGUI",               (EPage)0,  false,   2.0f, false);
            mCpuMonitors.miDT_DispatchToGpu    = CgsDev::PerfMonCpu::AddMonitor("DT: DispatchToGPU",             (EPage)0,  false,  95.0f, false);
            mCpuMonitors.miUT_ThreadSync       = CgsDev::PerfMonCpu::AddMonitor("UT: Thread sync",               (EPage)0,  false,  10.0f, false);
            mCpuMonitors.miUT_WaitOnDispatch   = CgsDev::PerfMonCpu::AddMonitor("      WaitOnDispatch",          (EPage)0,  false,  10.0f, false);
            mCpuMonitors.miUT_DebugManager     = CgsDev::PerfMonCpu::AddMonitor("UT: Debug manager",             (EPage)0,  false,   0.0f, false);
            mCpuMonitors.miUT_RaceCar_SQ       = CgsDev::PerfMonCpu::AddMonitor("RC SceneQueries",               (EPage)12, false,   0.5f, true);
            mCpuMonitors.miUT_Traffic_SQ       = CgsDev::PerfMonCpu::AddMonitor("Traf SceneQueries",             (EPage)2,  false,   0.5f, true);
            mCpuMonitors.miUT_Triggers_SQ      = CgsDev::PerfMonCpu::AddMonitor("Trigger SceneQueries",          (EPage)5,  false,   0.5f, true);
            mCpuMonitors.miUT_GameState_Bridge = CgsDev::PerfMonCpu::AddMonitor("GameState Bridges",             (EPage)11, false,   0.1f, true);
            mCpuMonitors.miUT_GUI_Bridge       = CgsDev::PerfMonCpu::AddMonitor("GUI Bridges",                   (EPage)11, false,   0.1f, true);
            mCpuMonitors.miUT_AI_Bridge        = CgsDev::PerfMonCpu::AddMonitor("AI Bridges",                    (EPage)11, false,   0.1f, true);
            mCpuMonitors.miUT_RaceCar_Bridge   = CgsDev::PerfMonCpu::AddMonitor("RaceCar Bridges",               (EPage)11, false,   0.1f, true);
            mCpuMonitors.miUT_Traffic_Bridge   = CgsDev::PerfMonCpu::AddMonitor("Traffic Bridges",               (EPage)11, false,   0.1f, true);
            mCpuMonitors.miUT_Director_Bridge  = CgsDev::PerfMonCpu::AddMonitor("Bridges",                       (EPage)13, false,   0.1f, true);
            mCpuMonitors.miUT_Director_SQ      = CgsDev::PerfMonCpu::AddMonitor("Scene Queries",                 (EPage)13, false,   1.0f, true);
            mCpuMonitors.miUT_SoundUpdate      = CgsDev::PerfMonCpu::AddMonitor("Sound",                         (EPage)14, false,  10.0f, true);
        }

        mSoundModule.Construct();
        f32 lfAspectRatio;
        bool lbHighDef;
        mRenderModule.Construct(mCpuMonitors, lfAspectRatio, lbHighDef);
        mGameDataModule.Construct();
        mWorldModule.Construct(mCpuMonitors);
        mInputModule.Construct();
        mGuiModule.Construct(mGameDataModule.GetHudMessageController(),
            mGameDataModule.GetPopupController(), lfAspectRatio, lbHighDef);
        mGameStateModule.Construct();
        mEffectsModule.Construct();
        mDirectorModule.Construct(lfAspectRatio);
        mReplayModule.Construct();
        siNativeValue82CDBEB4 = 0;
        siNativeValue82CDBF28 = 0;
        mNetworkModule.Construct(0);
        mDebugFont.mpResourceMemory = 0;
        mDebugFont.mpSourceEntry = 0;
        miPlayer0ControllerPort = CgsInput::KU_NUMBER_OF_PADS;
        miSecondaryControllerPort = CgsInput::KU_NUMBER_OF_PADS;
        mbEnableCalibrationUnfriendlyPostFx = true;
        miContrast = 50;
        miBrightness = 50;
        mbSimPaused = false;
        mbControllerDisconnected = false;
        miInputModuleState = E_INPUTBINDSTATE_UNBOUND;
        mbRequestStreamingWait = false;
        mbWorldStreamingRequestedStall = false;
        mbStreamingStalled = false;
        mbPlayerCarCrashing = false;
        mbIsSysMenuShowing = false;
        mbTakeScreenShots = false;
        mbGameOver = false;
        mbOnline = false;
        mbGuiPreAccept = false;
        miGuiFsmStage = E_GUIFLOW_MAX;
        const s32 liRefreshRate = BrnRendererModule::GetVSynchRate();
        static const char* const kapcPageNames[24] =
        {
            "General", "Resource", "Traffic", "Gui", "World", "GameState", "Physics", "AI",
            "Network", "Network Detail", "Graphics", "Bridges", "Race Car", "Director", "Sound",
            "Deformation", "SceneManager ContactGen", "SceneManager", "More Network Detail",
            "Sound: Aems Detail", "Particles", "Lion", "Video", "Flapt"
        };
        for (s32 liPage = 0; liPage < 24; ++liPage)
            mDebugManager.mDebugComponentPerfMonCpu.SetPageName(
                static_cast<CgsDev::PerfMonCpuPage>(liPage), kapcPageNames[liPage]);
        if (liRefreshRate == 50)
            CgsDev::PerfMonCpu::SetGameFrequency(CgsDev::E_PMF_50HZ);
        else if (liRefreshRate == 60)
            CgsDev::PerfMonCpu::SetGameFrequency(CgsDev::E_PMF_60HZ);
        else
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lMessage(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lMessage << "Refresh rate " << liRefreshRate
                << " not handled by M_CGS_PERFMON_CPU_SETGAMEFREQUENCY\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnGameModule.cpp", 498);
            CgsDev::Assert::EndAssert();
        }
        mGamePrepareReceiverQueue.Construct();
        mMainFlowStateMachine.Construct();
        muCatchupStartTime = 0;
        mTimingValue12C0 = 0;
        mTimingValue12C8 = 0;
        mTimerStatusInterface.Clear();
        const f32 lfRate = 1.0f / static_cast<f32>(liRefreshRate);
        mGameTimer.Prepare(lfRate);
        mSimTimer.Prepare(lfRate);
        mGameTimer.SetRunning(true);
        mSimTimer.SetRunning(true);
        mFrameRateManager.Construct(mSimTimer.GetScaleCurrent() * mSimTimer.GetRate() * 1000.0f,
            mSimTimer.GetScaleCurrent() * mSimTimer.GetRate() * 100.0f);
        miNumSimFramesRequired = 1;
        mi8FrameRateMinSteps = 1;
        mi8FrameRateMaxSteps = 3;
        mi8ActualFrameRateMinStepsThisFrame = 1;
        mi8ActualFrameRateMaxStepsThisFrame = 1;
        meFrameRateManagerType = CgsSystem::E_FRAMERATEMANAGER_MULTIPLE_CAPPED;
        {
            static const CgsDev::DebugUI::StringList kaFrameRateOptions[] =
            {
                {0, "Single"}, {1, "Multiple Capped"}, {2, "Multiple Uncapped"}
            };
            CgsDev::DebugInterface lDebug;
            s32* const lpType = reinterpret_cast<s32*>(&meFrameRateManagerType);
            lDebug.RegisterVariable(lpType, "Debug/Framerate", "Framerate type");
            lDebug.SetRange(lpType, 0, 2);
            lDebug.SetOptions(lpType, kaFrameRateOptions);
            lDebug.RegisterFunction(&BrnGameModule::StepFrameCB, this, "Debug/Sim", "Step");
            lDebug.RegisterFunction(&BrnGameModule::PlayFrameCB, this, "Debug/Sim", "Play");
            lDebug.RegisterVariable(&mbForceShutdown, "Debug/Sim", "Force shutdown");
            lDebug.RegisterVariable(&mbTakeScreenShots, "Core/Debug/Performance/CPU Monitors", "Take screen shots");
        }
        mFrameRateTypeRequestInterface.Clear();
        mfDebugFpsAverage = 0.0f;
        mfDebugFpsAccumulator = 0.0f;
        mfDebugFpsSampleCount = 0.0f;
        mbSteppingFrames = false;
        mbDoStep = false;
        mbStopStepping = false;
        mbTakeScreenShots = false;
        mbForceShutdown = false;
        mau64DebugFpsFrameStamps[0] = 0;
        mau64DebugFpsFrameStamps[1] = 0;
        mau64DebugFpsFrameStamps[2] = 0;
        mu8DebugFpsFrameStampIndex = 0;
        mu64DebugFpsAverageWindowStamp = 0;
        mpUpdateInputBufferStack = 0;
        mpUpdateOutputBufferStack = 0;
        mpResourceInputBufferStack = 0;
        mpResourceOutputBufferStack = 0;
        mpGuiInputBuffer = 0;
        mpGuiViewInputBuffer = 0;
        mpGuiModelOutputBuffer = 0;
        mpGuiOutputBuffer = 0;
        mpDirectorOutputBuffer = 0;
        mbPrevStalled = false;
        mbStalled = false;
        mbRequestDoStepFrame = false;
        mbRequestDoPlayFrame = false;
        mbTakeMetrics = false;
        mbSendMetrics = false;
        muRenderMetricsCount = 0;
        mDirectorBridgeSerialiser.Construct();
        mGameModuleSerialiser.Construct();
        mbHasGameTerminated = false;
        suNativeValue82FAE280 = 0;
        miWorldModuleUpdateTrace = 0;
        mbIsLoadingScreenVisible = false;
        mbDiskError = false;
        if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
            *CgsDev::Log::gpDebugPrint << "Hard disk available: "
                << (CgsSystem::HardwareInit::IsHardDiskAvailable() ? "true" : "false") << "\n";
    }

    // @ 0x823DCA10 -- the game->GUI flow-FSM bridge (see BrnGameModule.hpp). Posts each
    // pending stage's GuiEventRunFsm record(s) as raw 24-byte event-144 AddEvents (the
    // X360 posts these raw, not through the AddGuiEvent<T> template), then parks the
    // stage and clears the phase-complete flag.
    void BrnGameModule::BridgeGameToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput)
    {
        BrnGui::GuiEventRunFsm lEvent;
        // ARTIST823DCA10 copies all24 bytes but never writes meFsmToRun at+16.
        const char* lpcFsmName = 0;
        switch (miGuiFsmStage)
        {
        case E_GUIFLOW_MARKETING: lpcFsmName = "BrnVideoFsm"; break;
        case E_GUIFLOW_START: lpcFsmName = "BrnLegalFsm"; break;
        case E_GUIFLOW_COMPLETELOADING: lpcFsmName = "BrnCmpLdFsm"; break;
        case E_GUIFLOW_MEMCARD: lpcFsmName = "BrnBFProFsm"; break;
        case E_GUIFLOW_INGAME:
            lEvent.mFsmId = CgsIDCompress("BrnScreenFsm");
            lEvent.mInitialStateId = CgsIDCompress("LOADING");
            lEvent.meFlowToUse = BrnGui::E_GUIFLOW_SCREEN;
            lpGuiInput->GetGuiEvents()->AddEvent(&lEvent, 144, sizeof(lEvent));
            lpcFsmName = "BrnFBFsm";
            break;
        }
        if (lpcFsmName)
        {
            lEvent.mInitialStateId = 0;
            lEvent.mFsmId = CgsIDCompress(lpcFsmName);
            lEvent.meFlowToUse = BrnGui::E_GUIFLOW_HUD;
            lpGuiInput->GetGuiEvents()->AddEvent(&lEvent, 144, sizeof(lEvent));
        }
        if (miGuiFsmStage != E_GUIFLOW_MAX)
        {
            miGuiFsmStage = E_GUIFLOW_MAX;
            mbGuiPhaseComplete = false;
        }
    }

    // ARTIST823CB758: original GUI event consumer and soft-reboot request.
    void BrnGameModule::BridgeGuiToGame(const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput)
    {
        CgsResource::ResourceHandle lCalibrationTexture = CgsResource::NULLResourceHandle;
        DispatchThreadInputBuffer* const lpDispatch = mDispatchThreadInputBufferManager.GetWriteBuffer();
        const CgsModule::VariableEventQueue<18432,16>* const lpQueue = lpGuiOutput->GetOutEventQueue();
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liType = lpQueue->GetFirstEvent(&lpEvent, &liSize);
        if (sbForceQuitToStartMenu)
        {
            sbForceQuitToStartMenu = false;
            mbGameOver = true;
        }
        while (lpEvent)
        {
            switch (liType)
            {
            case 19:
                lpDispatch->ShowLoadingScreen();
                mbIsLoadingScreenVisible = true;
                break;
            case 20:
                lpDispatch->HideLoadingScreen();
                mbIsLoadingScreenVisible = false;
                break;
            case 70:
                mbGuiPhaseComplete = true;
                break;
            case 71:
                mbGuiPreAccept = true;
                break;
            case 86:
            case 87:
            case 89:
                mbGameOver = true;
                break;
            case 90:
                if (miInputModuleState == E_INPUTBINDSTATE_UNBOUND)
                    miInputModuleState = E_INPUTBINDSTATE_PRESS_START;
                break;
            case 138:
                mDispatchThreadInputBufferManager.GetWriteBuffer()->ShowLoadingScreenSaveLoadBG();
                break;
            case 189:
                {
                    const BrnGui::GuiOverlayCompletePayload* const lpOverlay =
                        static_cast<const BrnGui::GuiOverlayCompletePayload*>(lpEvent);
                    if (CgsIDCompress("SignOut") == lpOverlay->mOverlayId &&
                        lpOverlay->meLeaveMethod == BrnGui::GuiOverlayCompleteEvent::E_LEAVEMETHOD_OK &&
                        miInputModuleState == E_INPUTBINDSTATE_BOUND)
                        miInputModuleState = E_INPUTBINDSTATE_GAME_OVER;
                }
                break;
            case 362:
                {
                    lpDispatch->LockForWrite();
                    SnapShotRequest* const lpRequest = lpDispatch->GetSnapShotRequest();
                    const BrnGui::GuiSnapShotRequested* const lpSnapshot =
                        static_cast<const BrnGui::GuiSnapShotRequested*>(lpEvent);
                    lpRequest->mfStartX = lpSnapshot->mfStartX;
                    lpRequest->mfStartY = lpSnapshot->mfStartY;
                    lpRequest->mfWidth = lpSnapshot->mfWidth;
                    lpRequest->mbTakeSnapShot = true;
                    lpRequest->mfHeight = lpSnapshot->mfHeight;
                    lpDispatch->UnlockForWrite();
                }
                break;
            case 545:
                {
                    const BrnGui::GuiOptionsBrightnessContrast* const lpBrightnessContrast =
                        static_cast<const BrnGui::GuiOptionsBrightnessContrast*>(lpEvent);
                    if (!lpBrightnessContrast)
                    {
                        CgsDev::Assert::BeginAssert();
                        CgsDev::Assert::FireAssert("NULL != lpBrightnessContrast",
                            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnGameModule.cpp", 3194);
                        CgsDev::Assert::EndAssert();
                    }
                    miBrightness = lpBrightnessContrast->mBrightness;
                    miContrast = lpBrightnessContrast->mContrast;
                }
                break;
            case 546:
                {
                    const BrnGui::GuiOptionsBrightnessContrastPostFxControl* const lpControl =
                        static_cast<const BrnGui::GuiOptionsBrightnessContrastPostFxControl*>(lpEvent);
                    if (!lpControl)
                    {
                        CgsDev::Assert::BeginAssert();
                        CgsDev::Assert::FireAssert("lpBrightnessContrastPostFxControl",
                            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnGameModule.cpp", 3205);
                        CgsDev::Assert::EndAssert();
                    }
                    mbEnableCalibrationUnfriendlyPostFx = lpControl->mbEnablePostFx;
                    lCalibrationTexture = lpControl->mColourCalibrationTextureHandle;
                }
                break;
            case 589:
                mDispatchThreadInputBufferManager.GetWriteBuffer()->BlackOverlayFadeIn();
                break;
            case 590:
                mDispatchThreadInputBufferManager.GetWriteBuffer()->BlackOverlayFadeOut();
                break;
            }
            liType = lpQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
        lpDispatch->LockForWrite();
        lpDispatch->SetBrightness(miBrightness);
        lpDispatch->SetContrast(miContrast);
        lpDispatch->SetCalibrationUnfriendlyEnablePostFx(mbEnableCalibrationUnfriendlyPostFx);
        lpDispatch->SetCalibrationTextureHandle(lCalibrationTexture);
        lpDispatch->UnlockForWrite();
        if (mbGameOver)
        {
            XGetLaunchData(&mHardware, 0x98);
            mHardware.mPad94 = 1;
            XSetLaunchData(&mHardware, 0x98);
            XLaunchNewImage(mHardware.macCommandLine, 0);
        }
    }

    // ------------------------------------------------------------------------------------
    // BridgeGuiToDirector  @ 0x823CBF70   -- ⭐ THE GUI -> DIRECTOR SEAM
    //
    // The console walks the GUI module's out-event queue and, for each recognised command,
    // raises the matching published flag on the DIRECTOR's input buffer. Every store is a
    // literal 1 (or a payload word) into the buffer's flag tail; MainDirector::PostGuiUpdate
    // @0x82236F88 reads them all back one pass later and folds them into the director's
    // GameState. Reconstructed arm-for-arm from the X360 switch; each `case` below carries the
    // console's own offset so the mapping is checkable.
    //
    // ⭐ 476 / 477 / 478 are the whole point of this function for the intro: BrnGui::Intro
    // posts 477 at START_FLYBY and 478 when its 7.67 s dwell expires, and until this bridge
    // existed NOTHING carried them anywhere -- the director never learned the front-end had
    // asked for a fly-by. Their meanings are NOT assumed: 477 -> +0x7ABE ->
    // GameState::mbGameIntroFlybyActive is pinned by ArbStateCarSelect::Update's own assert
    // string, and 476/478 fall either side of it in both the DWARF member order and the asm.
    //
    void BrnGameModule::BridgeGuiToDirector(BrnDirector::DirectorIO::InputBuffer* lpDirectorInput,
                                            const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput)
    {
        const CgsModule::VariableEventQueue<18432, 16>* const lpGuiOutQueue =
            lpGuiOutput->GetOutEventQueue();

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = lpGuiOutQueue->GetFirstEvent(&lpEvent, &liSize);
        while (liId >= 0 && lpEvent != 0)
        {
            const s32 liCommand = liId;
            const u32* const lpuPayload = reinterpret_cast<const u32*>(lpEvent);
            const s32 liPayload0 = static_cast<s32>(lpuPayload[0]);

            switch (liCommand)
            {
                case 77:    // +0x7ACC  the car-select ticker closed
                    lpDirectorInput->SetCarSelectTickerClosedThisFrame();
                    break;

                case 94:    // the shortcut menu opened/closed (payload word = the new state)
                    lpDirectorInput->SetShortcutMenuEvent(liPayload0 != 0);
                    break;

                case 191:   // crash-nav shown/hidden -- ignored entirely when payload[1] == 2
                    if (static_cast<s32>(lpuPayload[1]) != 2)
                    {
                        if (liPayload0 == 0)
                            lpDirectorInput->SetGotCrashNavShownEvent();
                        else if (liPayload0 == 1)
                            lpDirectorInput->SetGotCrashNavHiddenEvent();
                    }
                    break;

                case 192:   // +0x7AC2  end of car select (only for payload {4, 1})
                    if (liPayload0 == 4 && static_cast<s32>(lpuPayload[1]) == 1)
                        lpDirectorInput->SetEndOfCarSelect();
                    break;

                case 290:   // +0x7AD0  entered the online post-event flow
                    lpDirectorInput->SetEnteredOnlinePostEvent();
                    break;

                case 294:   // +0x7ACF  left the online post-event flow
                    lpDirectorInput->SetLeftOnlinePostEvent();
                    break;

                case 298:   // the console's own "over to you, Big E..." placeholder -- a debug
                            // print and nothing else. Kept because it is real console output.
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0
                        && CgsDev::Log::gpDebugPrint != 0)
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "Handle new rival camera presentation stuff here - over to you, Big E...\n";
                    }
                    break;

                case 303:   // +0x7ABC / +0x7AB4  rank up this frame + the new rank. The X360
                            // also runs the colour-calibration-shown store on this arm (the
                            // compiler merged the two tails); reproduced.
                    lpDirectorInput->SetRankUp(liPayload0);
                    lpDirectorInput->SetGotColourCalibrationShownEvent();
                    break;

                case 415:   // +0x7ACB  the car selection changed
                    lpDirectorInput->SetCarSelectionChangedThisFrame();
                    break;

                case 469:   // +0x7AD3 / +0x7AD4  the 100%-completion sequence start/finish
                case 470:   // (the two ids share one body on the console)
                    if (liPayload0 != 0)
                        lpDirectorInput->SetStarting100PercentSequence();
                    else
                        lpDirectorInput->SetFinished100PercentSequence();
                    break;

                case 475:   // +0x7AC9 / +0x7AA4  new director profile data + its payload word
                    lpDirectorInput->SetDirectorProfileData(liPayload0);
                    break;

                case 476:   // +0x7ABD  ⭐ start the NEW-PROFILE intro
                    lpDirectorInput->SetStartNewProfileIntro();
                    break;

                case 477:   // +0x7ABE  ⭐ START the GAME-INTRO FLY-BY
                    lpDirectorInput->SetStartGameIntroFlyby();
                    break;

                case 478:   // +0x7ABF  ⭐ STOP the game-intro fly-by (clears both latches)
                    lpDirectorInput->SetStopGameIntroFlyby();
                    break;

                case 479:   // +0x7AD2  online event loading started
                    lpDirectorInput->SetStartedOnlineEventLoading();
                    break;

                case 480:   // +0x7AD1  online event loading finished
                    lpDirectorInput->SetFinishedOnlineEventLoading();
                    break;

                case 501:   // Original count plus100 pointers, at native host width.
                            // Retain the existing host record-bound/legacy-wrapper guard.
                    CGS_STATIC_ASSERT((sizeof(BrnGui::GuiPFXHookEnumeration) ==
                                  BrnDirector::DirectorIO::InputBuffer::KU_HOOK_ENUMERATION_BYTES),
                                  "the director copies the canonical501 payload");
                    if (liSize >= static_cast<s32>(BrnDirector::DirectorIO::InputBuffer::KU_HOOK_ENUMERATION_BYTES
                                                   + (reinterpret_cast<const u8*>(lpuPayload)
                                                          - reinterpret_cast<const u8*>(lpEvent))))
                    {
                        lpDirectorInput->SetHookEnumeration(
                            reinterpret_cast<const BrnGui::GuiPFXHookEnumeration*>(lpuPayload));
                    }
                    break;

                case 514:
                    lpDirectorInput->SetGotColourCalibrationShownEvent();
                    break;

                case 515:
                    lpDirectorInput->SetGotColourCalibrationHiddenEvent();
                    break;

                case 591:   // +0x7AB8  the requested camera type. The console stores 0 or 1 and
                            // fires "Unhandled camera type : <n>" for anything else WITHOUT
                            // storing (GameBridgeGUIToX.cpp:1657).
                    CGS_ASSERT(liPayload0 == 0 || liPayload0 == 1, "Unhandled camera type");
                    if (liPayload0 == 0 || liPayload0 == 1)
                        lpDirectorInput->SetCameraType(liPayload0);
                    break;

                default:
                    break;
            }

            const CgsModule::Event* lpNext = 0;
            liId = lpGuiOutQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
    }

    // ARTIST 823EE0E8..823EE4D4; DecFIGS BrnGameModuleUpdateFunctions.cpp:156.
    // The input belongs to this call, including its original failure assertions
    // and destruction after the outer GameState monitor has stopped.
    void BrnGameModule::DoUpdate_GameStatePreWorld(
        CgsModule::IOBufferStack* lpInputBufferStack,
        CgsModule::IOBufferStack* lpOutputBufferStack,
        const CgsInput::InputIO::OutputBuffer* lpInputModuleOutput,
        const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutput,
        BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput,
        BrnUpdateSet lUpdateSet)
    {
        using namespace BrnGameState::GameStateModuleIO;
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_GameState);
        CgsModule::IOHelper<PreWorldInputBuffer> lInput(lpInputBufferStack, "GameStatePreWorld");
        PreWorldInputBuffer* const lpInput = lInput;
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_GameState_Bridge);
        // sub_823B6FE0 checks both pointers before taking W(input)/R(input output).
        CGS_ASSERT(lpInput != 0, "lpInputBuffer");
        CGS_ASSERT(lpInputModuleOutput != 0, "lpOutputBuffer0");
        CgsModule::LockBuffersForIO(lpInput, lpInputModuleOutput);
        const bool lbWasStalled = mbStreamingStalled;

        if (mbSteppingFrames)
        {
            if (mbStopStepping)
            {
                if (mbSimPaused)
                {
                    PlayerPauseStateChangedEvent lEvent;
                    lEvent.mbActivated = false;
                    lEvent.mbStalled = false;
                    lEvent.mbWasStalled = lbWasStalled;
                    lpInput->GetGameEventQueue()->AddEvent(&lEvent,
                        E_EVENT_PLAYER_PAUSE_STATE_CHANGED);
                }
                mbStopStepping = false;
                mbSteppingFrames = false;
                mbDoStep = false;
            }
            else if (mbDoStep)
            {
                PlayerPauseStateChangedEvent lEvent;
                lEvent.mbActivated = !mbSimPaused;
                lEvent.mbStalled = false;
                lEvent.mbWasStalled = lbWasStalled;
                if (!mbSimPaused)
                    mbDoStep = false;
                lpInput->GetGameEventQueue()->AddEvent(&lEvent,
                    E_EVENT_PLAYER_PAUSE_STATE_CHANGED);
            }
        }

        if (mbWorldStreamingRequestedStall)
        {
            if (!mbSimPaused)
            {
                if (!mbOnline)
                {
                    PlayerPauseStateChangedEvent lEvent;
                    lEvent.mbActivated = true;
                    lEvent.mbStalled = true;
                    lEvent.mbWasStalled = lbWasStalled;
                    lpInput->GetGameEventQueue()->AddEvent(&lEvent,
                        E_EVENT_PLAYER_PAUSE_STATE_CHANGED);
                }
                mbStreamingStalled = true;
            }
        }
        else if (mbStreamingStalled)
        {
            if (!mbOnline)
            {
                PlayerPauseStateChangedEvent lEvent;
                lEvent.mbActivated = false;
                lEvent.mbStalled = false;
                lEvent.mbWasStalled = lbWasStalled;
                lpInput->GetGameEventQueue()->AddEvent(&lEvent,
                    E_EVENT_PLAYER_PAUSE_STATE_CHANGED);
            }
            mbStreamingStalled = false;
        }

        if (mbStreamingStalled && sbShowStreamStallMessage)
        {
            CgsDev::DebugInterface lDebug;
            const Vector2 lPosition = {200.0f, 40.0f};
            lDebug.Get2dRender().Draw2DText(
                "STREAM STALL: Stalling game while streaming catches up",
                lPosition, 16.0f, 0xFF0000FFu);
        }

        lpNetworkOutput->LockForRead();
        BridgeNetworkToGameState(lpInput, lpNetworkOutput);
        BridgeControllerToGameState(lpInput, lpInputModuleOutput, lpNetworkOutput->IsPlaying());
        lpNetworkOutput->UnlockForRead();
        if (mMainFlowStateMachine.IsVideoState())
        {
            CgsModule::UnlockBuffersForIO(lpInput, lpInputModuleOutput);
            CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_GameState_Bridge);
        }
        else
        {
            if (mbRequestStreamingWait)
            {
                mbRequestStreamingWait = false;
                // The original empty game-start tag copies one unspecified byte.
                GameEvent<E_EVENT_GAME_START> lEvent;
                lpInput->GetGameEventQueue()->AddEvent(&lEvent, E_EVENT_GAME_START);
            }
            lpInput->SetTimerStatusInterface(&mTimerStatusInterface);
            CgsModule::UnlockBuffersForIO(lpInput, lpInputModuleOutput);
            CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_GameState_Bridge);
            if (!mbDiskError)
                mGameStateModule.PreWorldUpdate(lpInputBufferStack, lpOutputBufferStack,
                    lpInput, lpGameStateOutput, lUpdateSet);
            CheckGameActions(lpGameStateOutput);
        }
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_GameState);
    }

    // @ 0x823EE4D8 -- the SOUND pre-update leg (faithful-audio-engine phase C4). Called by
    // BOTH console spines ahead of the network/game-state/world drives: the loading spine
    // @0x823F2714 and the full DoUpdate cascade. Statement for statement against the 42-instr
    // X360 body (progress/scratch_dossiers/spine_sound_leg_0x823F22D8.md section 4):
    //   * the miUT_Sound + miUT_SoundUpdate monitor pair brackets the WHOLE body (the only
    //     sound perfmon anywhere on the loading spine);
    //   * RootSoundModule::PreUpdate publishes the logic module's pre-update block into the
    //     caller's RootPreUpdateOutputBuffer (the stack arg is only forwarded there);
    //   * under the console's W(guiIn)+R(preUpdateOut) helper bracket (sub_823B6FE0 /
    //     sub_823B7060, reproduced inline -- the per-source idiom this repo already uses),
    //     the pre-update GuiOut queue (VariableEventQueue<256,16>) is bulk-appended into the
    //     GUI input buffer's 32768 event queue (Append<256,16> @0x823DAF20), then
    //     BridgeSoundToTraining walks the audio-effects message queue.
    void BrnGameModule::DoPreUpdate_Sound(CgsModule::IOBufferStack* lpUpdateOutputBufferStack,
                                          BrnSound::Module::Io::RootPreUpdateOutputBuffer* lpSoundPreUpdateOutputBuffer,
                                          CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInputBuffer)
    {
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_Sound);
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_SoundUpdate);

        mSoundModule.PreUpdate(lpUpdateOutputBufferStack, lpSoundPreUpdateOutputBuffer);

        // Console lock-pair helper sub_823B6FE0: assert both, W(dest guiIn), R(src preUpdateOut).
        CGS_ASSERT(lpGuiInputBuffer != 0, "lpInputBuffer");
        CGS_ASSERT(lpSoundPreUpdateOutputBuffer != 0, "lpOutputBuffer0");
        lpGuiInputBuffer->LockForWrite();
        lpSoundPreUpdateOutputBuffer->LockForRead();

        lpGuiInputBuffer->GetGuiEvents()->Append<256, 16>(
            lpSoundPreUpdateOutputBuffer->GetGuiEventQueue());

        BridgeSoundToTraining(lpSoundPreUpdateOutputBuffer);

        // sub_823B7060: unlock in reverse.
        lpSoundPreUpdateOutputBuffer->UnlockForRead();
        lpGuiInputBuffer->UnlockForWrite();

        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_SoundUpdate);
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_Sound);
    }

    void BrnGameModule::DoUpdate_Sound(CgsModule::IOBufferStack* lpInputBufferStack,
                                       CgsModule::IOBufferStack* lpOutputBufferStack,
                                       BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutputBuffer,
                                       BrnWorldIO::UpdateOutputBuffer* lpWorldOutputBuffer,
                                       BrnDirector::DirectorIO::OutputBuffer* lpDirectorOutputBuffer,
                                       BrnReplays::ReplayIO::OutputBuffer_PreSim* lpReplaysPreSimOutputBuffer,
                                       BrnSound::Module::Io::RootOutputBuffer* lpSoundOutputBuffer,
                                       CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer,
                                       BrnEffects::EffectsIO::OutputBuffer* lpEffectsOutputBuffer,
                                       BrnUpdateSet leUpdateSet)
    {
        typedef BrnSound::Module::Io::RootInputBuffer RootIn;

        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_Sound);
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_SoundUpdate);

        // The self-carved "Sound" root input (console @0x823DCF20, the CgsModuleIOHelper
        // create/destroy pair with the h:52/h:57 asserts).
        RootIn* lpSoundInputBuffer = 0;
        const bool lbCreated = lpInputBufferStack->CreateIOBuffer(&lpSoundInputBuffer, "Sound");
        CGS_ASSERT(lbCreated, "mpStack->CreateIOBuffer( &mpBuffer, lpcName )");  // CgsModuleIOHelper.h:52
        (void)lbCreated;

        // sub_823B7620 (bracket A), then the effects read lock (bracket B, interleaved).
        lpSoundInputBuffer->LockForWrite();
        if (lpDirectorOutputBuffer != 0)     lpDirectorOutputBuffer->LockForRead();
        if (lpWorldOutputBuffer != 0)        lpWorldOutputBuffer->LockForRead();
        if (lpGameStateOutputBuffer != 0)    lpGameStateOutputBuffer->LockForRead();
        if (lpGuiOutputBuffer != 0)          lpGuiOutputBuffer->LockForRead();
        if (lpReplaysPreSimOutputBuffer != 0) lpReplaysPreSimOutputBuffer->LockForRead();
        if (lpEffectsOutputBuffer != 0)      lpEffectsOutputBuffer->LockForRead();

        // @0x823DCF98 -- the world -> sound copy + installs (the update set feeds the
        // bit-0x100 replay-source select).
        if (lpWorldOutputBuffer != 0)
            BridgeWorldToSound(lpSoundInputBuffer, lpWorldOutputBuffer, leUpdateSet);

        // @0x823DCFA0/AC -- the director camera into the root input (+0x2F20 on console).
        // SetCameraInput invokes Camera::operator= on the complete by-value camera,
        // matching the console root-input member at +0x2F20.
        if (lpDirectorOutputBuffer != 0)
            lpSoundInputBuffer->SetCameraInput(lpDirectorOutputBuffer->GetCameraOutput());

        // @0x823DCFBC -- the game-state bridge (this leg is its ONLY console caller).
        if (lpGameStateOutputBuffer != 0)
            BridgeGameStateToSound(lpSoundInputBuffer, lpGameStateOutputBuffer);

        // @0x823DCFD4 -- the GUI out-queue install (console reloads the gm+0x9A0BD8
        // member; on PC the caller passes the same static buffer).
        if (lpGuiOutputBuffer != 0)
            BridgeGuiToSound(lpSoundInputBuffer, lpGuiOutputBuffer);

        // @0x823DCFDC/E8 -- the replay status interface into the root input (+0x4).
        if (lpReplaysPreSimOutputBuffer != 0)
            lpSoundInputBuffer->SetReplayStatusInterface(
                reinterpret_cast<const RootIn::ReplayStatusInterface*>(
                    lpReplaysPreSimOutputBuffer->GetStatusInterface()));

        // sub_823B7760 (reverse of bracket A), then the effects unlock (bracket B).
        if (lpReplaysPreSimOutputBuffer != 0) lpReplaysPreSimOutputBuffer->UnlockForRead();
        if (lpGuiOutputBuffer != 0)          lpGuiOutputBuffer->UnlockForRead();
        if (lpGameStateOutputBuffer != 0)    lpGameStateOutputBuffer->UnlockForRead();
        if (lpWorldOutputBuffer != 0)        lpWorldOutputBuffer->UnlockForRead();
        if (lpDirectorOutputBuffer != 0)     lpDirectorOutputBuffer->UnlockForRead();
        lpSoundInputBuffer->UnlockForWrite();
        if (lpEffectsOutputBuffer != 0)      lpEffectsOutputBuffer->UnlockForRead();

        // @0x823DD054 -- the pump, with the console's exact timer products (identical to
        // the loading spine's @0x823F2AC8; cross-confirmed in the decode).
        {
            const CgsSystem::Timer& lrGameTimer = mGameTimer;
            const CgsSystem::Timer& lrSimTimer  = mSimTimer;
            mSoundModule.Update(
                lrGameTimer.GetScaleCurrent() * lrGameTimer.GetRate(),
                lrSimTimer.GetScaleCurrent() * lrSimTimer.GetRate(),
                lpInputBufferStack,
                lpOutputBufferStack,
                lpSoundInputBuffer,
                lpSoundOutputBuffer,
                leUpdateSet);
        }

        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_SoundUpdate);
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_Sound);

        const bool lbDestroyed = lpInputBufferStack->DestroyIOBuffer(&lpSoundInputBuffer);
        CGS_ASSERT(lbDestroyed, "mpStack->DestroyIOBuffer( &mpBuffer )");  // CgsModuleIOHelper.h:57
        (void)lbDestroyed;
    }

    // ARTIST 0x823DD2A0. The update thread creates the phase input, merges the game-action
    // queue and timer snapshot, calls ReplayModule::Update_PreSim through virtual slot +0x44,
    // then serialises this frame's SIM TimerStatus through GameModuleSerialiser.
    void BrnGameModule::DoUpdate_ReplaysPreSim(
        CgsModule::IOBufferStack* lpUpdateInputBufferStack,
        CgsModule::IOBufferStack* /*lpUpdateOutputBufferStack*/,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutputBuffer,
        BrnReplays::ReplayIO::OutputBuffer_PreSim* lpReplaysPreSimOutputBuffer,
        BrnUpdateSet lUpdateSet)
    {
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_Replay);

        CgsModule::IOHelper<BrnReplays::ReplayIO::InputBuffer_PreSim> lReplayInput(
            lpUpdateInputBufferStack, "ReplaysPreSim");
        BrnReplays::ReplayIO::InputBuffer_PreSim* const lpReplayInputBuffer = lReplayInput;

        // ARTIST sub_823B6FE0 pointer assertions (CgsModuleUtils.h:259-260).
        if (lpReplayInputBuffer == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpInputBuffer",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h",
                259);
            CgsDev::Assert::EndAssert();
        }
        if (lpGameStateOutputBuffer == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpOutputBuffer0",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h",
                260);
            CgsDev::Assert::EndAssert();
        }
        CgsModule::LockBuffersForIO(lpReplayInputBuffer, lpGameStateOutputBuffer);
        lpReplayInputBuffer->AppendGameActionQueue(lpGameStateOutputBuffer->GetGameActionQueue());
        lpReplayInputBuffer->SetTimerStatusInterface(&mTimerStatusInterface);
        // ARTIST sub_823B7060 pointer assertions (CgsModuleUtils.h:272-273).
        if (lpReplayInputBuffer == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpInputBuffer",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h",
                272);
            CgsDev::Assert::EndAssert();
        }
        if (lpGameStateOutputBuffer == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpOutputBuffer0",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h",
                273);
            CgsDev::Assert::EndAssert();
        }
        CgsModule::UnlockBuffersForIO(lpReplayInputBuffer, lpGameStateOutputBuffer);

        mReplayModule.Update_PreSim(lpReplayInputBuffer, lpReplaysPreSimOutputBuffer, lUpdateSet);

        mGameModuleSerialiser.Lock();
        mGameModuleSerialiser.Serialise(
            mTimerStatusInterface.GetSimTimerStatus(),
            static_cast<s32>(sizeof(CgsSystem::TimerStatus)));
        mGameModuleSerialiser.Unlock();
        mGameModuleSerialiser.SetDataReady(true);
        mGameModuleSerialiser.SetDataRestored(true);

        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_Replay);
    }

    // ARTIST 0x823DD408. Merge the five producer request interfaces and GUI events into a
    // Replay PostSim input, register the two game-owned serialisers, then call virtual slot +0x48.
    void BrnGameModule::DoUpdate_ReplaysPostSim(
        CgsModule::IOBufferStack* lpUpdateInputBufferStack,
        CgsModule::IOBufferStack* /*lpUpdateOutputBufferStack*/,
        const BrnWorldIO::UpdateOutputBuffer* lpWorldOutputBuffer,
        const BrnEffects::EffectsIO::OutputBuffer* lpEffectsOutputBuffer,
        const BrnSound::Module::Io::RootOutputBuffer* lpSoundOutputBuffer,
        BrnDirector::DirectorIO::OutputBuffer* lpDirectorOutputBuffer,
        const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer,
        BrnReplays::ReplayIO::OutputBuffer_PostSim* lpReplaysPostSimOutputBuffer,
        BrnUpdateSet lUpdateSet)
    {
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_Replay);

        CgsModule::IOHelper<BrnReplays::ReplayIO::InputBuffer_PostSim> lReplayInput(
            lpUpdateInputBufferStack, "ReplaysPostSim");
        BrnReplays::ReplayIO::InputBuffer_PostSim* const lpReplayInputBuffer = lReplayInput;

        // ARTIST sub_823B7620 pointer assertions (CgsModuleUtils.h:403-408).
        if (lpReplayInputBuffer == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpInputBuffer",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h",
                403);
            CgsDev::Assert::EndAssert();
        }
        if (lpWorldOutputBuffer == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpOutputBuffer0",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h",
                404);
            CgsDev::Assert::EndAssert();
        }
        if (lpEffectsOutputBuffer == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpOutputBuffer1",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h",
                405);
            CgsDev::Assert::EndAssert();
        }
        if (lpSoundOutputBuffer == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpOutputBuffer2",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h",
                406);
            CgsDev::Assert::EndAssert();
        }
        if (lpDirectorOutputBuffer == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpOutputBuffer3",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h",
                407);
            CgsDev::Assert::EndAssert();
        }
        if (lpGuiOutputBuffer == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpOutputBuffer4",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h",
                408);
            CgsDev::Assert::EndAssert();
        }
        CgsModule::LockBuffersForIO(lpReplayInputBuffer, lpWorldOutputBuffer, lpEffectsOutputBuffer,
                                    lpSoundOutputBuffer, lpDirectorOutputBuffer, lpGuiOutputBuffer);
        lpReplayInputBuffer->AppendRequestInterface(lpWorldOutputBuffer->GetReplayRequestInterface());
        lpReplayInputBuffer->AppendRequestInterface(lpEffectsOutputBuffer->GetReplayRequestInterface());
        lpReplayInputBuffer->AppendRequestInterface(lpSoundOutputBuffer->GetReplayRequestInterface());
        const BrnReplays::ReplayIO::RequestInterface* const lpcDirectorRequests =
            reinterpret_cast<const BrnReplays::ReplayIO::RequestInterface*>(
                lpDirectorOutputBuffer->GetReplayRe());
        lpReplayInputBuffer->AppendRequestInterface(lpcDirectorRequests);
        BridgeGuiToReplay_PostSim(lpReplayInputBuffer, lpGuiOutputBuffer);

        BrnReplays::ReplayIO::RequestInterface* const lpRequestInterface =
            lpReplayInputBuffer->GetRequestInterface();
        lpRequestInterface->RegisterSerialiser(&mDirectorBridgeSerialiser);
        lpRequestInterface->RegisterSerialiser(&mGameModuleSerialiser);
        // ARTIST sub_823B7760 pointer assertions (CgsModuleUtils.h:428-433).
        if (lpReplayInputBuffer == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpInputBuffer",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h",
                428);
            CgsDev::Assert::EndAssert();
        }
        if (lpWorldOutputBuffer == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpOutputBuffer0",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h",
                429);
            CgsDev::Assert::EndAssert();
        }
        if (lpEffectsOutputBuffer == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpOutputBuffer1",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h",
                430);
            CgsDev::Assert::EndAssert();
        }
        if (lpSoundOutputBuffer == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpOutputBuffer2",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h",
                431);
            CgsDev::Assert::EndAssert();
        }
        if (lpDirectorOutputBuffer == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpOutputBuffer3",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h",
                432);
            CgsDev::Assert::EndAssert();
        }
        if (lpGuiOutputBuffer == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpOutputBuffer4",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h",
                433);
            CgsDev::Assert::EndAssert();
        }
        CgsModule::UnlockBuffersForIO(lpReplayInputBuffer, lpWorldOutputBuffer, lpEffectsOutputBuffer,
                                      lpSoundOutputBuffer, lpDirectorOutputBuffer, lpGuiOutputBuffer);

        mReplayModule.Update_PostSim(lpReplayInputBuffer, lpReplaysPostSimOutputBuffer, lUpdateSet);

        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_Replay);
    }

    //
    // X360 order:
    //   CgsDev::PerfMonCpu::StartMonitor(gm+10055508);
    //   CreateIOBuffer<BrnWorldIO::UpdateInputBuffer>(inStack, &worldIn, "World")
    //     (asserting "mpStack->CreateIOBuffer( &mpBuffer, lpcName )", CgsModuleIOHelper.h:52);
    //   sub_823B7620(worldIn, controllerOut, gameStateOut, guiOut, replayOut, soundOut)
    //     == LockBuffersForIO with FIVE sources;
    //   UpdateInputBuffer::SetTimerStatusInterface(worldIn, gm+10095372);
    //   LockForRead(networkOut);
    //   BridgeControllerToWorld / BridgeNetworkToWorld / BridgeGameStateToWorld /
    //   BridgeGuiToWorld; SetReplayStatusInterface(ReplayIO::OutputBuffer_PreSim::
    //   GetStatusInterface(replayOut)); BridgeSoundToWorld; UnlockForRead(networkOut);
    //   sub_823B7760(...) == the matching UnlockBuffersForIO;
    //   StopMonitor;
    //   (updateSet & 0x20) ? WorldModule::UpdateForBootUpVideo(updateSet, inStack, outStack,
    //                            worldIn, worldOut)
    //                      : world->vtbl+76(updateSet, inStack, outStack, worldIn, worldOut,
    //                            gm->mpWorldUpdateFrameAllocator);
    //   LockForRead(worldOut);
    //   gm+10094123 = (worldOut->GetWorldEntityStatusInterface()->GetImmediateStreamed() == 0);
    //   UnlockForRead(worldOut);
    //   DestroyIOBuffer(inStack, &worldIn) (asserting CgsModuleIOHelper.h:57).
    void BrnGameModule::DoUpdate_World(CgsModule::IOBufferStack* lpUpdateInputBufferStack,
                                       CgsModule::IOBufferStack* lpUpdateOutputBufferStack,
                                       const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer,
                                       const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutputBuffer,
                                       const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutputBuffer,
                                       const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer,
                                       const BrnReplays::ReplayIO::OutputBuffer_PreSim* lpReplayOutputBuffer,
                                       BrnSound::Module::Io::RootPreUpdateOutputBuffer* lpSoundPreUpdateOutputBuffer,
                                       BrnWorldIO::UpdateOutputBuffer* lpWorldUpdateOutputBuffer,
                                       BrnUpdateSet lUpdateSet)
    {
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_World);
        CgsModule::IOHelper<BrnWorldIO::UpdateInputBuffer> lWorldInput(
            lpUpdateInputBufferStack, "World");
        BrnWorldIO::UpdateInputBuffer* const lpWorldInput = lWorldInput;
        CgsModule::LockBuffersForIO(lpWorldInput, lpInputOutputBuffer,
            lpGameStateOutputBuffer, lpGuiOutputBuffer, lpReplayOutputBuffer,
            lpSoundPreUpdateOutputBuffer);
        lpWorldInput->SetTimerStatusInterface(&mTimerStatusInterface);
        lpNetworkOutputBuffer->LockForRead();
        BridgeControllerToWorld(lpWorldInput, lpInputOutputBuffer);
        BridgeNetworkToWorld(lpWorldInput, lpNetworkOutputBuffer);
        BridgeGameStateToWorld(lpWorldInput, lpGameStateOutputBuffer);
        BridgeGuiToWorld(lpWorldInput, lpGuiOutputBuffer);
        lpWorldInput->SetReplayStatusInterface(lpReplayOutputBuffer->GetStatusInterface());
        BridgeSoundToWorld(lpWorldInput, lpSoundPreUpdateOutputBuffer);
        lpNetworkOutputBuffer->UnlockForRead();
        CgsModule::UnlockBuffersForIO(lpWorldInput, lpInputOutputBuffer,
            lpGameStateOutputBuffer, lpGuiOutputBuffer, lpReplayOutputBuffer,
            lpSoundPreUpdateOutputBuffer);
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_World);
        if ((lUpdateSet & 0x20) != 0)
            mWorldModule.UpdateForBootUpVideo(lUpdateSet, lpUpdateInputBufferStack,
                lpUpdateOutputBufferStack, lpWorldInput, lpWorldUpdateOutputBuffer);
        else
            mWorldModule.Update(lUpdateSet, lpUpdateInputBufferStack, lpUpdateOutputBufferStack,
                lpWorldInput, lpWorldUpdateOutputBuffer, mpReusableLoadingScreenAllocator);
        lpWorldUpdateOutputBuffer->LockForRead();
        mbWorldStreamingRequestedStall =
            !lpWorldUpdateOutputBuffer->GetWorldEntityStatusInterface()->GetImmediateStreamed();
        lpWorldUpdateOutputBuffer->UnlockForRead();
    }

    // ------------------------------------------------------------------------------------
    // DoUpdate_GameStatePostWorld -- the game-state post-world leg of the update step.
    //
    // Console order:
    //   CgsDev::PerfMonCpu::StartMonitor(miUT_GameState);
    //   IOHelper<PostWorldInputBuffer>(inStack, "GameStatePostWorld")   (asserting
    //     "mpStack->CreateIOBuffer( &mpBuffer, lpcName )", CgsModuleIOHelper.h:52);
    //   CgsDev::PerfMonCpu::StartMonitor(miUT_GameState_Bridge);
    //   LockBuffersForIO(buffer, input, world, gui, replaysPostSim, director);
    //   BridgeGuiToGameState(buffer, gui);
    //   if (!(updateSet & 0x20)) BridgeWorldToGameState(buffer, world);
    //   if (director->GetDirectorOutputInterface()->GetPauseRequest())
    //       game event 43 { GetPauseRequestState() } (1 byte);
    //   the replay post-sim game events appended onto the buffer's game-event queue;
    //   if (!mbIsLoadingScreenVisible && !mbDiskError
    //       && updateSet bit 0x4 != mbControllerDisconnected)
    //       latch it into mbControllerDisconnected and post game event 10 { it } (1 byte);
    //   buffer->SetInPictureParadise(the director camera's E_FLAG_IS_PICTURE_PARADISE);
    //   UnlockBuffersForIO (the same six); CgsDev::PerfMonCpu::StopMonitor(miUT_GameState_Bridge);
    //   if (!(updateSet & 0x20) && !mbDiskError)
    //       GameStateModule::PostWorldUpdate(inStack, outStack, buffer, updateSet);
    //   CgsDev::PerfMonCpu::StopMonitor(miUT_GameState);
    //   the helper's DestroyIOBuffer (asserting "mpStack->DestroyIOBuffer( &mpBuffer )",
    //     CgsModuleIOHelper.h:57).
    //
    // The director pause-request and controller-disconnect producers below are source-recovered.
    // The Replay append remains null-guarded because the active caller still passes null; its
    // PostSim dispatch and lifetime are a separate prerequisite. The current PC full-cascade
    // caller also supplies ConstructUpdateSetFromFsm(), whose body never sets bit 0x4, so the
    // event-10 transition remains dormant until the existing incomplete late-set path is wired.
    // BridgeGuiToGameState takes the GUI module's own out-event queue and the game-state module
    // (its signature is adapted, see its declaration), so its game events reach the module's
    // carry queue directly rather than through this buffer's game-event queue. They land ahead
    // of the buffer's events either way, which is the console's order.
    // ------------------------------------------------------------------------------------
    void BrnGameModule::DoUpdate_GameStatePostWorld(CgsModule::IOBufferStack* lpUpdateInputBufferStack,
                                                    CgsModule::IOBufferStack* lpUpdateOutputBufferStack,
                                                    const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer,
                                                    const BrnWorldIO::UpdateOutputBuffer* lpWorldUpdateOutputBuffer,
                                                    const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer,
                                                    const BrnReplays::ReplayIO::OutputBuffer_PostSim* lpReplaysPostSimOutputBuffer,
                                                    const BrnDirector::DirectorIO::OutputBuffer* lpDirectorOutputBuffer,
                                                    BrnUpdateSet lUpdateSet)
    {
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_GameState);

        CgsModule::IOHelper<BrnGameState::GameStateModuleIO::PostWorldInputBuffer> lPostWorldInput(
            lpUpdateInputBufferStack, "GameStatePostWorld");
        BrnGameState::GameStateModuleIO::PostWorldInputBuffer* lpPostWorldInput = lPostWorldInput;

        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_GameState_Bridge);

        // LockBuffersForIO: the buffer write-locked, the five sources read-locked. The replay
        // post-sim source is the only one this build can pass as null.
        lpPostWorldInput->LockForWrite();
        lpInputOutputBuffer->LockForRead();
        lpWorldUpdateOutputBuffer->LockForRead();
        lpGuiOutputBuffer->LockForRead();
        if (lpReplaysPostSimOutputBuffer != 0) lpReplaysPostSimOutputBuffer->LockForRead();
        lpDirectorOutputBuffer->LockForRead();

        BridgeGuiToGameState(lpPostWorldInput, lpGuiOutputBuffer);

        if ((lUpdateSet & 0x20) == 0)
        {
            BridgeWorldToGameState(lpPostWorldInput, lpWorldUpdateOutputBuffer);
        }

        // ARTIST 0x823E9390..0x823E93CC: event 43 follows the World bridge and precedes
        // Replay's queue append. The typed accessor asserts the Director output read lock.
        const BrnDirector::DirectorOutputInterface* lpDirectorOutputInterface =
            lpDirectorOutputBuffer->GetDirectorOutputInterface();
        if (lpDirectorOutputInterface->GetPauseRequest())
        {
            BrnGameState::GameStateModuleIO::TrainingPauseStateChangedEvent lPauseEvent;
            lPauseEvent.mbActivated = lpDirectorOutputInterface->GetPauseRequestState();
            lpPostWorldInput->GetGameEventQueue()->AddEvent(
                &lPauseEvent,
                BrnGameState::GameStateModuleIO::E_EVENT_TRAINING_PAUSE_STATE_CHANGED);
        }

        // ARTIST 0x823E93D0..0x823E93E4: append Replay events after event 43 and before
        // event 10. The active PC caller currently passes null for this output.
        if (lpReplaysPostSimOutputBuffer != 0)
        {
            lpPostWorldInput->GetGameEventQueue()->Append(
                *lpReplaysPostSimOutputBuffer->GetGameEventQueue());
        }

        // ARTIST 0x823E93F8..0x823E9450: sample update-set bit 0x4 only when neither
        // loading-screen visibility nor disk error suppresses the controller event.
        const bool lbControllerDisconnected = (lUpdateSet & 0x4) != 0;
        if (!mbIsLoadingScreenVisible && !mbDiskError &&
            lbControllerDisconnected != mbControllerDisconnected)
        {
            mbControllerDisconnected = lbControllerDisconnected;
            BrnGameState::GameStateModuleIO::ControllerDisconnectedEvent lDisconnectEvent;
            lDisconnectEvent.mbDisconnected = lbControllerDisconnected;
            lpPostWorldInput->GetGameEventQueue()->AddEvent(
                &lDisconnectEvent,
                BrnGameState::GameStateModuleIO::E_EVENT_CONTROLLER_DISCONNECTED);
        }

        // The picture-paradise byte is the director camera's own state flag (+0x144 bit 14 on the
        // console). Construct leaves it unset, so it is written on every pass.
        lpPostWorldInput->SetInPictureParadise(
            lpDirectorOutputBuffer->GetCameraOutput()->GetState().IsFlagSet(
                BrnDirector::Camera::CameraState::E_FLAG_IS_PICTURE_PARADISE));

        lpDirectorOutputBuffer->UnlockForRead();
        if (lpReplaysPostSimOutputBuffer != 0) lpReplaysPostSimOutputBuffer->UnlockForRead();
        lpGuiOutputBuffer->UnlockForRead();
        lpWorldUpdateOutputBuffer->UnlockForRead();
        lpInputOutputBuffer->UnlockForRead();
        lpPostWorldInput->UnlockForWrite();

        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_GameState_Bridge);

        if ((lUpdateSet & 0x20) == 0 && !mbDiskError)
        {
            mGameStateModule.PostWorldUpdate(lpUpdateInputBufferStack, lpUpdateOutputBufferStack,
                                             lpPostWorldInput, lUpdateSet);
        }

        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_GameState);
    }

    // ------------------------------------------------------------------------------------
    // The per-frame DIRECTOR leg (2026-07-29, DJ fly-by campaign).
    //
    // The console drives the director module through the module scheduler in three passes per
    // sub-step, bracketing the GUI update:
    //     DirectorModule::PreSceneQueryUpdate @0x8225C768   (before the scene query)
    //     <the scene manager services the queries>
    //     DirectorModule::Update              @0x82275300   (consumes the answers, runs
    //                                                        MainDirector -> the arbitrator ->
    //                                                        CameraFinaliser, PUBLISHES)
    //     <the GUI update>
    //     DirectorModule::PostGuiUpdate       @0x82250DD0
    //
    //
    //
    // The console body is:
    //     LockForRead(gameStateOut); LockForRead(directorOut);
    //     TimerRequests::Append(gm+10095420, GameStateModuleIO::OutputBuffer::GetTimerR(...));
    //     TimerRequests::Append(gm+10095428, ... + 8);
    //     if ( !((GetTimerR(...)[2] >> 2) & 1) )      // the "director owns the timers" bit
    //     {
    //         lpIn = DirectorIO::OutputBuffer::GetTimerRequestIn(directorOut);
    //         TimerRequests::Append(gm+10095420, lpIn);
    //         TimerRequests::Append(gm+10095428, lpIn + 8);
    //     }
    //     UnlockForRead(directorOut); UnlockForRead(gameStateOut);
    //     TimerRequestInterface::ApplyToTimers(gm+10095420, gm+10095316, gm+10095344);
    //     CgsSystem::Timer::Update(gm+10095316);
    //     CgsSystem::Timer::Update(gm+10095344);
    //
    // ⭐⭐ THE REQUEST HALF IS LIVE (2026-08-28, crash-slomo transport wave). THE OLD GATE NOTE
    // WAS STALE IN BOTH OF ITS CLAIMS, and it was load-bearing: it said "the two producers are
    // BOTH un-staged on this build (the GameState module is a placeholder, and the director
    // output's timer request interface is never written)". Measured against the tree:
    //   * The GameState producer is REAL and SHIPPING. DriveThruManager::SetPlayerCarDriver
    //     (BrnDriveThruManager.cpp, mounted 2026-08-27) calls
    //     `GetSimTimerRequests()->SetTimestepMultiplier(0.52631581f)` -- the drive-thru's own
    //     slow-motion presentation -- off exactly this buffer's GetTimerRequestInterface(),
    //     handed to it by GameStateModule_gUI_00.cpp:751. ModeManager::FinishCurrentMode
    //     (BrnModeManager_Finish.cpp:460) posts the 1.0f restore on the same slot.
    //   * The director producer is written as of this wave (MainDirector::Update's
    //     SetTimestepMultiplier publish, previously gated -- see BrnMainDirector.cpp).
    // ⛔ THIS FUNCTION IS THE SINGLE CHOKE POINT FOR EVERY SLOW-MOTION REQUEST IN THE GAME.
    // The X360 has exactly four callers of TimerRequests::SetTimestepMultiplier (checked
    // against the whole export set): MainDirector::Update, ModeManager::SetupGameMode,
    // ModeManager::FinishCurrentMode and DriveThruManager::SetPlayerCarDriver. All four post
    // into one of the two request interfaces this function drains, so while the Appends were
    // commented out NO producer anywhere in the game could dilate time -- the writes landed
    // and were dropped on the floor, with green logs and a plausible 1.0.
    //
    // The `(GetTimerR(...)[2] >> 2) & 1` test is `lwz r11, 8(r3); extrwi r11, r11, 1, 29`
    // @0x823BD02C-0x823BD030: the word at interface+8 is mSimTimer.muFlags and bit 2 is
    // KU_FLAG_MULTIPLIER -- i.e. the GAME-STATE module's sim slow-mo request WINS, and the
    // director's is folded in only when the game state did not ask for one this frame. (The
    // old comment glossed it as "the director owns the timers" bit, which reads the priority
    // backwards.)
    //
    //
    // ⭐⭐ ORDER -- CORRECTED 2026-09-25 (FX-DIRECTOR2): this now runs where the console runs it,
    // at the END of the step (DoUpdate @0x823F0AF8 calls it at 0x823F1CFC, after
    // DoUpdate_NetworkPostSim), so it drains the request MainDirector::Update published in THIS
    // step's DoUpdate_Director (0x82275148) and the next step's BridgeTimers snapshot carries it
    // to the world. It used to run mid-step, between the world leg and the director (boot audit
    // F-P3-13 had flagged the position and deferred the move): every director time-scale request
    // was drained one step late and integrated one step after that. Measured on the h225 s80
    // crash cell: the hard stop's 0.000125 reached the world on the crash's FOURTH frame instead
    // of its third, so the impact frame that takes the car from 197 to 102 mph ran at full dt.
    // The game-state requests were never affected (their legs run before this one either way).
    // ------------------------------------------------------------------------------------
    void BrnGameModule::UpdateTimers(
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput,
        const BrnDirector::DirectorIO::OutputBuffer* lpDirectorOutput)
    {
        lpGameStateOutput->LockForRead();
        lpDirectorOutput->LockForRead();
        const CgsSystem::TimerRequestInterface* const lpGameStateRequests =
            lpGameStateOutput->GetTimerRequestInterface();
        mTimerRequestInterface.GetGameTimerRequests()->Append(
            *lpGameStateRequests->GetGameTimerRequests());
        mTimerRequestInterface.GetSimTimerRequests()->Append(
            *lpGameStateRequests->GetSimTimerRequests());
        if (!lpGameStateOutput->GetTimerRequestInterface()->GetSimTimerRequests()->IsMultiplierRequested())
        {
            const CgsSystem::TimerRequestInterface* const lpDirectorRequests =
                lpDirectorOutput->GetTimerRequestInterface();
            mTimerRequestInterface.GetGameTimerRequests()->Append(
                *lpDirectorRequests->GetGameTimerRequests());
            mTimerRequestInterface.GetSimTimerRequests()->Append(
                *lpDirectorRequests->GetSimTimerRequests());
        }
        lpDirectorOutput->UnlockForRead();
        lpGameStateOutput->UnlockForRead();
        mTimerRequestInterface.ApplyToTimers(&mGameTimer, &mSimTimer);
        mGameTimer.Update();
        mSimTimer.Update();
    }

    // ------------------------------------------------------------------------------------
    // BridgeTimers  @ 0x823BD150   -- ⭐ THE FRAME TIMESTEP PUBLISH
    //
    //     *(gm+10095424) = 1.0f;  *(gm+10095420) = 0;      // the game TimerRequests reset
    //     *(gm+10095432) = 1.0f;  *(gm+10095428) = 0;      // the sim  TimerRequests reset
    //     TimerStatusInterface::StoreTimers(gm+10095372, gm+10095316, gm+10095344);
    //     LockForWrite(directorInput);
    //       <48-byte copy of gm+10095372 into DirectorIO::InputBuffer::GetTimerStatusInterface()>
    //     UnlockForWrite(directorInput);
    //
    // ⭐ THE TWO RESETS ARE LIVE (2026-08-28, crash-slomo transport wave). They are the RESET
    // half of the accumulate-and-reset cycle whose accumulate half UpdateTimers above now runs;
    // the old note gated them on "with no producers there is nothing to clear", which stopped
    // being true the moment the drive-thru and the director started posting. Without them the
    // accumulator's KU_FLAG_MULTIPLIER latches on the first request and the last multiplier
    // sticks for the rest of the session.
    // The four console stores ARE TimerRequests::Clear inlined twice (asm @0x823BD198..0x823BD1A4;
    // see the body now in CgsTimerRequestInterface.h) -- mfMultiplier = 1.0f, muFlags = 0, per
    // half. ⚠️ 1.0f, not 0.0f: a zero here would drive the sim timer's scale target to zero and
    // stop the world.
    // The StoreTimers snapshot and the copy into the director input are the other live half:
    // without them the director input's timer status stays at DoUpdate_Director's zero-fill,
    // every TimerStatus::GetCurrentTimeStep() reads 0, and the whole camera-behaviour middle
    // advances by `speed * 0` per frame.
    //
    void BrnGameModule::BridgeTimers(BrnDirector::DirectorIO::InputBuffer* lpDirectorInput)
    {
        mTimerRequestInterface.Clear();

        mTimerStatusInterface.StoreTimers(&mGameTimer, &mSimTimer);

        if (lpDirectorInput != 0)
        {
            lpDirectorInput->LockForWrite();
            *lpDirectorInput->GetTimerStatusInterface() = mTimerStatusInterface;
            lpDirectorInput->UnlockForWrite();
        }
    }

    // ------------------------------------------------------------------------------------
    // BrnGameModule::BridgeGameStateToDirector  @ X360 0x823CD170
    // (console home GameSource/Game/GameBridgeGameStateToX.cpp; body kept here with the other
    //  DoUpdate_Director bridges -- the BridgeGuiToDirector precedent.)
    //
    // ⭐ THE GAME-ACTION LEG (the whole reason this bridge exists, and the only leg whose two
    // ends are real today):
    //     lpDirectorInput->GetGameActionQueue()->Append( *lpGameStateOutput->GetGameActionQueue() )
    // The X360 renders it as
    //     v29 = sub_823B96F0(a3);                                  // gameStateOut->GetGameActionQueue() const
    //     v30 = BrnDirector::DirectorIO::InputBuffe(a2);           // directorIn->GetGameActionQueue()
    //     CgsModule::VariableEventQueue<13312,16>::Append<13312,16>(v30, v29);
    // -- the console itself proving both queues are the SAME instantiated type.
    //
    // ⚠️ THE BRIEF'S "TWO-LINER" IS WRONG, and it matters: 0x823CD170 is a ~150-line function
    // whose OTHER legs are, in console order --
    //   * a BrnReplays::DirectorBridgeSerialiser state machine wrapped round the whole body
    //     (Lock / GetStaticLayout / Unlock; on replay PLAYBACK the game actions are Appended
    //     from the SERIALISED layout at +10128 instead of from the live buffer, and on RECORD
    //     they are Appended into it as well);
    //   * a walk of the game-state output buffer's takedown-event queue (this+0x4040) matching
    //     each record's race-car index against the player's, publishing mbPlayerTakenDown /
    //     mePlayerKillerCarIndex into the director input (+31424 / +31404) with the verbatim
    //     assert "lePlayerKillerRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID"
    //     (GameBridgeGameStateToX.cpp:228) -- ⭐ RECONSTRUCTED 2026-09-24 (crash parity FX-BRIDGES
    //     CC-8), see the body: both of its reasons had expired (the queue is walked by name for
    //     the world and GUI bridges, and MainDirector::ProcessInputQueue / ArbStateCrashing are its
    //     consumers);
    //   * the player's vehicle TEAM into the director input (+31408) and all 8 teams through
    //     DirectorIO::InputBuffer::SetVehicleTeam -- ⭐ RECONSTRUCTED 2026-09-25 (crash parity
    //     FX-DIRECTOR2), see the body: the input's +0x7AB0 word is miPlayerTeam now and the
    //     +0x3238 team words feed AllVehicleData::Update;
    //   * a CarScoreData::operator= of the player's score record (gameStateOut+173240 + 296*i)
    //     into the director input at +12560, plus +12888;
    //   * the player's end-of-event pair at +31445 / +31446 (@0x7AD5 / @0x7AD6) -- ⭐
    //     RECONSTRUCTED 2026-09-24 (crash parity FX-DIRECTOR), see the body.
    // The score leg is still not reconstructed: its director-input destination is opaque storage
    // in this model and it is not on the crash path. Writing it against opaque zeroes would be the
    // "data arrives wrong-but-plausible" failure mode, so it is documented, not faked.
    // DELETE-WHEN: the score span of the input buffer is typed.
    // ------------------------------------------------------------------------------------
    void BrnGameModule::BridgeGameStateToDirector(
        BrnDirector::DirectorIO::InputBuffer* lpDirectorInput,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput)
    {
        // ---- the player-taken-down leg (X360 live arm 0x823CD330..0x823CD3C8) ----------------
        //   0x823CD330  r25 = `lwzx gameStateOut + 0x2AEEC` == ScoringOutputInterface
        //               (+173240) ::mePlayerRaceCarIndex (+0xA34)
        //   0x823CD340  r27 = GetTakedownEventOutputQueue() const (sub_823B9840)->miLength, read ONCE
        //   0x823CD36C  per event: the queue is re-fetched and GetEvent(i) (0x822AC660) taken;
        //               `lwz 4` (meVictimIndex) == r25 -> killer = `lwz 0` (meAggressorIndex),
        //               the :228 assert, then `stb 1, 0x7AC0` + `stw killer, 0x7AAC` (the inlined
        //               SetPlayerKiller). A later match overwrites an earlier one.
        // The cast is the documented cross-home one the world and GUI bridges carry:
        // GameStateModuleIO::TakedownEventOutputQueueType is still a forward-declared class and the
        // payload behind it is the committed EventQueue<BrnGameState::TakedownEvent,8>.
        // The director input is a fresh, Construct()ed buffer every sub-step on this build too
        // (DoUpdate_Director), so the pair starts cleared exactly as the console's does.
        {
            const EActiveRaceCarIndex lePlayerRaceCarIndex =
                lpGameStateOutput->GetScoringOutputInterface()->mePlayerRaceCarIndex;
            const s32 liNumTakedowns = reinterpret_cast<const CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>*>(
                lpGameStateOutput->GetTakedownEventOutputQueue())->GetLength();
            for (s32 liTakedown = 0; liTakedown < liNumTakedowns; ++liTakedown)
            {
                const BrnGameState::TakedownEvent& lrTakedownEvent =
                    reinterpret_cast<const CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>*>(
                        lpGameStateOutput->GetTakedownEventOutputQueue())->GetEvent(liTakedown);
                if (lrTakedownEvent.meVictimIndex == lePlayerRaceCarIndex)
                {
                    const EActiveRaceCarIndex lePlayerKillerRaceCarIndex = lrTakedownEvent.meAggressorIndex;
                    CGS_ASSERT(lePlayerKillerRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID,
                               "lePlayerKillerRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID");   // GameBridgeGameStateToX.cpp:228
                    lpDirectorInput->SetPlayerKiller(lrTakedownEvent.meAggressorIndex);

                    // [DIAG] NOT IN THE X360 BINARY -- BRN_CRASHCAM_DIAG (the crash-camera gate):
                    // the takedown camera's input as published (at most 30 lines).
                    static s32 siTakenDownLinesLeft = 30;
                }
            }
        }

        lpDirectorInput->GetGameActionQueue()->Append(*lpGameStateOutput->GetGameActionQueue());

        // ---- the player's TEAM word (X360 live arm 0x823CD428..0x823CD454) -------------------
        // ⭐ 2026-09-25 (crash parity FX-DIRECTOR2). With the player slot r25 of the takedown leg
        // (the scoring snapshot's mePlayerRaceCarIndex):
        //   0x823CD440  `addis r11, r25, 1 ; addi r11, r11, -0x540E ; slwi 2` == 4 * r25 + 0x2AFC8
        //   0x823CD450  `lwzx r11, r11, r31` -- gameStateOut + 0x2AFC8 + 4 * player ==
        //               OnlineScoringOutputInterface (+0x2AF68) ::maePlayerTeam (+0x60) [player]
        //   0x823CD454  `stw r11, 0x7AB0(r23)` -- the director input's player team, which
        //               ProcessInputQueue copies into GameState::miPlayerTeam (+0x1CC).
        // No bounds test: a -1 slot reads the word before the array, maiOnlineAwardVariables[7]
        // (gameStateOut + 0x2AFC4); that read is reproduced by name rather than by a negative index.
        // (The score record copy into +0x3110 and the +0x3258 byte that follow it, 0x823CD458..
        //  0x823CD46C, stay open: the input's score span is still opaque.)
        {
            const BrnGameState::GameStateModuleIO::OnlineScoringOutputInterface* const lpOnlineScoring =
                lpGameStateOutput->GetOnlineScoringOutputInterface();
            const s32 liPlayerSlot =
                static_cast<s32>(lpGameStateOutput->GetScoringOutputInterface()->mePlayerRaceCarIndex);
            const s32 liPlayerTeam = (liPlayerSlot == -1)
                ? lpOnlineScoring->maiOnlineAwardVariables[7]
                : static_cast<s32>(lpOnlineScoring->maePlayerTeam[liPlayerSlot]);
            lpDirectorInput->SetPlayerTeam(liPlayerTeam);                                        // @0x7AB0
        }

        // ---- the player's end-of-event pair (X360 live arm 0x823CD4A0..0x823CD510) -----------
        // ⭐ 2026-09-24 (crash parity FX-DIRECTOR). The console reads both out of the scoring
        // snapshot (gameStateOut + 0x2A4B8, the inlined GetScoringOutputInterface()) with the
        // player slot it loaded for the takedown leg (r25, `lwzx +0x2AEEC` @0x823CD33C ==
        // mePlayerRaceCarIndex), and stores them without a lock-bit test:
        //   0x823CD4A0  `add. r11, r31, 0x2A4B8` -- the accessor's non-gating assert
        //               "lpGameStateOutput->GetScoringOutputInterface()" (GameBridgeGameStateToX.cpp:256)
        //   0x823CD4D8  `lbzx +0x2AEB8 + player` == mabPlayerEliminated[player] (+0xA00), with no
        //               bounds test -> `stb 0x7AD5`
        //   0x823CD4E4  `lfsx +0x2AF4C` == mfModeTimeRemaining (+0xA94) `fcmpu` flt_82001CC0 (0.0f):
        //               `bgt` -> 0; else `lbzx +0x2AF60` == mbTimerActive (+0xAA8) != 0 -> r19 (1,
        //               `li r19, 1` @0x823CD1A8) else 0 -> `stb 0x7AD6`. An unordered compare (NaN)
        //               is not `gt`, so NaN takes the mbTimerActive arm; `!(x > 0.0f)` keeps that.
        // (The console's team word @0x7AB0 -- landed above -- the score record and the +0x3258 byte
        //  sit between the Append and this arm, 0x823CD428..0x823CD49C; the last two are still open.)
        // MainDirector::ProcessInputQueue copies the pair into GameState +0x1D0 / +0x1D1. Its one
        // reader, ArbStateRoaming::ProcessPossibleFX, plays "Damage_Crit" / "Wrecked" on it in the
        // online modes 12 / 14 / 17 only; offline, @0x7AD6 rises when a Road Rage or Stunt Run
        // clock runs out.
        {
            const BrnGameState::GameStateModuleIO::ScoringOutputInterface* const lpScoringOutputInterface =
                lpGameStateOutput->GetScoringOutputInterface();
            CGS_ASSERT(lpScoringOutputInterface != 0,
                       "lpGameStateOutput->GetScoringOutputInterface()");       // GameBridgeGameStateToX.cpp:256
            const EActiveRaceCarIndex lePlayerRaceCarIndex = lpScoringOutputInterface->mePlayerRaceCarIndex;
            lpDirectorInput->SetPlayerEliminated(
                lpScoringOutputInterface->mabPlayerEliminated[lePlayerRaceCarIndex]);            // @0x7AD5
            lpDirectorInput->SetModeTimeExpired(
                !(lpScoringOutputInterface->mfModeTimeRemaining > 0.0f) &&                       // flt_82001CC0
                lpScoringOutputInterface->mbTimerActive);                                         // @0x7AD6
        }

        // ---- the per-car TEAMS (X360 live arm 0x823CD514..0x823CD56C, the bridge's last leg) ----
        // ⭐ 2026-09-25 (crash parity FX-DIRECTOR2). r31 = gameStateOut + 0x2AFC8 (`addis 3 ; addi
        // -0x5038` @0x823CD518 / 0x823CD528) == OnlineScoringOutputInterface::maePlayerTeam, then for
        // each active race-car slot i = 0..7 (the EActiveRaceCarIndex iteration, its
        // "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT" tripwire BurnoutConstants.h:39 at 0x823CD550):
        //   `lwz r5, 0(r31)` then DirectorIO::InputBuffer::SetVehicleTeam @0x823B2938 (input, i, team)
        //   -- `stw team, 0x3238 + 4 * i`, behind its own two index asserts and the write-lock test.
        // Their one reader is AllVehicleData::Update's fifth argument (lpaVehicleTeams): the team on
        // each nearest-car row, which SqDistanceOfNearestOpposingTeamMember compares.
        {
            const BrnGameState::GameStateModuleIO::OnlineScoringOutputInterface* const lpOnlineScoring =
                lpGameStateOutput->GetOnlineScoringOutputInterface();
            for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
            {
                lpDirectorInput->SetVehicleTeam(static_cast<EActiveRaceCarIndex>(liCar),
                                                static_cast<s32>(lpOnlineScoring->maePlayerTeam[liCar]));
            }
        }
    }

    // ------------------------------------------------------------------------------------
    // ⭐⭐ BridgeGameStateToWorld  @ 0x823E1890
    //
    // Twelve transfers, no branches -- the entire console body is `mr r3,r30 / bl <getter> /
    // mr r4,r3 / mr r3,r31 / bl <setter>` twelve times over. Called by DoUpdate_World
    // @0x823E8BD0 with this sub-step's world UPDATE INPUT buffer (r4) and the game-state
    // module's OUTPUT buffer (r5). The caller owns both locks (world input write-locked,
    // game-state output read-locked); every accessor below asserts one or the other, which is
    // how the console proves that bracket.
    //
    // WHY THE FIRST LINE IS THE POINT OF THE WHOLE FUNCTION. It is the ONLY producer of the
    // world update input buffer's game-action queue in the entire image.
    // WorldModule::HandleGameActions (0x827C44D8), WorldModule::BridgeInputToEntityModules
    // (0x827ADF88) and WorldModule::BridgeActionsTo{Physics,Traffic}Module all DRAIN
    // UpdateInputBuffer::GetGameActionQueue(); nothing else ever fills it. Until now every one
    // of them was draining a queue that could not be non-empty, so no game action reached the
    // world at all -- including ResetPlayerCarAction (type 0), the record CarSelectManager
    // builds to place the player's car at a junkyard spawn location.
    //
    // FIVE of the twelve getters are UNNAMED in the ARTIST exports (sub_823B96F0 /
    // sub_823B9840 / the truncated "BrnGameState::GameStateM" @0x823B9AE0 / sub_823B9B88 /
    // sub_823BA038) and TWO are INLINED (the bridge computes `+173240` and `+175976` itself,
    // asm 0x823E1938 and 0x823E1948). All seven are identified at their accessors in
    // BrnGameStateModuleIO.h by the member offset each returns plus its assert's own
    // __FILE__/__LINE__ -- the same recovery route GetGameActionQueue() const already used.
    //
    // FLAG (home): the declared home is GameSource/Game/GameBridgeGameStateToX.cpp, alongside
    // its five siblings. The body sits HERE for the same reason BridgeGameStateToDirector's
    // does. Re-home both bridges there when this file's bridge family is consolidated.
    //
    // ⚠️ ONE DELIBERATE DEVIATION FROM A LITERAL TRANSCRIPTION, and it is load-bearing. The
    // console passes the scoring snapshots as raw addresses because there the source span and
    // the destination slice are both 2736 bytes. MEASURED on this x64 build:
    // sizeof(GameStateModuleIO::ScoringOutputInterface) == 2672, while
    // BrnWorldIO::SetScoringInterface still memcpy's the console's 2736. So the source goes
    // through GetScoringOutputInterface(), whose storage is deliberately kept at the CONSOLE's
    // width -- see the ⚠️ at that accessor. Without that the bridge would over-read the source
    // by 64 bytes every frame.
    // ------------------------------------------------------------------------------------
    void BrnGameModule::BridgeGameStateToWorld(
        BrnWorldIO::UpdateInputBuffer* lpWorldInput,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput)
    {
        CGS_ASSERT(lpWorldInput != 0, "lpWorldInput");
        CGS_ASSERT(lpGameStateOutput != 0, "lpGameStateOutput");

        // 1. the GAME-ACTION queue (sub_823B96F0 -> AppendGameActionQueue 0x823C8B80). Both
        //    sides are the same CgsModule::VariableEventQueue<13312,16> instantiation.
        lpWorldInput->AppendGameActionQueue(lpGameStateOutput->GetGameActionQueue());

        // 2. the takedown-event queue (sub_823B9840 -> AppendTakedownEventQueue 0x823C8C38).
        //    FLAG cross-home cast: GameStateModuleIO::TakedownEventOutputQueueType is still a
        //    forward-declared incomplete class; BrnWorldIO names the same X360 payload as the
        //    committed EventQueue<BrnGameState::TakedownEvent,8>.
        lpWorldInput->AppendTakedownEventQueue(
            reinterpret_cast<const BrnWorldIO::TakedownEventQueue*>(
                lpGameStateOutput->GetTakedownEventOutputQueue()));

        // 3/4. the active-payback pair (0x823B9E28 / 0x823B9ED8 -> 0x823B4F50 / 0x823B5000)
        lpWorldInput->SetActivePaybackType(lpGameStateOutput->GetActivePaybackType());
        lpWorldInput->SetActivePaybackAggressor(lpGameStateOutput->GetActivePaybackAggressor());

        // 5. the trigger-management interface (0x823B9AE0 -> 0x823DB778). Both sides are the
        //    committed BrnWorld::TriggerEntityModuleIO::TriggerManagementInputInterface now.
        lpWorldInput->AppendTriggerManagementInputInterface(
            lpGameStateOutput->GetTriggerManagementInputInterface());

        // 6. the trigger-query interface (sub_823B9B88 -> 0x823C8CF0). Both sides are the same
        //    VariableEventQueue<4096,16> instantiation.
        lpWorldInput->AppendTriggerQueryInputInterface(
            lpGameStateOutput->GetTriggerQueryInputInterface());

        // 7. the race-car race-distance interface (sub_823BA038 -> 0x823B4A58).
        //    FLAG cross-home cast: BrnWorldIO models this payload as its own 10-word slice.
        lpWorldInput->SetRaceCarRaceDistanceInterface(
            reinterpret_cast<const BrnWorldIO::RaceCarRaceDistanceInterface*>(
                lpGameStateOutput->GetRaceCarRaceDistanceInterface()));

        // 8/9. the two scoring snapshots (INLINED on X360 -> 0x823B4B28 / 0x823B4BE0).
        //      FLAG cross-home casts; see the ⚠️ in the banner for the 2736-vs-2672 span.
        lpWorldInput->SetScoringInterface(
            reinterpret_cast<const BrnWorldIO::ScoringInterface*>(
                lpGameStateOutput->GetScoringOutputInterface()));
        lpWorldInput->SetOnlineScoringInterface(
            reinterpret_cast<const BrnWorldIO::OnlineScoringInterface*>(
                lpGameStateOutput->GetOnlineScoringOutputInterface()));

        // 10. the controller-active flag (0x823B9F88 -> 0x823B4C98)
        lpWorldInput->SetControllerActive(lpGameStateOutput->GetControllerActive());
    }

    // =========================================================================================
    // BridgeEntityToEffects  @0x823CDF00 -- the WORLD -> EFFECTS bridge.
    //
    // Register decode (the pseudocode's argument numbering is scrambled; the asm is not):
    //   r3 this, r4 the world UpdateOutputBuffer, r5 the effects InputBuffer, r6 the update set.
    // Six installs, in the console's order, plus the eight-slot boost copy:
    //   SetContactSpyInterface(world->GetContactSpyInterface())
    //   SetDeformationInterface(world->GetDeformationOutputInterface())
    //   SetPropVFXLocatorQueue(world->GetPropVFXLocatorQueue())
    //   SetActiveRaceCarInterface( (updateSet & 0x100) ? GetReplayActiveRaceCarOutputInterface()
    //                                                  : GetActiveRaceCarOutputInterface() )
    //   -- then for i in 0..7: nine words from (interface + 0x210) + i*0x24 into
    //      (effectsIn + 4) + i*0x24. That is SetBoostInfoN(i, GetBoostOutputInfoN(i)) inlined:
    //      the two asserts the loop carries are GetBoostOutputInfoN's own bound checks
    //      (BrnRaceCarEntityModuleOutputInterface.h:1157/:1158), and 0x210 == 528 is exactly
    //      RCEntityActiveRaceCarOutputInterface::maBoostOutputInfo.
    //   SetVehiclePhysicalStateQueue(world->GetVehicleOutputInterface() + 0x2620)
    //   SetEffectsEnvironmentInterface(world->GetEffectsEnvironmentInterface())
    //   SetTriangleCacheInterface(world->GetTriangleCacheInterface())
    // =========================================================================================
    void BrnGameModule::BridgeEntityToEffects(const BrnWorldIO::UpdateOutputBuffer* lpWorldOutputBuffer,
                                              BrnEffects::EffectsIO::InputBuffer* lpEffectsInputBuffer,
                                              BrnUpdateSet leUpdateSet)
    {
        typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface ActiveRaceCars;

        lpEffectsInputBuffer->SetContactSpyInterface(lpWorldOutputBuffer->GetContactSpyInterface());
        lpEffectsInputBuffer->SetDeformationInterface(lpWorldOutputBuffer->GetDeformationOutputInterface());
        lpEffectsInputBuffer->SetPropVFXLocatorQueue(lpWorldOutputBuffer->GetPropVFXLocatorQueue());

        // `rlwinm r11, r31, 0,23,23` == the update set's bit 8 (0x100): the replay source select.
        const ActiveRaceCars* lpActiveRaceCars =
            ((static_cast<u32>(leUpdateSet) & 0x100u) != 0)
                ? lpWorldOutputBuffer->GetReplayActiveRaceCarOutputInterface()
                : lpWorldOutputBuffer->GetActiveRaceCarOutputInterface();
        lpEffectsInputBuffer->SetActiveRaceCarInterface(lpActiveRaceCars);

        for (s32 liCar = 0;
             liCar < static_cast<s32>(BrnEffects::EffectsIO::InputBuffer::KU_NUM_BOOST_INFOS);
             ++liCar)
        {
            const EActiveRaceCarIndex leIndex = static_cast<EActiveRaceCarIndex>(liCar);
            lpEffectsInputBuffer->SetBoostInfoN(liCar, lpActiveRaceCars->GetBoostOutputInfoN(leIndex));
        }

        // `addi r4, r11, 0x2620` -- the vehicle output interface's own physical-traffic queue.
        lpEffectsInputBuffer->SetVehiclePhysicalStateQueue(
            lpWorldOutputBuffer->GetVehicleOutputInterface()->GetTrafficStateQueue());   // +0x2620
        lpEffectsInputBuffer->SetEffectsEnvironmentInterface(
            lpWorldOutputBuffer->GetEffectsEnvironmentInterface());
        lpEffectsInputBuffer->SetTriangleCacheInterface(
            reinterpret_cast<const BrnEffects::EffectsIO::InputBuffer::InTriangleCacheInterface*>(
                lpWorldOutputBuffer->GetTriangleCacheInterface()));
    }

    // =========================================================================================
    // DoUpdate_Effects  @0x823DD0A8 -- the EFFECTS leg of the per-frame cascade.
    //
    // The console order (asm 0x823DD0A8-0x823DD29C), statement for statement. The lock helper
    // sub_823B78A0 is LockBuffersForIO(effectsIn, out0..out5) -- W(effectsIn) then R(out0..out5)
    // in argument order, with sub_823B7A10 the exact reverse; both are reproduced inline, the
    // same idiom DoUpdate_Sound already uses in this file.
    //
    void BrnGameModule::DoUpdate_Effects(CgsModule::IOBufferStack* lpInputBufferStack,
                                         CgsModule::IOBufferStack* lpOutputBufferStack,
                                         const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer,
                                         BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutputBuffer,
                                         BrnWorldIO::UpdateOutputBuffer* lpWorldOutputBuffer,
                                         BrnDirector::DirectorIO::OutputBuffer* lpDirectorOutputBuffer,
                                         BrnReplays::ReplayIO::OutputBuffer_PreSim* lpReplaysPreSimOutputBuffer,
                                         BrnSound::Module::Io::RootPreUpdateOutputBuffer* lpSoundPreUpdateOutputBuffer,
                                         BrnEffects::EffectsIO::OutputBuffer* lpEffectsOutputBuffer,
                                         bool lbSuspendEffects,
                                         BrnUpdateSet leUpdateSet)
    {
        typedef BrnEffects::EffectsIO::InputBuffer EffectsIn;

        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_Effects);

        // The self-carved "Effects" input (console @0x823DD0F8, the CgsModuleIOHelper
        // create/destroy pair with the h:52/h:57 asserts).
        EffectsIn* lpEffectsInputBuffer = 0;
        const bool lbCreated = lpInputBufferStack->CreateIOBuffer(&lpEffectsInputBuffer, "Effects");
        CGS_ASSERT(lbCreated, "mpStack->CreateIOBuffer( &mpBuffer, lpcName )");  // CgsModuleIOHelper.h:52
        (void)lbCreated;
        if (lpEffectsInputBuffer == 0)
        {
            CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_Effects);
            return;
        }

        // sub_823B78A0(effectsIn, director, input, world, gameState, replays, soundPreUpdate)
        lpEffectsInputBuffer->LockForWrite();
        if (lpDirectorOutputBuffer != 0)       lpDirectorOutputBuffer->LockForRead();
        if (lpInputOutputBuffer != 0)          const_cast<CgsInput::InputIO::OutputBuffer*>(lpInputOutputBuffer)->LockForRead();
        if (lpWorldOutputBuffer != 0)          lpWorldOutputBuffer->LockForRead();
        if (lpGameStateOutputBuffer != 0)      lpGameStateOutputBuffer->LockForRead();
        if (lpReplaysPreSimOutputBuffer != 0)  lpReplaysPreSimOutputBuffer->LockForRead();
        if (lpSoundPreUpdateOutputBuffer != 0) lpSoundPreUpdateOutputBuffer->LockForRead();

        // @0x823DD154/60 -- the game module own timer status (gm + 0x9A0B0C).
        lpEffectsInputBuffer->SetTimerStatusInterface(&mTimerStatusInterface);

        // @0x823DD164/74 -- the director finalised camera.
        if (lpDirectorOutputBuffer != 0)
            lpEffectsInputBuffer->SetCameraInput(lpDirectorOutputBuffer->GetCameraOutput());

        // @0x823DD178/A0 -- pad 0 button word bit 1 raises the QA "restart effects" latch:
        //   `lwz r11, 0x34(padInfo); extrwi r11, r11, 1,30` == (buttons >> 1) & 1.
        if (lpInputOutputBuffer != 0)
        {
            // `lwz r11, 0x34(padInfo)` == maActionInfo[3].muStatus (+0x18 + 3*8 + 4), and
            // `extrwi r11,r11,1,30` is its bit 1 == pressed-this-frame.
            const CgsInput::InputIO::PadOutputInformation* lpPad = lpInputOutputBuffer->GetPadInfo(0);
            if (lpPad != 0 && ((lpPad->maActionInfo[3].muStatus >> 1) & 1u) != 0)
                BrnEffects::EffectsModule::RestartEffects();
        }

        // @0x823DD1A4/B8 -- the world -> effects bridge (it takes the update set, not the
        // buffer own copy).
        if (lpWorldOutputBuffer != 0)
            BridgeEntityToEffects(lpWorldOutputBuffer, lpEffectsInputBuffer, leUpdateSet);

        // @0x823DD1BC/D4 -- the game-state action queue, appended (NOT replaced).
        if (lpGameStateOutputBuffer != 0)
        {
            // ⚠ THE CONST OVERLOAD, DELIBERATELY. MEASURED (runs 4-6, 2026-09-02): the
            // non-const GetGameActionQueue @0x8231D4B8 asserts "Not locked for writing" and
            // this buffer is read-locked three lines up -- 15,333 firings in one boot, which
            // by themselves slowed the boot roughly four-fold (13.6s -> 53s to the new-profile
            // mark) and left the flow parked at CARSELECT. The console calls the READ-lock
            // accessor here (sub_823B96F0, BrnGameStateModuleIO.h:265): this leg only COPIES
            // the queue out of the game-state output buffer.
            const BrnGameState::GameStateModuleIO::OutputBuffer* lpcGameStateOutputBuffer =
                lpGameStateOutputBuffer;
            lpEffectsInputBuffer->GetGameActionQueue()->Append<13312, 16>(
                *lpcGameStateOutputBuffer->GetGameActionQueue());
        }

        // @0x823DD1D8/E8 -- the replay status interface.
        if (lpReplaysPreSimOutputBuffer != 0)
        {
            lpEffectsInputBuffer->SetReplayStatusInterface(
                lpReplaysPreSimOutputBuffer->GetStatusInterface());
        }

        // @0x823DD1EC/200 -- the sound pre-update block audio-effects message queue
        // (`addi r4, r11, 0x2A0`).
        if (lpSoundPreUpdateOutputBuffer != 0)
        {
            lpEffectsInputBuffer->SetAudioEffectsMessageQueue(
                &lpSoundPreUpdateOutputBuffer->GetPreUpdateOutput().GetAudioEffectsMessageQueue());
        }

        // @0x823DD228 -- `stbx r10, r31, 0xE494`: the suspend flag.
        lpEffectsInputBuffer->SetSuspendEffects(lbSuspendEffects);

        // sub_823B7A10 -- the exact reverse of the lock helper.
        if (lpSoundPreUpdateOutputBuffer != 0) lpSoundPreUpdateOutputBuffer->UnlockForRead();
        if (lpReplaysPreSimOutputBuffer != 0)  lpReplaysPreSimOutputBuffer->UnlockForRead();
        if (lpGameStateOutputBuffer != 0)      lpGameStateOutputBuffer->UnlockForRead();
        if (lpWorldOutputBuffer != 0)          lpWorldOutputBuffer->UnlockForRead();
        if (lpInputOutputBuffer != 0)          const_cast<CgsInput::InputIO::OutputBuffer*>(lpInputOutputBuffer)->UnlockForRead();
        if (lpDirectorOutputBuffer != 0)       lpDirectorOutputBuffer->UnlockForRead();
        lpEffectsInputBuffer->UnlockForWrite();

        // @0x823DD234/58 -- the module own step, through vtable +0x44.
        mEffectsModule.Update(lpInputBufferStack, lpOutputBufferStack,
                              lpEffectsInputBuffer, lpEffectsOutputBuffer, leUpdateSet);

        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_Effects);

        const bool lbDestroyed = lpInputBufferStack->DestroyIOBuffer(&lpEffectsInputBuffer);
        CGS_ASSERT(lbDestroyed, "mpStack->DestroyIOBuffer( &mpBuffer )");  // CgsModuleIOHelper.h:57
        (void)lbDestroyed;
    }

    //
    // Once per director update, AFTER DirectorModule::Update has published the frame's camera, the PUBLISHED eye
    // is put through the console's own WORLD tests. These diagnostic queries are staged separately from
    // the director's camera queries on a private SceneManagerIO::InputBuffer_Query and
    // answered by the same WorldModule::ExternalSceneQueriesUpdate the closure uses, so every answer comes from
    // SceneManagerModule::ProcessVolumeTestDeepest / ProcessLineTestNearest's world arms (flags 2, world only --
    // no entity candidates, so no fine-module kernel is reached by the lines):
    //   in     the 0.1 m sphere (flt_82004014, VisibilityCollisionPolicy's own camera-in-geometry test) at the eye
    //          intersects the world;
    //   above  a nearest line from 200 m above the eye DOWN to the eye hits a surface (the world is single-sided:
    //          a hit means a surface facing up lies above the eye);
    //   below  a nearest line from the eye DOWN 200 m hits a surface;
    //   occA / occB  a nearest line eye -> player car / car -> eye hits the world (both directions, as
    //          VisibilityTest does, because a single-sided wall only answers from its front);
    // UNDER (below the map) = above && !below; OCC = occA || occB. One line per update, capped. The arbitrator
    // state that owns each frame is read off BRN_CRASHCAM_DIAG's "[crashcam] container current state" lines.
    // DELETE-WHEN: the owner's camera-collision item is closed.
    // ============================================================================
    namespace
    {
        const s32 KI_CAMCOLLIDE_MAX_LINES    = 30000;
        const f32 KF_CAMCOLLIDE_PROBE_METRES = 200.0f;

    }

    // ARTIST 823E8DE0. The caller owns Director input and all producer outputs.
    // Only the three scene-query buffers below are local to this wrapper.
    void BrnGameModule::DoUpdate_Director(
        CgsModule::IOBufferStack* lpInputStack,
        CgsModule::IOBufferStack* lpOutputStack,
        BrnDirector::DirectorIO::InputBuffer* lpDirectorInput,
        const CgsInput::InputIO::OutputBuffer* lpInputOutput,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput,
        const BrnWorldIO::UpdateOutputBuffer* lpWorldOutput,
        const BrnReplays::ReplayIO::OutputBuffer_PreSim* lpReplayOutput,
        BrnDirector::DirectorIO::OutputBuffer* lpDirectorOutput,
        BrnUpdateSet lUpdateSet)
    {
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_Director);
        if ((lUpdateSet & 0x20) == 0)
        {
            CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_Director_Bridge);
            if (mDirectorBridgeSerialiser.IsPlaying())
                mDirectorBridgeSerialiser.Read();
            CgsModule::IOHelper<CgsSceneManager::SceneManagerIO::InputBuffer_Query>
                lExternalQuery(lpInputStack, "SceneQuery");
            CgsModule::IOHelper<BrnDirector::DirectorIO::SceneQueryInputBuffer>
                lSceneQueryInput(lpInputStack, "DirectorSQ");
            CgsModule::LockBuffersForIO(lpDirectorInput, lpInputOutput, lpGameStateOutput,
                lpWorldOutput, lpReplayOutput);
            BridgeControllerToDirector(lpDirectorInput, lpInputOutput);
            BridgeWorldToDirector(lpDirectorInput, lpWorldOutput);
            BridgeGameStateToDirector(lpDirectorInput, lpGameStateOutput);
            lpDirectorInput->SetSimPaused((lUpdateSet & 0x100) == 0 && !mbOnline && mbSimPaused);
            if (mDirectorBridgeSerialiser.IsRecording())
                mDirectorBridgeSerialiser.Write();
            CgsModule::UnlockBuffersForIO(lpDirectorInput, lpInputOutput, lpGameStateOutput,
                lpWorldOutput, lpReplayOutput);
            CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_Director_Bridge);
            {
                CgsModule::IOHelper<BrnDirector::DirectorIO::SceneQueryOutputBuffer>
                    lSceneQueryOutput(lpOutputStack, "DirectorSQ");
                mDirectorModule.PreSceneQueryUpdate(lpInputStack, lpOutputStack,
                    lpDirectorInput, lpDirectorOutput, lSceneQueryOutput, lUpdateSet);
                CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_Director_Bridge);
                CgsSceneManager::SceneManagerIO::InputBuffer_Query* const lpQuery = lExternalQuery;
                const BrnDirector::DirectorIO::SceneQueryOutputBuffer* const lpOutput = lSceneQueryOutput;
                CgsModule::LockBuffersForIO(lpQuery, lpOutput);
                lpQuery->GetSceneQueryInterface()->Append(*lpOutput->GetSceneQueryInterface());
                CgsModule::UnlockBuffersForIO(lpQuery, lpOutput);
                CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_Director_Bridge);
            }
            {
                CgsModule::IOHelper<CgsSceneManager::SceneManagerIO::OutputBuffer>
                    lExternalResults(lpOutputStack, "DirectorESQ");
                CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_Director_SQ);
                mWorldModule.ExternalSceneQueriesUpdate(lpInputStack, lpOutputStack,
                    lExternalQuery, lExternalResults, lUpdateSet);
                CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_Director_SQ);
                CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_Director_Bridge);
                BrnDirector::DirectorIO::SceneQueryInputBuffer* const lpInput = lSceneQueryInput;
                const CgsSceneManager::SceneManagerIO::OutputBuffer* const lpResults = lExternalResults;
                CgsModule::LockBuffersForIO(lpInput, lpResults);
                lpInput->GetResultsQueue()->Append(*lpResults->GetResultsQueue());
                CgsModule::UnlockBuffersForIO(lpInput, lpResults);
                CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_Director_Bridge);
            }
            {
                CgsModule::IOHelper<BrnDirector::DirectorIO::SceneQueryOutputBuffer>
                    lSceneQueryOutput(lpOutputStack, "DirectorSQ");
                mDirectorModule.Update(lpInputStack, lpOutputStack, lpDirectorInput,
                    lpDirectorOutput, lSceneQueryInput, lSceneQueryOutput);
            }
            lpDirectorOutput->LockForRead();
            if (lpDirectorOutput->GetCameraOutput()->GetState().IsFlagSet(
                    BrnDirector::Camera::CameraState::E_FLAG_RECORD_METRICS))
            {
                mbTakeMetrics = true;
                mRecordMetricsCamMatrix = lpDirectorOutput->GetCameraOutput()->GetTransform();
            }
            if (lpDirectorOutput->GetCameraOutput()->GetState().IsFlagSet(
                    BrnDirector::Camera::CameraState::E_FLAG_SEND_METRICS))
                mbSendMetrics = true;
            lpDirectorOutput->UnlockForRead();
        }
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_Director);
    }

    // ARTIST 823DCE38: a separate supplied-IO post-GUI leg.
    void BrnGameModule::DoUpdate_DirectorPostGUI(
        CgsModule::IOBufferStack* lpInputStack,
        CgsModule::IOBufferStack* lpOutputStack,
        BrnDirector::DirectorIO::InputBuffer* lpDirectorInput,
        const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput,
        BrnDirector::DirectorIO::OutputBuffer* lpDirectorOutput)
    {
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_Director);
        CgsModule::LockBuffersForIO(lpDirectorInput, lpGuiOutput);
        BridgeGuiToDirector(lpDirectorInput, lpGuiOutput);
        CgsModule::UnlockBuffersForIO(lpDirectorInput, lpGuiOutput);
        mDirectorModule.PostGuiUpdate(lpInputStack, lpOutputStack,
            lpDirectorInput, lpDirectorOutput);
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_Director);
    }

    // ARTIST 823F0758. This value is BSS-zeroed at native82FAEBC4 and
    // advances by0.5 only on frames whose update set does not contain0x80.
    static f32 sfGuiLoadingPercentage;

    void BrnGameModule::DoUpdate_GUI(
        CgsModule::IOBufferStack* lpInputStack,
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
        BrnSound::Module::Io::RootOutputBuffer* /*lpSoundOutput*/,
        BrnResource::GameDataIO::InputBuffer* lpResourceInput,
        const BrnResource::GameDataIO::OutputBuffer* lpResourceOutput,
        BrnUpdateSet lUpdateSet, u32 luStartPressedPort)
    {
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_GUI);
        CgsDev::PerfMonCpu::StartMonitor(BrnGui::GuiPerfmons::miGuiModuleUpdate);
        lpGuiOutput->Clear();
        lpModelOutput->Clear();
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_GUI_Bridge);
        const BrnDirector::DirectorIO::OutputBuffer* const lpDirectorRead = lpDirectorOutput;
        CgsModule::LockBuffersForIO(lpGuiInput, lpInputOutput, lpGameStateOutput,
            lpWorldOutput, lpReplayOutput, lpDirectorRead);
        BridgeWorldToGui(lpGuiInput, lpWorldOutput);
        BridgeControllerToGui(lpGuiInput, lpInputOutput);
        BridgeGameStateToGui(lpGuiInput, lpGameStateOutput);
        BridgeDirectorToGui(lpGuiInput, lpDirectorRead);
        BridgeReplayToGui(lpGuiInput, lpReplayOutput);
        lpNetworkOutput->LockForRead();
        BridgeNetworkToGui(lpGuiInput, lpNetworkOutput);
        lpNetworkOutput->UnlockForRead();
        f32 lfLoadingPercentage = -1.0f;
        if ((lUpdateSet & 0x80) == 0)
        {
            const XMVECTOR lOne = __vcfsx(__vspltisw(1), 0);
            const XMVECTOR lHundred = XMVectorReplicate(100.0f);
            const XMVECTOR lValue = XMVectorReplicate(sfGuiLoadingPercentage + 0.5f);
            XMVECTOR lInverse = __vrefp(lHundred);
            lInverse = __vmaddfp(lInverse, __vnmsubfp(lInverse, lHundred, lOne), lInverse);
            lInverse = __vmaddfp(lInverse, __vnmsubfp(lInverse, lHundred, lOne), lInverse);
            const XMVECTOR lQuotient = __vrfiz(__vmulfp(lInverse, lValue));
            sfGuiLoadingPercentage = __vnmsubfp(lQuotient, lHundred, lValue).x;
            lfLoadingPercentage = sfGuiLoadingPercentage;
        }
        lpGuiInput->GetGuiEvents()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lfLoadingPercentage), 225, sizeof(lfLoadingPercentage));
        if (luStartPressedPort != CgsInput::KU_NUMBER_OF_PADS)
            lpGuiInput->GetGuiEvents()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&luStartPressedPort), 143, sizeof(luStartPressedPort));
        BridgeGameToGui(lpGuiInput);
        CgsModule::UnlockBuffersForIO(lpGuiInput, lpInputOutput, lpGameStateOutput,
            lpWorldOutput, lpReplayOutput, lpDirectorRead);
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_GUI_Bridge);
        lpResourceInput->LockForWrite();
        lpResourceOutput->LockForRead();
        if (!mbDiskError)
            mGuiModule.Update(lUpdateSet, lpInputStack, lpOutputStack,
                lpGuiInput, lpGuiOutput, lpModelOutput, lpViewInput,
                lpResourceInput, lpResourceOutput, true);
        lpResourceOutput->UnlockForRead();
        lpResourceInput->UnlockForWrite();
        lpGuiOutput->LockForRead();
        BridgeGuiToGame(lpGuiOutput);
        lpGuiOutput->UnlockForRead();
        CgsDev::PerfMonCpu::StopMonitor(BrnGui::GuiPerfmons::miGuiModuleUpdate);
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_GUI);
    }

    // ARTIST 823BD0A8. GameState requests override the previous frame type,
    // after which both fields in the shared request are cleared.
    void BrnGameModule::UpdateFrameRateType(
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput)
    {
        lpGameStateOutput->LockForRead();
        mFrameRateTypeRequestInterface.Append(*lpGameStateOutput->GetFrameRateTypeRequestInterface());
        lpGameStateOutput->UnlockForRead();
        if (mFrameRateTypeRequestInterface.IsFrameRateTypeChangeRequested())
        {
            if (!mFrameRateTypeRequestInterface.IsFrameRateTypeChangeRequested())
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("mbIsFrameRateTypeChangeRequested",
                    "..\\..\\..\\GameShared\\GameClasses\\System\\Timer/CgsFrameRateTypeRequestInterface.h", 113);
                CgsDev::Assert::EndAssert();
            }
            meFrameRateManagerType = mFrameRateTypeRequestInterface.GetRequestedFrameRateTypeChange();
        }
        mFrameRateTypeRequestInterface.Clear();
    }

    // ARTIST 823F0AF8 repeats this exact diagnostic after each module leg.
    // Keep the original source site numbers even though the expression is shared.
    static __forceinline void CheckUpdateMemory(u32& lruAvailableMemory, s32 liLine)
    {
        const u32 luNow = BrnResource::GetAvailableMemory();
        if (lruAvailableMemory != luNow)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
                *CgsDev::Log::gpDebugPrint << "[" << liLine << ":"
                    << "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnGameModule.cpp"
                    << "] Available memory changed from " << lruAvailableMemory
                    << " to " << luNow << "\n";
            lruAvailableMemory = luNow;
            if (BrnResource::Allocators::mbAssertOnMemoryChange)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("Memory check failed",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnGameModule.cpp", liLine);
                CgsDev::Assert::EndAssert();
            }
        }
    }

    // ARTIST 823F0AF8. These ten local buffers live for exactly one simulation
    // step. The five static GUI/Director buffers belong to GameMain instead.
    void BrnGameModule::DoUpdate()
    {
        u32 luAvailableMemory = BrnResource::GetAvailableMemory();
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_Director);
        CgsSystem::GetSystemTimerBaseTime();
        const BrnUpdateSet lInitialUpdateSet = ConstructUpdateSetFromFsm();
        CgsModule::IOHelper<CgsInput::InputIO::OutputBuffer> lInputOutput(mpUpdateInputBufferStack, "Input");
        CgsModule::IOHelper<BrnNetwork::BrnNetworkModuleIO::OutputBuffer> lNetworkOutput(mpUpdateOutputBufferStack, "Network");
        CgsModule::IOHelper<BrnGameState::GameStateModuleIO::OutputBuffer> lGameStateOutput(mpUpdateOutputBufferStack, "GameState");
        CgsModule::IOHelper<BrnWorldIO::UpdateOutputBuffer> lWorldOutput(mpUpdateOutputBufferStack, "World");
        CgsModule::IOHelper<BrnDirector::DirectorIO::InputBuffer> lDirectorInput(mpUpdateInputBufferStack, "Director");
        CgsModule::IOHelper<BrnSound::Module::Io::RootOutputBuffer> lSoundOutput(mpUpdateOutputBufferStack, "Sound");
        CgsModule::IOHelper<BrnReplays::ReplayIO::OutputBuffer_PreSim> lReplayPreOutput(mpUpdateOutputBufferStack, "ReplaysPreSim");
        CgsModule::IOHelper<BrnReplays::ReplayIO::OutputBuffer_PostSim> lReplayPostOutput(mpUpdateOutputBufferStack, "ReplaysPostSim");
        CgsModule::IOHelper<BrnEffects::EffectsIO::OutputBuffer> lEffectsOutput(mpUpdateOutputBufferStack, "Effects");
        CgsModule::IOHelper<BrnSound::Module::Io::RootPreUpdateOutputBuffer> lSoundPreOutput(mpUpdateOutputBufferStack, "SoundPreUpdate");
        CgsInput::InputIO::OutputBuffer* const lpInputOutput = lInputOutput;
        BrnNetwork::BrnNetworkModuleIO::OutputBuffer* const lpNetworkOutput = lNetworkOutput;
        BrnGameState::GameStateModuleIO::OutputBuffer* const lpGameStateOutput = lGameStateOutput;
        BrnWorldIO::UpdateOutputBuffer* const lpWorldOutput = lWorldOutput;
        BrnDirector::DirectorIO::InputBuffer* const lpDirectorInput = lDirectorInput;
        BrnSound::Module::Io::RootOutputBuffer* const lpSoundOutput = lSoundOutput;
        BrnReplays::ReplayIO::OutputBuffer_PreSim* const lpReplayPreOutput = lReplayPreOutput;
        BrnReplays::ReplayIO::OutputBuffer_PostSim* const lpReplayPostOutput = lReplayPostOutput;
        BrnEffects::EffectsIO::OutputBuffer* const lpEffectsOutput = lEffectsOutput;
        BrnSound::Module::Io::RootPreUpdateOutputBuffer* const lpSoundPreOutput = lSoundPreOutput;
        BrnResource::GameDataIO::InputBuffer* const lpResourceInput = mpGameDataModuleInputBuffer;
        const BrnResource::GameDataIO::OutputBuffer* const lpResourceOutput = mpGameDataModuleOutputBuffer;
        BridgeTimers(lpDirectorInput);
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_Director);
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_DebugManager);
        lpResourceOutput->LockForRead();
        const bool lbSuspendEffects = *lpResourceOutput->GetLiveUpdateStatus();
        lpResourceOutput->UnlockForRead();
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_DebugManager);
        if (!mpReusableLoadingScreenAllocator)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mpReusableLoadingScreenAllocator",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnGameModule.cpp", 2008);
            CgsDev::Assert::EndAssert();
        }
        mpReusableLoadingScreenAllocator->FreeAll();
        CheckUpdateMemory(luAvailableMemory, 2013);
        const u32 luStartPressedPort = DoUpdate_InputPreWorld(mpUpdateInputBufferStack,
            mpUpdateOutputBufferStack, lpInputOutput);
        CheckUpdateMemory(luAvailableMemory, 2021);
        DoUpdate_NetworkPreSim(mpUpdateInputBufferStack, mpUpdateOutputBufferStack,
            lpInputOutput, lpNetworkOutput, lInitialUpdateSet);
        CheckUpdateMemory(luAvailableMemory, 2033);
        DoUpdate_GameStatePreWorld(mpUpdateInputBufferStack, mpUpdateOutputBufferStack,
            lpInputOutput, lpNetworkOutput, lpGameStateOutput, lInitialUpdateSet);
        CheckUpdateMemory(luAvailableMemory, 2048);
        DoUpdate_ReplaysPreSim(mpUpdateInputBufferStack, mpUpdateOutputBufferStack,
            lpGameStateOutput, lpReplayPreOutput, lInitialUpdateSet);
        CheckUpdateMemory(luAvailableMemory, 2059);
        DoUpdate_GuiPreWorld(mpUpdateInputBufferStack, mpUpdateOutputBufferStack,
            mpGuiOutputBuffer, mpGuiModelOutputBuffer, lpResourceInput);
        CheckUpdateMemory(luAvailableMemory, 2070);
        DoPreUpdate_Sound(mpUpdateOutputBufferStack, lpSoundPreOutput, mpGuiInputBuffer);
        CheckUpdateMemory(luAvailableMemory, 2079);
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_GameState_Bridge);
        const BrnUpdateSet lUpdateSet = ConstructUpdateSet(lpGameStateOutput,
            lpNetworkOutput, lpInputOutput, mpGuiModelOutputBuffer, lpReplayPreOutput);
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_GameState_Bridge);
        CheckUpdateMemory(luAvailableMemory, 2094);
        DoUpdate_World(mpUpdateInputBufferStack, mpUpdateOutputBufferStack, lpInputOutput,
            lpNetworkOutput, lpGameStateOutput, mpGuiOutputBuffer, lpReplayPreOutput,
            lpSoundPreOutput, lpWorldOutput, lUpdateSet);
        CheckUpdateMemory(luAvailableMemory, 2112);
        DoUpdate_InputPostWorld(mpUpdateInputBufferStack, mpUpdateOutputBufferStack,
            lpGameStateOutput, lpWorldOutput, luStartPressedPort);
        CheckUpdateMemory(luAvailableMemory, 2123);
        DoUpdate_Director(mpUpdateInputBufferStack, mpUpdateOutputBufferStack, lpDirectorInput,
            lpInputOutput, lpGameStateOutput, lpWorldOutput, lpReplayPreOutput,
            mpDirectorOutputBuffer, lUpdateSet);
        CheckUpdateMemory(luAvailableMemory, 2138);
        if (!mbSteppingFrames)
            DoUpdate_GUI(mpUpdateInputBufferStack, mpUpdateOutputBufferStack,
                mpGuiInputBuffer, mpGuiViewInputBuffer, lpInputOutput, lpNetworkOutput,
                lpGameStateOutput, lpWorldOutput, lpReplayPreOutput, mpGuiOutputBuffer,
                mpGuiModelOutputBuffer, mpDirectorOutputBuffer, lpSoundOutput,
                lpResourceInput, lpResourceOutput, lUpdateSet, luStartPressedPort);
        CheckUpdateMemory(luAvailableMemory, 2166);
        DoUpdate_DirectorPostGUI(mpUpdateInputBufferStack, mpUpdateOutputBufferStack,
            lpDirectorInput, mpGuiOutputBuffer, mpDirectorOutputBuffer);
        CheckUpdateMemory(luAvailableMemory, 2177);
        DoUpdate_Effects(mpUpdateInputBufferStack, mpUpdateOutputBufferStack, lpInputOutput,
            lpGameStateOutput, lpWorldOutput, mpDirectorOutputBuffer, lpReplayPreOutput,
            lpSoundPreOutput, lpEffectsOutput, lbSuspendEffects, lUpdateSet);
        CheckUpdateMemory(luAvailableMemory, 2194);
        DoUpdate_Sound(mpUpdateInputBufferStack, mpUpdateOutputBufferStack, lpGameStateOutput,
            lpWorldOutput, mpDirectorOutputBuffer, lpReplayPreOutput, lpSoundOutput,
            mpGuiOutputBuffer, lpEffectsOutput, lUpdateSet);
        CheckUpdateMemory(luAvailableMemory, 2210);
        DoUpdate_ReplaysPostSim(mpUpdateInputBufferStack, mpUpdateOutputBufferStack,
            lpWorldOutput, lpEffectsOutput, lpSoundOutput, mpDirectorOutputBuffer,
            mpGuiOutputBuffer, lpReplayPostOutput, lUpdateSet);
        CheckUpdateMemory(luAvailableMemory, 2225);
        DoUpdate_GameStatePostWorld(mpUpdateInputBufferStack, mpUpdateOutputBufferStack,
            lpInputOutput, lpWorldOutput, mpGuiOutputBuffer, lpReplayPostOutput,
            mpDirectorOutputBuffer, lUpdateSet);
        CheckUpdateMemory(luAvailableMemory, 2239);
        DoUpdate_NetworkPostSim(mpUpdateInputBufferStack, mpUpdateOutputBufferStack,
            lpGameStateOutput, lpWorldOutput, mpGuiOutputBuffer, lUpdateSet);
        CheckUpdateMemory(luAvailableMemory, 2253);
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_GameState_Bridge);
        UpdateTimers(lpGameStateOutput, mpDirectorOutputBuffer);
        CheckUpdateMemory(luAvailableMemory, 2260);
        UpdateFrameRateType(lpGameStateOutput);
        CheckUpdateMemory(luAvailableMemory, 2266);
        const BrnGameState::GameStateModuleIO::OutputBuffer* const lpGameStateRead = lpGameStateOutput;
        const BrnWorldIO::UpdateOutputBuffer* const lpWorldRead = lpWorldOutput;
        const BrnDirector::DirectorIO::OutputBuffer* const lpDirectorRead = mpDirectorOutputBuffer;
        const CgsGui::ModelIO::OutputBuffer* const lpModelRead = mpGuiModelOutputBuffer;
        const BrnSound::Module::Io::RootOutputBuffer* const lpSoundRead = lpSoundOutput;
        const CgsGui::CgsGuiModuleIO::OutputBuffer* const lpGuiRead = mpGuiOutputBuffer;
        const BrnReplays::ReplayIO::OutputBuffer_PreSim* const lpReplayRead = lpReplayPreOutput;
        CgsModule::LockBuffersForIO(lpResourceInput, lpGameStateRead, lpWorldRead,
            lpDirectorRead, lpModelRead, lpSoundRead, lpGuiRead, lpReplayRead);
        BridgeGuiToResource(lpResourceInput, lpModelRead, lpGuiRead);
        BridgeWorldToResource(lpResourceInput, lpWorldRead);
        lpResourceInput->GetAttribSysRequestInterface()->mRequestQueue.Append(
            lpDirectorRead->GetVaultRequestInterface()->mRequestQueue);
        lpResourceInput->AppendRequestInterface<512>(*lpDirectorRead->GetResourceRequestInterface());
        lpResourceInput->AppendRequestInterface<3072>(*lpGameStateRead->GetResourceRequestInterface());
        lpResourceInput->GetAttribSysRequestInterface()->mRequestQueue.Append(
            lpSoundRead->GetAttribSysRequestInterface()->mRequestQueue);
        lpResourceInput->GetRequestInterface()->mRequestQueue.Append(
            lpSoundRead->GetResourceRequestInterface()->mRequestQueue);
        lpResourceInput->AppendRequestInterface<1024>(*lpReplayRead->GetGameDataRequestInterface());
        CgsModule::UnlockBuffersForIO(lpResourceInput, lpGameStateRead, lpWorldRead,
            lpDirectorRead, lpModelRead, lpSoundRead, lpGuiRead, lpReplayRead);
        CheckUpdateMemory(luAvailableMemory, 2281);
        lpInputOutput->LockForRead();
        BridgeInputToGame(lpInputOutput);
        lpInputOutput->UnlockForRead();
        CgsSystem::GetSystemTimerBaseTime();
        CgsSystem::GetSystemTimerFrequency();
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_GameState_Bridge);
        CheckUpdateMemory(luAvailableMemory, 2308);
    }

    // ARTIST 823DC458: the original RendererIO -> WorldDispatch handoff.
    void BrnGameModule::DoDispatch()
    {
        DispatchThreadInputBuffer* const lpDispatch = mDispatchThreadInputBufferManager.GetWriteBuffer();
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_RenderFX);
        mEffectsModule.PreRenderUpdate(lpDispatch);
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_RenderFX);
        lpDispatch->LockForWrite();
        lpDispatch->SetIsStalled(mbStalled);
        lpDispatch->UnlockForWrite();
        if (mbStalled)
            return;
        BrnUpdateSet lUpdateSet = ConstructUpdateSetFromFsm();
        BrnResource::GameDataIO::InputBuffer* const lpResourceInput = mpGameDataModuleInputBuffer;
        const BrnResource::GameDataIO::OutputBuffer* const lpResourceOutput = mpGameDataModuleOutputBuffer;
        CgsModule::IOHelper<BrnWorldIO::DispatchInputBuffer> lWorldInput(mpUpdateInputBufferStack, "WorldDispatch");
        CgsModule::IOHelper<BrnWorldIO::DispatchOutputBuffer> lWorldOutput(mpUpdateOutputBufferStack, "WorldDispatch");
        CgsModule::IOHelper<RendererIO::InputBuffer> lRenderInput(mpUpdateInputBufferStack, "Render");
        CgsModule::IOHelper<RendererIO::OutputBuffer> lRenderOutput(mpUpdateOutputBufferStack, "Render");
        CgsModule::IOHelper<BrnEffects::EffectsIO::DispatchInputBuffer> lEffectsInput(mpUpdateInputBufferStack, "EffectsDispatch");
        RendererIO::InputBuffer* const lpRenderInput = lRenderInput;
        RendererIO::OutputBuffer* const lpRenderOutput = lRenderOutput;
        BrnWorldIO::DispatchInputBuffer* const lpWorldInput = lWorldInput;
        BrnWorldIO::DispatchOutputBuffer* const lpWorldOutput = lWorldOutput;
        BrnEffects::EffectsIO::DispatchInputBuffer* const lpEffectsInput = lEffectsInput;
        lpRenderInput->LockForWrite();
        mpDirectorOutputBuffer->LockForRead();
        lpRenderInput->SetBrnCamera(*mpDirectorOutputBuffer->GetCameraOutput());
        mpDirectorOutputBuffer->UnlockForRead();
        lpRenderInput->UnlockForWrite();
        mpDirectorOutputBuffer->LockForRead();
        lpDispatch->LockForWrite();
        const BrnDirector::Camera::CameraState& lrState = mpDirectorOutputBuffer->GetCameraOutput()->GetState();
        const bool lbRacingCamera = lrState.IsFlagSet(BrnDirector::Camera::CameraState::E_FLAG_RACING_GAMEPLAY_CAMERA);
        const bool lbRoadCamera = mpDirectorOutputBuffer->GetCameraOutput()->GetState().IsFlagSet(
            BrnDirector::Camera::CameraState::E_FLAG_ROAD_FOLLOWING_CAM);
        lpDispatch->SetIsRenderingAtFullFrameRate(lbRacingCamera || lbRoadCamera);
        lpDispatch->UnlockForWrite();
        mpDirectorOutputBuffer->UnlockForRead();
        mRenderModule.Update(mpUpdateInputBufferStack, mpUpdateOutputBufferStack, lpRenderInput, lpRenderOutput);
        lpEffectsInput->LockForWrite();
        lpWorldInput->LockForWrite();
        lpResourceInput->LockForWrite();
        lpRenderOutput->LockForRead();
        mpGuiInputBuffer->LockForWrite();
        BridgeRendererToWorld(lpWorldInput, lpRenderOutput);
        BridgeRendererToGui(mpGuiInputBuffer, lpRenderOutput);
        BridgeRendererToEffects(lpEffectsInput, lpRenderOutput);
        lpResourceInput->SetIm2dDebugRenderBuffer(lpRenderOutput->GetIm2dDebugRenderBuffer());
        lpEffectsInput->UnlockForWrite();
        lpWorldInput->UnlockForWrite();
        lpResourceInput->UnlockForWrite();
        mpGuiInputBuffer->UnlockForWrite();
        lpWorldInput->LockForWrite();
        lpWorldInput->SetDispatchThreadInputBuffer(mDispatchThreadInputBufferManager.GetWriteBuffer());
        lpWorldInput->UnlockForWrite();
        const BrnDirector::DirectorIO::OutputBuffer* const lpDirectorRead = mpDirectorOutputBuffer;
        CgsModule::LockBuffersForIO(lpEffectsInput, lpDirectorRead);
        lpEffectsInput->SetCameraInput(lpDirectorRead->GetCameraOutput());
        CgsModule::UnlockBuffersForIO(lpEffectsInput, lpDirectorRead);
        lpResourceOutput->LockForRead();
        if (*lpResourceOutput->GetLiveUpdateStatus())
            lUpdateSet &= static_cast<BrnUpdateSet>(~0x80u);
        lpResourceOutput->UnlockForRead();
        mWorldModule.GenerateFrustumQueries(mpUpdateInputBufferStack, mpUpdateOutputBufferStack,
            lpWorldInput, lpWorldOutput, &lUpdateSet);
        if (!mbSteppingFrames)
        {
            CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_RenderGUI);
            mGuiModule.Render(mpGuiViewInputBuffer, mpGuiInputBuffer, lpRenderOutput);
            CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_RenderGUI);
        }
        lpDispatch->LockForWrite();
        lpDispatch->SetIsDiskError(mbDiskError);
        lpDispatch->UnlockForWrite();
        mWorldModule.GenerateDispatchLists(mpUpdateInputBufferStack, mpUpdateOutputBufferStack,
            lpWorldInput, lpWorldOutput, &lUpdateSet);
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_RenderFX);
        lpWorldOutput->LockForRead();
        lpEffectsInput->LockForWrite();
        BridgeWorldToEffects_Dispatch(lpEffectsInput, lpWorldOutput);
        lpEffectsInput->UnlockForWrite();
        lpWorldOutput->UnlockForRead();
        lpEffectsInput->LockForRead();
        mEffectsModule.GenerateDispatchLists(mpUpdateInputBufferStack, lpEffectsInput, lpDispatch);
        lpEffectsInput->UnlockForRead();
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_RenderFX);
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_RenderAll);
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_DebugManager);
        mGameDataModule.RenderDebug(lpResourceInput);
        DebugManagerRender(lpRenderOutput);
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_DebugManager);
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_RenderAll);
        mRendererModulePerfmonsForREADONLYAccess = *lpRenderOutput->GetExternallyVisiblePerformanceMonitors();
        lpRenderOutput->UnlockForRead();
    }

    // @ BrnGameModule.cpp:1047 (X360 0x823BC868) - tear down the owned modules + the module base.
    // The X360 first clears a terminated flag (off +10094134) and destructs an off-path collision
    // generator (off +10095440) - both fall in the omitted member ranges of this incremental
    // layout - then calls Destruct() on all 11 engine modules in the order below, the module base,
    // and finally the network module. Reconstructed faithfully (the two off-path subsystem touches
    // are noted, not declared here). Not on the boot/loading path.
    void BrnGameModule::Destruct()
    {
        mbIsSysMenuShowing = false;
        mFrameRateManager.Destruct();
        mEffectsModule.Destruct();
        mSoundModule.Destruct();
        mDirectorModule.Destruct();
        mRenderModule.Destruct();
        mGameStateModule.Destruct();
        mGuiModule.Destruct();
        mInputModule.Destruct();
        mWorldModule.Destruct();
        mReplayModule.Destruct();
        mGameDataModule.Destruct();
        CgsModule::ModuleSingleBuffered::Destruct();
        mNetworkModule.Destruct();
    }

    // @ BrnGameModule.cpp:925 (X360 0x823CB2A8) - resumable staged release (counterpart of
    // Prepare). meReleaseStage is the resume point; a failed sub-release returns false so the
    // caller re-drives from the same stage. The release SEQUENCE is GUI -> sound -> hardware
    // (+input) -> game-data -> module base -> network -> done; the cases are written in that
    // execution order with fallthrough (the same resumable-switch shape as the module base's
    // ModuleSingleBuffered::Release), so entering at any stage runs the rest. Note the enum
    // values along that path are not monotonic (HARDWARE = 5 sits between SOUND = 2 and
    // GAMEDATAMODULE = 3); C++ fallthrough follows source order, not case value, so this is
    // exact - the Hex-Rays output only used gotos because its jump table sorts by case value.
    // Off-path touches (the System360HW release + two terminated-flag fields) are in this
    // layout's omitted ranges and noted. Not on the boot/loading path.
    bool BrnGameModule::Release()
    {
        switch (meReleaseStage)
        {
        case E_RELEASESTAGE_START:
        case E_RELEASESTAGE_GUI: goto release_gui;
        case E_RELEASESTAGE_SOUND: goto release_sound;
        case E_RELEASESTAGE_GAMEDATAMODULE: goto release_game_data;
        case E_RELEASESTAGE_MANAGER: goto release_manager;
        case E_RELEASESTAGE_HARDWARE: goto release_hardware;
        case E_RELEASESTAGE_NETWORK: goto release_network;
        case E_RELEASESTAGE_DONE: goto release_done;
        default:
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("0",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnGameModule.cpp", 1033);
            CgsDev::Assert::EndAssert();
            return false;
        }
    release_gui:
        meReleaseStage = E_RELEASESTAGE_GUI;
        if (!mGuiModule.Release()) return false;
    release_sound:
        meReleaseStage = E_RELEASESTAGE_SOUND;
        if (!mSoundModule.Release()) return false;
    release_hardware:
        meReleaseStage = E_RELEASESTAGE_HARDWARE;
        if (!mHardware.Release() || !mInputModule.Release()) return false;
    release_game_data:
        meReleaseStage = E_RELEASESTAGE_GAMEDATAMODULE;
        if (!mGameDataModule.Release()) return false;
        meReleaseStage++;
    release_manager:
        meReleaseStage = E_RELEASESTAGE_MANAGER;
        if (!CgsModule::ModuleSingleBuffered::Release()) return false;
        meReleaseStage++;
    release_network:
        if (!mNetworkModule.Release()) return false;
    release_done:
        mbIsSysMenuShowing = false;
        meReleaseStage = E_RELEASESTAGE_DONE;
        mePrepareStage = E_PREPARESTAGE_START;
        return true;
    }

    // operator++(EReleaseStage&, int) @ 0x823A8AD8 - POST-increment for the EReleaseStage stage
    // machine, used by Release() to advance meReleaseStage. X360 body: v1 = *a1; *a1 = *a1 + 1;
    // if (*a1 > 7) assert; return v1 -- save the old value, advance the stage, assert the advanced
    // value is still <= E_RELEASESTAGE_DONE (=7), and return the OLD value.
    //
    // FLAG (static-message assert): the X360 fires the assert with the literal expression string
    // "leEnumIndex <= BrnGameModule::E_RELEASESTAGE_DONE" at BrnGameModule.h:1443. We use the
    // committed CGS_ASSERT macro with that same expression text (the macro supplies __FILE__ /
    // __LINE__); the original's BrnGameModule.h:1443 site is NOT baked in.
    BrnGameModule::EReleaseStage operator++(BrnGameModule::EReleaseStage& leStage, int)
    {
        BrnGameModule::EReleaseStage leOld = leStage;
        leStage = static_cast<BrnGameModule::EReleaseStage>(leStage + 1);
        if (leStage > BrnGameModule::E_RELEASESTAGE_DONE)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("leEnumIndex <= BrnGameModule::E_RELEASESTAGE_DONE",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnGameModule.h", 1443);
            CgsDev::Assert::EndAssert();
        }
        return leOld;
    }

    // ARTIST823EFBD0; DecFIGS329400 confirms bool GamePrepare().
    // The original receiver-count gates are three bundle completions, then five
    // acquire replies. The six acquire ids include the separately retained font.
    bool BrnGameModule::GamePrepare()
    {
        BrnResource::GameDataIO::InputBuffer* const lpResourceInput = mpGameDataModuleInputBuffer;
        const BrnResource::GameDataIO::OutputBuffer* const lpResourceOutput = mpGameDataModuleOutputBuffer;
        lpResourceInput->LockForWrite();
        lpResourceOutput->LockForRead();
        switch (meGamePrepareStage)
        {
        case E_GAMEPREPARESTAGE_START:
        case E_GAMEPREPARESTAGE_LOADBUNDLES:
            meGamePrepareStage = E_GAMEPREPARESTAGE_LOADBUNDLES;
            lpResourceInput->GetRequestInterface()->LoadBundle(
                &mGamePrepareReceiverQueue, 2, 10, "shaders.bndl", true);
            lpResourceInput->GetRequestInterface()->LoadBundle(
                &mGamePrepareReceiverQueue, 1, 10, "GlobalTextureDictionary.bin", false);
            lpResourceInput->GetRequestInterface()->LoadBundle(
                &mGamePrepareReceiverQueue, 0, 0, "Language\\Fonts\\Default.font", false);
        case E_GAMEPREPARESTAGE_WAITBUNDLES:
            meGamePrepareStage = E_GAMEPREPARESTAGE_WAITBUNDLES;
            if (mGamePrepareReceiverQueue.GetCount() < 3)
                break;
            mGamePrepareReceiverQueue.Clear();
            {
                static const char* const kapcResourceNames[] =
                {
                    "gamedb://burnout5/Playground/GlobalTextures/blobbyshadow.TextureConfig2d?ID=240931",
                    "gamedb://burnout5/Playground/GlobalTextures/cloud1density.TextureConfig2d?ID=381388",
                    "gamedb://burnout5/Playground/GlobalTextures/cloud1quadrant.TextureConfig2d?ID=381382",
                    "gamedb://burnout5/Playground/GlobalTextures/corona_atlas.TextureConfig2d?ID=297312",
                    "gamedb://burnout5/Playground/GlobalTextures/glass_fracture.TextureConfig2d?ID=440917",
                    "default"
                };
                CgsResource::Events::AcquireResourceRequest lRequest;
                for (s32 liEventId = 0; liEventId < 6; ++liEventId)
                {
                    BrnResource::GameDataIO::RequestInterface<32768>* const lpRequests =
                        lpResourceInput->GetRequestInterface();
                    const u32 luHash = static_cast<u32>(CgsResource::ID::HashString(
                        reinterpret_cast<const u8*>(kapcResourceNames[liEventId])));
                    lRequest.mpUser = &mGamePrepareReceiverQueue;
                    lRequest.miEventId = liEventId;
                    lRequest.miPoolId = liEventId == 5 ? 0 : 10;
                    lRequest.mResourceId.SetHash(static_cast<u64>(luHash));
                    lpRequests->mRequestQueue.AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lRequest), 4, sizeof(lRequest));
                }
            }
        case E_GAMEPREPARESTAGE_WAITACQUIRES:
            meGamePrepareStage = E_GAMEPREPARESTAGE_WAITACQUIRES;
            if (mGamePrepareReceiverQueue.GetCount() < 5)
                break;
            {
                const renderengine::Texture* lapTextures[5] = { 0, 0, 0, 0, 0 };
                const CgsModule::Event* lpEvent;
                s32 liSize;
                s32 liType = mGamePrepareReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
                while (lpEvent)
                {
                    if (liType == 4)
                    {
                        const CgsResource::Events::AcquireResourceResponse* const lpResponse =
                            reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpEvent);
                        switch (lpResponse->miEventId)
                        {
                        case 0:
                        case 1:
                        case 2:
                        case 3:
                        case 4:
                            lapTextures[lpResponse->miEventId] = static_cast<const renderengine::Texture*>(
                                static_cast<const CgsResource::SmallResource*>(lpResponse->mpResourceMemory)
                                    ->GetMemoryResource());
                            break;
                        case 5:
                            {
                                mDebugFont.mpSourceEntry = lpResponse->mpSourceEntry;
                                mDebugFont.mpResourceMemory = lpResponse->mpResourceMemory;
                                rw::IResourceAllocator* const lpAllocator =
                                    rw::ResourceAllocatorRegistry::GetDefaultAllocator();
                                mDebugFont->CreateTextureState(lpAllocator);
                                mDebugManager.SetDebugFont(mDebugFont);
                            }
                            break;
                        default:
                            CgsDev::Assert::BeginAssert();
                            CgsDev::Assert::FireAssert("Unexpected Event Id in BrnGameModule::GamePrepare",
                                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnGameModule.cpp", 1741);
                            CgsDev::Assert::EndAssert();
                            break;
                        }
                    }
                    liType = mGamePrepareReceiverQueue.GetNextEvent(lpEvent, &lpEvent, &liSize);
                }
                static const char* const kapcTextureAssertions[] =
                {
                    "lpBlobbyShadow", "lpCloudDensityTexture", "lpCloudLightingTexture",
                    "lpCoronaAtlas != NULL", "lpGlassFractureTexture != NULL"
                };
                for (s32 liTexture = 0; liTexture < 5; ++liTexture)
                {
                    if (!lapTextures[liTexture])
                    {
                        CgsDev::Assert::BeginAssert();
                        CgsDev::Assert::FireAssert(kapcTextureAssertions[liTexture],
                            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnGameModule.cpp", 1751 + liTexture);
                        CgsDev::Assert::EndAssert();
                    }
                }
                if (!mRenderModule.PrepareAgain(lapTextures[0], lapTextures[1], lapTextures[2],
                                               lapTextures[3], lapTextures[4]))
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert("mRenderModule.PrepareAgain ( lpBlobbyShadow, lpCloudDensityTexture, lpCloudLightingTexture, lpCoronaAtlas, lpGlassFractureTexture )",
                        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnGameModule.cpp", 1761);
                    CgsDev::Assert::EndAssert();
                }
                mGamePrepareReceiverQueue.Clear();
            }
        case E_GAMEPREPARESTAGE_GAMESTATE:
            {
                meGamePrepareStage = E_GAMEPREPARESTAGE_GAMESTATE;
                BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput;
                mpUpdateOutputBufferStack->CreateIOBuffer(&lpGameStateOutput, "GameState");
                if (mGameStateModule.Prepare(lpGameStateOutput, mpUpdateOutputBufferStack,
                                           lpResourceOutput->GetAllocatorList()))
                {
                    mpUpdateOutputBufferStack->DestroyIOBuffer(&lpGameStateOutput);
                    lpResourceInput->UnlockForWrite();
                    lpResourceOutput->UnlockForRead();
                    return true;
                }
                lpGameStateOutput->LockForRead();
                const BrnGameState::GameStateModuleIO::OutputBuffer* const lpRead = lpGameStateOutput;
                lpResourceInput->AppendRequestInterface<3072>(*lpRead->GetResourceRequestInterface());
                lpGameStateOutput->UnlockForRead();
                mpUpdateOutputBufferStack->DestroyIOBuffer(&lpGameStateOutput);
            }
            break;
        default:
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Got into an unknown state",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnGameModule.cpp", 1807);
            CgsDev::Assert::EndAssert();
            break;
        }

        // ARTIST823F021C..03B8 is the not-ready path, including the original
        // IOHelper assertion sites and reverse destruction after CheckDiskError.
        RendererIO::InputBuffer* lpRendererInput;
        RendererIO::OutputBuffer* lpRendererOutput;
        if (!mpUpdateInputBufferStack->CreateIOBuffer(&lpRendererInput, "Render"))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mpStack->CreateIOBuffer( &mpBuffer, lpcName )",
                "..\\..\\..\\GameShared\\GameClasses\\Module/CgsModuleIOHelper.h", 52);
            CgsDev::Assert::EndAssert();
        }
        if (!mpUpdateOutputBufferStack->CreateIOBuffer(&lpRendererOutput, "Render"))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mpStack->CreateIOBuffer( &mpBuffer, lpcName )",
                "..\\..\\..\\GameShared\\GameClasses\\Module/CgsModuleIOHelper.h", 52);
            CgsDev::Assert::EndAssert();
        }
        mRenderModule.Update(mpUpdateInputBufferStack, mpUpdateOutputBufferStack,
                             lpRendererInput, lpRendererOutput);
        lpRendererOutput->LockForRead();
        mpReusableLoadingScreenAllocator = lpRendererOutput->GetReusableLoadingScreenAllocator();
        lpRendererOutput->UnlockForRead();
        DispatchThreadInputBuffer* const lpDispatchInput = mDispatchThreadInputBufferManager.GetWriteBuffer();
        lpDispatchInput->LockForWrite();
        lpDispatchInput->ShowLoadingScreen();
        lpDispatchInput->SetIsDiskError(mbDiskError);
        lpDispatchInput->UnlockForWrite();
        lpResourceInput->UnlockForWrite();
        lpResourceOutput->UnlockForRead();
        CheckDiskError(lpResourceOutput);
        if (!mpUpdateOutputBufferStack->DestroyIOBuffer(&lpRendererOutput))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mpStack->DestroyIOBuffer( &mpBuffer )",
                "..\\..\\..\\GameShared\\GameClasses\\Module/CgsModuleIOHelper.h", 57);
            CgsDev::Assert::EndAssert();
        }
        if (!mpUpdateInputBufferStack->DestroyIOBuffer(&lpRendererInput))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mpStack->DestroyIOBuffer( &mpBuffer )",
                "..\\..\\..\\GameShared\\GameClasses\\Module/CgsModuleIOHelper.h", 57);
            CgsDev::Assert::EndAssert();
        }
        return false;
    }

    // ARTIST823F03C8: resumable native module game-release sequence.
    bool BrnGameModule::GameRelease()
    {
        BrnResource::GameDataIO::InputBuffer* const lpResourceInput = mpGameDataModuleInputBuffer;
        const BrnResource::GameDataIO::OutputBuffer* const lpResourceOutput = mpGameDataModuleOutputBuffer;
        lpResourceInput->LockForWrite();
        lpResourceOutput->LockForRead();
        bool lbReleased = false;
        switch (meGameReleaseStage)
        {
        case E_GAMERELEASESTAGE_START:
        case E_GAMERELEASESTAGE_NETWORK:
            meGameReleaseStage = E_GAMERELEASESTAGE_NETWORK;
            if (!mNetworkModule.Release()) break;
        case E_GAMERELEASESTAGE_SOUND_MODULE:
            meGameReleaseStage = E_GAMERELEASESTAGE_SOUND_MODULE;
            if (!mSoundModule.Release()) break;
        case E_GAMERELEASESTAGE_DIRECTORMODULE:
            meGameReleaseStage = E_GAMERELEASESTAGE_DIRECTORMODULE;
            if (!mDirectorModule.Release()) break;
        case E_GAMERELEASESTAGE_GUIMODULE:
            meGameReleaseStage = E_GAMERELEASESTAGE_GUIMODULE;
            if (!mGuiModule.Release()) break;
        case E_GAMERELEASESTAGE_CONTROLLERMODULE:
            meGameReleaseStage = E_GAMERELEASESTAGE_CONTROLLERMODULE;
            if (!mInputModule.Release()) break;
        case E_GAMERELEASESTAGE_WORLDMODULE:
            {
                meGameReleaseStage = E_GAMERELEASESTAGE_WORLDMODULE;
                BrnWorldIO::UpdateOutputBuffer* lpWorldOutput = 0;
                mpUpdateOutputBufferStack->CreateIOBuffer(&lpWorldOutput, "World");
                if (!mWorldModule.Release(mpUpdateOutputBufferStack, lpWorldOutput))
                {
                    BridgeWorldToResource(lpResourceInput, lpWorldOutput);
                    mpUpdateOutputBufferStack->DestroyIOBuffer(&lpWorldOutput);
                    break;
                }
                mpUpdateOutputBufferStack->DestroyIOBuffer(&lpWorldOutput);
            }
        case E_GAMERELEASESTAGE_RENDERMODULE:
            meGameReleaseStage = E_GAMERELEASESTAGE_RENDERMODULE;
            if (!mRenderModule.Release()) break;
        case E_GAMERELEASESTAGE_GAMESTATEMODULE:
            {
                meGameReleaseStage = E_GAMERELEASESTAGE_GAMESTATEMODULE;
                BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput = 0;
                mpUpdateOutputBufferStack->CreateIOBuffer(&lpGameStateOutput, "GameState");
                if (!mGameStateModule.Release(lpGameStateOutput))
                {
                    const BrnGameState::GameStateModuleIO::OutputBuffer* const lpRead = lpGameStateOutput;
                    lpResourceInput->AppendRequestInterface<3072>(*lpRead->GetResourceRequestInterface());
                    mpUpdateOutputBufferStack->DestroyIOBuffer(&lpGameStateOutput);
                    break;
                }
                mpUpdateOutputBufferStack->DestroyIOBuffer(&lpGameStateOutput);
            }
        case E_GAMERELEASESTAGE_REPLAYMODULE:
            meGameReleaseStage = E_GAMERELEASESTAGE_REPLAYMODULE;
            if (!mReplayModule.Release()) break;
        case E_GAMERELEASESTAGE_DONE:
            meGamePrepareStage = E_GAMEPREPARESTAGE_START;
            meGameReleaseStage = E_GAMERELEASESTAGE_DONE;
            lbReleased = true;
            break;
        default:
            CgsDev::Assert::BeginAssert();
            {
                CgsDev::StrStream lMessage(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
                lMessage << "Invalid game release stage\n";
                CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnGameModule.cpp", 2809);
            }
            CgsDev::Assert::EndAssert();
            break;
        }
        lpResourceInput->UnlockForWrite();
        lpResourceOutput->UnlockForRead();
        return lbReleased;
    }

    // @ BrnGameModule.cpp:1253 (X360 0x823A8BB0) - start-of-update-frame hook. The X360 body is
    // exactly two statements:
    //   1. EffectsModule::StartOfFrame -> ParticleModule::StartOfFrame (DWARF _compile/BrnGameUnity.cpp:392-395
    //      names the call; both are inline, EffectsModule.h:277 / ParticleModule.h:309), compiled to ONE store:
    //      `lis r10, 0x88 ; ori r9, r10, 0x194C ; lfs f0, flt_82001CC0 (0.0) ; stfsx f0, r11, r9`
    //      (0x823A8BB0..0x823A8BC8) -- *(this + 0x88194C) = 0.0f. The particle module is this + 0x878B40
    //      (OnEndOfUpdateFrame hands ParticleModule::EndOfFrame exactly that: `addis r3, r30, 0x88 ; addi r3, r3,
    //      -0x74C0` at 0x823DC378), so the store is its mRenderData.mfCurrentTimeStep (+0x8E0C): the clear of the
    //      sum ParticleModule::Update builds over the frame's sim sub-steps (`+= rate * step`,
    //      0x8228185C..0x82281870). Every reader of the published record -- the spark ring, the trails, the
    //      debris jobs, the motion blur -- gets the frame's step, and 0.0 on a frame that ran no sub-step.
    //   2. `BrnRendererModule::StartOfFrame(this + 15808)` (the tail call at 0x823A8BCC): it rewinds the
    //      game-side dispatch-list ring the world modules fill this frame and opens the shader-constant table's
    //      frame on its bin.
    // ⛔ CORRECTED 2026-09-25 (FX-CRASHVFX): this banner used to read the store as "the frame lookback timer ...
    // not in this layout yet" and the body dropped it. On PC the particle step then grew from boot: the debris
    // integrated 21.8 s steps (scratch/bugtest/runs/fxcrashvfx_glass/20260925_113502), the spark ring, the trails
    // and the debris each carried a first-difference workaround, and the motion blur's zero-step arm never ran.
    // The PC calls this once per UPDATE frame, ahead of that frame's sub-steps, as the console's ThreadLayout does:
    // BrnMain.cpp's EngineUpdate loop runs OnStartOfUpdateFrame -> OnCompletionOfVsyncWait -> UpdateThread
    // (GameMain's `do { ... DoUpdate_Effects ... } while (liStep < miNumSimFramesRequired)`, then the flow
    // state's Render -> DoDispatch, which publishes the record) -> OnEndOfUpdateFrame -> DispatchThread.
    void BrnGameModule::OnStartOfUpdateFrame()
    {
        mEffectsModule.StartOfFrame();   // *(this + 0x88194C) = 0.0f  (0x823A8BB0..0x823A8BC8)
        mRenderModule.StartOfFrame();    // the tail call (0x823A8BCC)
    }

    // ARTIST 823DBBA0. This entry receives the IThreadClass subobject at
    // GameModule+0x640 (ctor 827E5F68..70, vtable820D1498); its two stall
    // bytes are the actual mbStalled and mbPrevStalled, after that base shift.
    void BrnGameModule::OnEndOfUpdateFrame()
    {
        const bool lbStalled = mbStalled || mbPrevStalled;
        mEffectsModule.ParticleModuleRef().EndOfFrame(lbStalled);

        mDispatchThreadInputBufferManager.Swap();

        // ⭐⭐⭐ THE GUI END-OF-FRAME NOTIFY, restored 2026-09-07 -- the SIBLING of the
        // particle hole above, named by the very same stale banner line, and found the same
        // way: `BrnGui::GuiModule::EndOfFrame @0x824F1008` was DECLARED NOWHERE, DEFINED
        // NOWHERE and CALLED BY NOTHING in this tree, which links in silence. The console
        // calls it HERE, after the dispatch swap and before the renderer's own end-of-frame,
        // exactly as the pseudocode above shows.
        //
        // It is three tail calls: GuiModule::EndOfFrame -> CustomRendererManager::EndOfFrame
        // (+311952) -> NetworkPlayerImageRenderer::SwapBuffers (+112192), and it is the ONLY
        // writer of that renderer's two ring cursors. Frozen, they sit at render=1 / copyTo=2
        // for the life of the process, so a picture RecvEvent copies into slot 2 is read back
        // from slot 1 and can never appear. Full derivation on the body in BrnGuiModule.cpp.
        mGuiModule.EndOfFrame();   // X360 @0x823DBBA0 calls it HERE, before mRenderModule's

        mRenderModule.EndOfFrame(lbStalled);
        CgsDev::PerfMonGpu::Swap(); // ARTIST823DC3F4, after renderer EndOfFrame.
        mbPrevStalled = mbStalled;
    }

    void BrnGameModule::ResourceUpdateThread(Mutex* /*lpMutex*/)
    {
        mbStalled = mGameDataModule.Update(mpResourceInputBufferStack, mpResourceOutputBufferStack,
            mpGameDataModuleInputBuffer, mpGameDataModuleOutputBuffer) ==
            BrnResource::GameDataModule::E_RESULT_REQUIRE_STALL;
        CheckDiskError(mpGameDataModuleOutputBuffer);
    }

    // @ BrnGameModule.cpp:1221 - the render/dispatch thread body. The threaded D3D
    // thread-ownership handshake and the per-module dispatch are reconstructed with the
    // threading core; for the boot loading screen this drives one render through the renderer
    // module.
    void BrnGameModule::DispatchThread()
    {
        D3DDevice_AcquireThreadOwnership(gpD3DDevice);
        mReplayModule.Update_Dispatch();
        mRenderModule.Render(&mEffectsModule, mDispatchThreadInputBufferManager.GetReadBuffer());
        D3DDevice_ReleaseThreadOwnership(gpD3DDevice);
    }

    // ARTIST 823BCB88. The caller holds fresh RendererIO output for read;
    // all debug commands are recorded during the original producer phase.
    void BrnGameModule::DebugManagerRender(RendererIO::OutputBuffer* lpRendererOutput)
    {
        CgsDev::PerfMonCpu::AddPIXCounters();
        CgsGraphics::Im3dRenderBuffer* const lpIm3dDebug = lpRendererOutput->GetIm3dDebugRenderBuffer();
        CgsGraphics::Im2dRenderBuffer* const lpIm2dDebug = lpRendererOutput->GetIm2dDebugRenderBuffer();

        // 3. the gate (@0x823BCBCC-C20): the debug font must have arrived (GamePrepare's id-5
        // acquire writes the pair; both words NULL = not yet).
        if (mDebugFont.mpResourceMemory == 0 && mDebugFont.mpSourceEntry == 0)
            return;
        if (lpIm3dDebug == 0 || lpIm2dDebug == 0)
            return;

        // 4. the fps state (@0x823BCC24-D44).
        const u64 luNow  = CgsSystem::GetSystemTimerBaseTime();
        const u64 luFrequency = CgsSystem::GetSystemTimerFrequency();

        const f32 lfThreeFrameSeconds =
            static_cast<f32>(luNow - mau64DebugFpsFrameStamps[mu8DebugFpsFrameStampIndex]) /
            static_cast<f32>(luFrequency);
        const f32 lfCurrentFps = 1.0f / (lfThreeFrameSeconds * 0.33333334f);   // flt_820065E0

        mfDebugFpsSampleCount = mfDebugFpsSampleCount + 1.0f;
        mfDebugFpsAccumulator = mfDebugFpsAccumulator + lfCurrentFps;
        mau64DebugFpsFrameStamps[mu8DebugFpsFrameStampIndex] = luNow;
        mu8DebugFpsFrameStampIndex = static_cast<u8>((mu8DebugFpsFrameStampIndex + 1) % 3);

        const f32 lfWindowSeconds =
            static_cast<f32>(luNow - mu64DebugFpsAverageWindowStamp) / static_cast<f32>(luFrequency);
        if (lfWindowSeconds > 60.0f)   // flt_82004C6C
        {
            mu64DebugFpsAverageWindowStamp = luNow;
            mfDebugFpsAverage     = mfDebugFpsAccumulator / mfDebugFpsSampleCount;
            mfDebugFpsAccumulator = 0.0f;
            mfDebugFpsSampleCount = 0.0f;
        }

        // 5. queue the overlay under the debug lock (@0x823BCD48: Enter(dword_83019264) -- the
        // same per-manager section ThreadSafeAquire brackets).
        CgsDev::DebugManager* lpDebugManager = CgsDev::DebugManager::ThreadSafeAquire();

        lpDebugManager->RenderBuildInfo();

        // highlight = clamp(1 - secondsSinceAverage/30, 0, 1) (@0x823BCE7C-EE8, flt_82037210).
        f32 lfAverageHighlight = 1.0f - lfWindowSeconds * 0.033333335f;
        if (lfAverageHighlight < 0.0f) lfAverageHighlight = 0.0f;
        if (lfAverageHighlight > 1.0f) lfAverageHighlight = 1.0f;

        lpDebugManager->RenderFrameRateColouredWithAverage(
            lfCurrentFps, mfDebugFpsAverage,
            0xFF00FF00u /*high: green*/, 0xFF0000FFu /*low: red*/, 0xFF00FFFFu /*mid: yellow*/,
            60.0f, 30.0f,   // flt_82004C6C / flt_82004F5C
            "over last minute", lfAverageHighlight, mFrameRateManager.mbIsSimulationRunningInRealTime);

        lpDebugManager->RenderMemory();

        // ARTIST 823BCD48..CF14 inlines the general affine inverse of the
        // director module's actual graphics view, then records both debug banks.
        const CgsGraphics::Camera& lrCamera = mDirectorModule.GetGraphicsCameraForDebug();
        const Matrix44& lrView = lrCamera.mView;
        Matrix44Affine lView;
        {
            const Vector3 lAggregateValue = {lrView.xAxis.x, lrView.xAxis.y, lrView.xAxis.z, lrView.xAxis.w};
            lView.xAxis = lAggregateValue;
        }
        {
            const Vector3 lAggregateValue = {lrView.yAxis.x, lrView.yAxis.y, lrView.yAxis.z, lrView.yAxis.w};
            lView.yAxis = lAggregateValue;
        }
        {
            const Vector3 lAggregateValue = {lrView.zAxis.x, lrView.zAxis.y, lrView.zAxis.z, lrView.zAxis.w};
            lView.zAxis = lAggregateValue;
        }
        {
            const Vector3 lAggregateValue = {lrView.wAxis.x, lrView.wAxis.y, lrView.wAxis.z, lrView.wAxis.w};
            lView.wAxis = lAggregateValue;
        }
        const Matrix44Affine lInverseView = rw::math::vpu::Inverse(lView);
        lpDebugManager->Render(lrCamera.GetViewProjectionMatrix(), lInverseView.wAxis,
                               lpIm3dDebug, lpIm2dDebug);

        CgsDev::DebugManager::ThreadSafeRelease(lpDebugManager);
    }

    // @ BrnGameModule.cpp:3916 - clear the whole game-module object, then stamp each owned
    // module's memory region with 0x7FFFFFFF so any read of un-prepared module memory is caught.
    // ARTIST main calls this before placement construction. The native target
    // must recover the actual layout before these original writes can run.
    void BrnGameModule::DebugMemoryInit(BrnGameModule* lpData)
    {
#if defined(_XBOX)
        typedef char NativeGameModuleSize[sizeof(BrnGameModule) == 0x9A1300 ? 1 : -1];
        typedef char NativeGameStateOffset[offsetof(BrnGameModule, mGameStateModule) == 0x669500 ? 1 : -1];
        typedef char NativeGuiOffset[offsetof(BrnGameModule, mGuiModule) == 0x6EAA20 ? 1 : -1];
        typedef char NativeNetworkOffset[offsetof(BrnGameModule, mNetworkModule) == 0x8C3600 ? 1 : -1];
        typedef char NativeInputOffset[offsetof(BrnGameModule, mInputModule) == 0x6E9630 ? 1 : -1];
#endif
        std::memset(lpData, 0, 0x9A1300);
        DebugSetMemoryToInt(&lpData->mGameStateModule, 292368, 0x7FFFFFFF);
        DebugSetMemoryToInt(&lpData->mGuiModule, 1629392, 0x7FFFFFFF);
        DebugSetMemoryToInt(&lpData->mNetworkModule, 866560, 0x7FFFFFFF);
        DebugSetMemoryToInt(&lpData->mInputModule, 5096, 0x7FFFFFFF);
    }

    // @ BrnGameModule.cpp:1845 - the per-frame update spine.
    //
    // Latches this frame's simulation-step bounds (forcing a single step when single-stepping
    // frames), begins the frame-rate manager's frame, releases the input read-lock, ticks the
    // debug manager and polls the step-frame request. Then it runs the active flow state's
    // Update() once per required simulation sub-step (recreating the static IO buffers between
    // steps), and finally the active state's Render() once for the frame, before tearing down
    // this frame's GUI/director IO buffers. Returns false (the X360 returns 0).
    //
    // The mCpuMonitors.* handles are the CPU perfmon slots the original brackets each region
    // with; the field names follow the BrnCpuMonitors declaration order at the X360 offsets.
    bool BrnGameModule::GameMain()
    {
        using namespace CgsDev;
        PerfMonCpu::StartMonitor(mCpuMonitors.miUT_TotalUpdate);
        mi8ActualFrameRateMinStepsThisFrame = mi8FrameRateMinSteps;
        mi8ActualFrameRateMaxStepsThisFrame = mi8FrameRateMaxSteps;
        CgsSystem::EFrameRateManagerType leFrameRateType = meFrameRateManagerType;
        if (mbSteppingFrames)
        {
            leFrameRateType = CgsSystem::E_FRAMERATEMANAGER_SINGLE;
            mi8ActualFrameRateMinStepsThisFrame = 1;
            mi8ActualFrameRateMaxStepsThisFrame = 1;
        }
        if (_lbForceAssert)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("!_lbForceAssert",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnGameModule.cpp", 1872);
            CgsDev::Assert::EndAssert();
        }
        mFrameRateManager.StartUpdateFrame(leFrameRateType, false);
        UnlockInputForRead();
        PerfMonCpu::StopMonitor(mCpuMonitors.miUT_TotalUpdate);
        PerfMonCpu::StartMonitor(mCpuMonitors.miUT_DebugManager);
        mDebugManager.Update(mGameTimer.GetScaleCurrent() * mGameTimer.GetRate());
        UpdateRequestDoStepFrame();
        PerfMonCpu::StopMonitor(mCpuMonitors.miUT_DebugManager);
        PerfMonCpu::StartMonitor(mCpuMonitors.miUT_TotalUpdate);
        PerfMonCpu::SetNumIterationsTaken(miNumSimFramesRequired);
        sbSimUpdateComplete = 0;
        for (s32 liStep = 0; liStep < miNumSimFramesRequired; ++liStep)
        {
            PerfMonCpu::StartMonitor(mCpuMonitors.miUT_GameState);
            CreateStaticIOBuffers();
            mFrameRateManager.miPrevNumSimulationStepsRequired = liStep + 1;
            PerfMonCpu::StopMonitor(mCpuMonitors.miUT_GameState);
            PerfMonCpu::StartMonitor(mCpuMonitors.miUT_EachUpdate);
            const BrnGameMainFlowController::EMainGameFlowState leState =
                mMainFlowStateMachine.GetCurrentState();
            if (leState != BrnGameMainFlowController::E_MGS_INVALID)
                mMainFlowStateMachine.GetState(leState)->Update();
            PerfMonCpu::StopMonitor(mCpuMonitors.miUT_EachUpdate);
            if (liStep != miNumSimFramesRequired - 1)
            {
                PerfMonCpu::StartMonitor(mCpuMonitors.miUT_GameState);
                DestroyStaticIOBuffers();
                PerfMonCpu::StopMonitor(mCpuMonitors.miUT_GameState);
            }
        }
        PerfMonCpu::StopMonitor(mCpuMonitors.miUT_TotalUpdate);
        sbSimUpdateComplete = 1;
        PerfMonCpu::StartMonitor(mCpuMonitors.miUT_RenderAll);
        const BrnGameMainFlowController::EMainGameFlowState leState =
            mMainFlowStateMachine.GetCurrentState();
        if (leState != BrnGameMainFlowController::E_MGS_INVALID)
            mMainFlowStateMachine.GetState(leState)->Render();
        DestroyStaticIOBuffers();
        PerfMonCpu::StopMonitor(mCpuMonitors.miUT_RenderAll);
        return false;
    }

    // @ BrnGameModule.cpp:2497 - allocate this sub-step's static GUI/director IO buffers from
    // the update input/output buffer stacks.
    // ARTIST823DCD78. The scheduler continues to use its existing host
    // route; this recovered native caller has no selected invocation yet.
    void BrnGameModule::DoUpdate_GuiPreWorld(
        CgsModule::IOBufferStack* lpInputBufferStack,
        CgsModule::IOBufferStack* lpOutputBufferStack,
        CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput,
        CgsGui::ModelIO::OutputBuffer* lpModelOutput,
        BrnResource::GameDataIO::InputBuffer* lpGameDataInput)
    {
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_GUI);
        if (!mbDiskError)
            mGuiModule.PreWorldUpdate(lpInputBufferStack, lpOutputBufferStack,
                lpGuiOutput, lpModelOutput);
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_GUI);

        const CgsGui::ModelIO::OutputBuffer* lpModelRead = lpModelOutput;
        const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiRead = lpGuiOutput;
        CgsModule::LockBuffersForIO(lpGameDataInput, lpModelRead, lpGuiRead);
        lpGameDataInput->GetRequestInterface()->mRequestQueue.Append(
            *lpGuiRead->GetGuiResourceRequestQueue());
        CgsModule::UnlockBuffersForIO(lpGameDataInput, lpModelRead, lpGuiRead);
    }

    // ARTIST823BCA58: five persistent per-frame pointers, created in this order.
    void BrnGameModule::CreateStaticIOBuffers()
    {
        mpUpdateInputBufferStack->CreateIOBuffer(&mpGuiInputBuffer, "Gui");
        mpUpdateInputBufferStack->CreateIOBuffer(&mpGuiViewInputBuffer, "GuiView");
        mpUpdateOutputBufferStack->CreateIOBuffer(&mpGuiOutputBuffer, "Gui");
        mpUpdateOutputBufferStack->CreateIOBuffer(&mpGuiModelOutputBuffer, "GuiModel");
        mpUpdateOutputBufferStack->CreateIOBuffer(&mpDirectorOutputBuffer, "Director");
    }

    // ARTIST823BCAF8: destroy the five objects in reverse creation order.
    void BrnGameModule::DestroyStaticIOBuffers()
    {
        mpUpdateOutputBufferStack->DestroyIOBuffer(&mpDirectorOutputBuffer);
        mpUpdateOutputBufferStack->DestroyIOBuffer(&mpGuiModelOutputBuffer);
        mpUpdateOutputBufferStack->DestroyIOBuffer(&mpGuiOutputBuffer);
        mpUpdateInputBufferStack->DestroyIOBuffer(&mpGuiViewInputBuffer);
        mpUpdateInputBufferStack->DestroyIOBuffer(&mpGuiInputBuffer);
    }

    // =========================================================================================
    // ⭐ [FX-RUMBLE3 2026-09-24, crash-parity G10-D4] DoUpdate_InputPreWorld -- X360 0x823C5650
    // (DWARF BrnGameModule.h:550; BrnGameModuleUpdateFunctions.cpp:45, locals
    // `IOHelper<PreWorldInputBuffer> lpInputModulePreWorldInputBuffer`, `uint32_t liStartPressedPort`).
    // The first leg of every update step, and the ONLY place the game's rumble requests reach the
    // pads: until it existed RumbleManager's four queues were filled every frame and never drained.
    //   0x823C5670  CgsDev::PerfMonCpu::StartMonitor(gm+0x996F28 == mCpuMonitors+0x10, miUT_GameState)
    //   0x823C5680  IOHelper: CreateIOBuffer<CgsInput::InputIO::PreWorldInputBuffer>(inStack, "InputPreWorld")
    //               (0x823AEE18: Alloc 920 + PreWorldInputBuffer::Construct), asserting
    //               "mpStack->CreateIOBuffer( &mpBuffer, lpcName )" (CgsModuleIOHelper.h:52)
    //   0x823C56C8  CgsDev::PerfMonCpu::StartMonitor(gm+0x996F9C == mCpuMonitors+0x84, miUT_GameState_Bridge)
    //   0x823C56D8  LockForWrite ; GameStateModule::BridgeRumbleToInput(gm+0x669400, buffer,
    //               gm+0x9A0B0C == &mTimerStatusInterface) ; UnlockForWrite ; StopMonitor
    //   0x823C5720  mInputModule (gm+0x6E9630) vtable+0x44 == InputModule::PreWorldUpdate(inStack,
    //               outStack, buffer, lpInputModuleOutput, *(gm+0x9A0622) == 0 -- !mbDiskError)
    //   0x823C5738..0x823C57F8  if (miInputModuleState == 1): select the invite user or first
    //               START-pressed pad, then set miInputModuleState = 2
    //   0x823C5808  QueryRequestDoStepFrame(lpInputModuleOutput)                   -- caller unactivated
    //   0x823C57F8  CgsDev::PerfMonCpu::StopMonitor(miUT_GameState) ; the IOHelper's DestroyIOBuffer (0x823AEEE8) at scope end
    // =========================================================================================
    u32 BrnGameModule::DoUpdate_InputPreWorld(CgsModule::IOBufferStack* lpInputBufferStack,
                                              CgsModule::IOBufferStack* lpOutputBufferStack,
                                              CgsInput::InputIO::OutputBuffer* lpInputModuleOutput)
    {
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_GameState);
        CgsModule::IOHelper<CgsInput::InputIO::PreWorldInputBuffer> lpInputModulePreWorldInputBuffer(
            lpInputBufferStack, "InputPreWorld");

        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_GameState_Bridge);
        lpInputModulePreWorldInputBuffer->LockForWrite();
        mGameStateModule.BridgeRumbleToInput(lpInputModulePreWorldInputBuffer, &mTimerStatusInterface);
        lpInputModulePreWorldInputBuffer->UnlockForWrite();
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_GameState_Bridge);

        mInputModule.PreWorldUpdate(lpInputBufferStack, lpOutputBufferStack,
                                    lpInputModulePreWorldInputBuffer, lpInputModuleOutput, !mbDiskError);

        // The selection path keeps the original no-user sentinel until state 1 finds a user.
        u32 liStartPressedPort = CgsInput::KU_NUMBER_OF_PADS;
        if (miInputModuleState == 1)
        {
            lpInputModuleOutput->LockForRead();
            if (mHardware.HasGameBeenRebootedDueToInvite())
            {
                BrnHW::System360HW* lpLaunchData = GetSoftRebootData();
                if (lpLaunchData == 0)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert("lpLaunchData",
                        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnGameModuleUpdateFunctions.cpp", 71);
                    CgsDev::Assert::EndAssert();
                }
                liStartPressedPort = lpLaunchData->mdwUserIndex;
                miInputModuleState = E_INPUTBINDSTATE_START_PRESSED;
            }
            else
            {
                for (s32 liPort = 0; liPort < CgsInput::KU_NUMBER_OF_PADS; ++liPort)
                {
                    const CgsInput::InputIO::PadOutputInformation* lpPadInfo =
                        lpInputModuleOutput->GetPadInfo(liPort);
                    if (((lpPadInfo->maActionInfo[E_GAMEINPUTACTIONS_START].muStatus >> 1) & 1) != 0)
                    {
                        liStartPressedPort = static_cast<u32>(liPort);
                        miInputModuleState = E_INPUTBINDSTATE_START_PRESSED;
                        break;
                    }
                }
            }
            lpInputModuleOutput->UnlockForRead();
        }
        // This remains an idle/rebind transition: construction starts at state 4; an unbind
        // completion returns to 0, and BridgeGuiToGame event 90 can arm state 1 again.
        // Original 0x823C5808: query only after the input module has published its pad output.
        QueryRequestDoStepFrame(lpInputModuleOutput);
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_GameState);
        return liStartPressedPort;
    }

    // ARTIST 0x823BCF40; one-time initial console mapping, all four pads (port -1).
    void BrnGameModule::PrepareInitialInputMapping(
        CgsInput::InputIO::PostWorldInputBuffer* lpPostWorldInput)
    {
        CGS_ASSERT(lpPostWorldInput != 0, "lpInputBuffer");
        lpPostWorldInput->LockForWrite();
        lpPostWorldInput->PostMappingRequest(KA_CONSOLE_DEFAULT_GAME_INPUT_MAPPING, -1);
        CGS_ASSERT(lpPostWorldInput != 0, "lpInputBuffer");
        lpPostWorldInput->UnlockForWrite();
    }

    // ARTIST 0x823BD390; bind player 0 to the port selected by the pre-world input leg.
    void BrnGameModule::BindInputToPort(
        s32 liPort,
        CgsInput::InputIO::PostWorldInputBuffer* lpPostWorldInput)
    {
        CGS_ASSERT(lpPostWorldInput != 0, "lpInputBuffer");
        lpPostWorldInput->LockForWrite();
        lpPostWorldInput->PostBindRequest(0, liPort);
        CGS_ASSERT(lpPostWorldInput != 0, "lpInputBuffer");
        lpPostWorldInput->UnlockForWrite();
    }

    // ARTIST 0x823C59E8. Both stacks are passed by the original caller; only the input stack
    // owns this helper. The GameState output is read-locked and the World output read-locked
    // around their respective bridges, in console order.
    void BrnGameModule::DoUpdate_InputPostWorld(
        CgsModule::IOBufferStack* lpInputBufferStack,
        CgsModule::IOBufferStack* /*lpOutputBufferStack*/,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput,
        const BrnWorldIO::UpdateOutputBuffer* lpWorldOutput,
        u32 liStartPressedPort)
    {
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_GameState);
        CgsModule::IOHelper<CgsInput::InputIO::PostWorldInputBuffer> lpPostWorldInput(
            lpInputBufferStack, "InputPostWorld");
        CgsInput::InputIO::PostWorldInputBuffer* lpPostWorldInputBuffer = lpPostWorldInput;

        if (!sbInitialInputMappingPosted)
        {
            PrepareInitialInputMapping(lpPostWorldInputBuffer);
            sbInitialInputMappingPosted = true;
        }

        if (miInputModuleState == 2)
        {
            BindInputToPort(static_cast<s32>(liStartPressedPort), lpPostWorldInputBuffer);
            miInputModuleState = E_INPUTBINDSTATE_BIND_REQUESTED;
        }
        else if (miInputModuleState == 5)
        {
            lpPostWorldInputBuffer->LockForWrite();
            lpPostWorldInputBuffer->PostUnbindRequest(0);
            lpPostWorldInputBuffer->UnlockForWrite();
            miInputModuleState = E_INPUTBINDSTATE_UNBIND_REQUESTED;
        }

        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_GameState_Bridge);
        CgsModule::LockBuffersForIO(lpPostWorldInputBuffer, lpGameStateOutput);
        BridgeGameStateToController(lpPostWorldInputBuffer, lpGameStateOutput);
        CgsModule::UnlockBuffersForIO(lpPostWorldInputBuffer, lpGameStateOutput);

        CgsModule::LockBuffersForIO(lpPostWorldInputBuffer, lpWorldOutput);
        const BrnWorldIO::UpdateOutputBuffer::VehicleManagerOutputInterface*
            lpVehicleManagerOutput = lpWorldOutput->GetVehicleManagerOutputInterface();
        lpPostWorldInputBuffer->SetWheelFFSpring(lpVehicleManagerOutput->GetPlayerWheelFFSpring());
        CgsModule::UnlockBuffersForIO(lpPostWorldInputBuffer, lpWorldOutput);
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_GameState_Bridge);

        mInputModule.PostWorldUpdate(lpInputBufferStack, lpPostWorldInputBuffer);
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_GameState);
    }

    // ARTIST 0x823BD248. The caller owns the input-module output buffer's read lock.
    void BrnGameModule::BridgeInputToGame(
        const CgsInput::InputIO::OutputBuffer* lpInputModuleOutput)
    {
        if (lpInputModuleOutput->GetBindResultQueue()->GetLength() != 0 &&
            miInputModuleState != 3)
        {
            CGS_ASSERT(false,
                "lpInputModuleOutput->GetBindResultQueue()->GetLength() == 0 || "
                "meInputBindState == E_INPUTBINDSTATE_BIND_REQUESTED");
        }
        if (lpInputModuleOutput->GetUnbindResultQueue()->GetLength() != 0 &&
            miInputModuleState != 6)
        {
            CGS_ASSERT(false,
                "lpInputModuleOutput->GetUnbindResultQueue()->GetLength() == 0 || "
                "meInputBindState == E_INPUTBINDSTATE_UNBIND_REQUESTED");
        }

        if (miInputModuleState == 3)
        {
            const CgsInput::InputIO::OutputBuffer::BindResultQueue* lpBindResults =
                lpInputModuleOutput->GetBindResultQueue();
            const s32 liNumResults = lpBindResults->GetLength();
            for (s32 i = 0; i < liNumResults; ++i)
                HandleInputBindResult(lpBindResults->GetEvent(i));
        }
        else if (miInputModuleState == 6)
        {
            const CgsInput::InputIO::OutputBuffer::UnBindResultQueue* lpUnbindResults =
                lpInputModuleOutput->GetUnbindResultQueue();
            const s32 liNumResults = lpUnbindResults->GetLength();
            for (s32 i = 0; i < liNumResults; ++i)
                HandleInputUnbindResult(lpUnbindResults->GetEvent(i));
        }
    }

    // ARTIST 0x823A8C38. Only player 0 changes the game's active-controller state.
    void BrnGameModule::HandleInputBindResult(const CgsInput::InputIO::BindResult& lrResult)
    {
        if (lrResult.miPlayer != 0)
            return;

        CGS_ASSERT(miInputModuleState == 3,
                   "meInputBindState == E_INPUTBINDSTATE_BIND_REQUESTED");
        CGS_ASSERT(lrResult.meResultCode == CgsInput::E_BINDRESULTOK,
                   "CgsInput::E_BINDRESULTOK == lResult.GetResultCode()");

        miPlayer0ControllerPort = lrResult.miPort;
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "Active controller is port: "
                                       << lrResult.miPort << "\n";

        miSecondaryControllerPort = (miPlayer0ControllerPort == 0) ? 1 : 0;
        mDebugManager.SetGamePad(
            mInputModule.mControllers.GetDebugGamePad(miSecondaryControllerPort));
        miInputModuleState = E_INPUTBINDSTATE_BOUND;
    }

    // ARTIST 0x823A8D50. Only player 0 changes the game's active-controller state.
    void BrnGameModule::HandleInputUnbindResult(const CgsInput::InputIO::UnBindResult& lrResult)
    {
        if (lrResult.miPlayer != 0)
            return;

        CGS_ASSERT(miInputModuleState == 6,
                   "meInputBindState == E_INPUTBINDSTATE_UNBIND_REQUESTED");
        CGS_ASSERT(lrResult.meResultCode == CgsInput::E_UNBINDRESULTOK,
                   "CgsInput::E_UNBINDRESULTOK == lResult.GetResultCode()");

        miPlayer0ControllerPort = CgsInput::KU_NUMBER_OF_PADS;
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "Active controller invalidated\n";
        mDebugManager.SetGamePad(0);
        miInputModuleState = E_INPUTBINDSTATE_UNBOUND;
    }

    // X360 0x823C5580; BrnGameModule.cpp:4013 (DecFIGS).
    void BrnGameModule::QueryRequestDoStepFrame(
        const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer)
    {
        if (_gbHACKIsOnline)
            return;

        lpInputOutputBuffer->LockForRead();
        s32 liControllerPort = 0;
        const CgsInput::InputIO::PadOutputInformation* lpPadInfo =
            GetPadInfoForPlayer0(lpInputOutputBuffer, &liControllerPort);
        if (lpPadInfo != 0 &&
            (lpPadInfo->maActionInfo[21].muStatus & 2u) != 0 &&
            (lpPadInfo->maActionInfo[57].muStatus & 1u) != 0 &&
            (lpPadInfo->maActionInfo[56].muStatus & 1u) != 0)
        {
            if (mbSteppingFrames)
                mbRequestDoPlayFrame = true;
            else
                mbRequestDoStepFrame = true;
        }
        lpInputOutputBuffer->UnlockForRead();
    }

    // @ BrnGameModule.cpp:4048 - apply a pending step/play-frame request: a step request
    // starts single-stepping and runs one step; a play request resumes from stepping.
    void BrnGameModule::UpdateRequestDoStepFrame()
    {
        if (mbRequestDoStepFrame)
        {
            mbSteppingFrames = true;
            mbDoStep = true;
            mbRequestDoStepFrame = false;
        }
        else if (mbRequestDoPlayFrame)
        {
            if (mbSteppingFrames)
                mbStopStepping = true;
            mbRequestDoPlayFrame = false;
        }
    }

    // @ 0x823C54D8 [P1 sim-pause] -- the pause spine's consumer end: drain the game-action
    // queue under the output buffer's read lock; action 86 (KI_ACTION_PAUSE_STRICT) raises
    // mbSimPaused and STOPS the sim timer, action 87 (KI_ACTION_UNPAUSE) clears it and
    // restarts the timer. Console stores, verbatim (p1_dump.txt):
    //     case 86: *(gm+0x9A0621) = 1; *(gm+0x9A0B08) = 0;   // mbSimPaused / simTimer.mbRunning
    //     case 87: *(gm+0x9A0621) = 0; *(gm+0x9A0B08) = 1;
    // (0x9A0B08 == the sim Timer @0x9A0AF4 +0x18 mbRunning -- the byte Construct's step 9
    // seeds to 1; see the step-9 note above.) Both stores by name here. The stopped sim
    // timer is what freezes the world: every sim sub-step's timestep derives from it, and
    // ConstructUpdateSetFromFsm additionally raises update-set bit 0 off mbSimPaused.
    void BrnGameModule::CheckGameActions(
        BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput)
    {
        lpGameStateOutput->LockForRead();
        const BrnGameState::GameStateModuleIO::GameActionQueue* lpActionQueue =
            static_cast<const BrnGameState::GameStateModuleIO::OutputBuffer*>(lpGameStateOutput)
                ->GetGameActionQueue();

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liAction = lpActionQueue->GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent != 0)
        {
            if (liAction == 86)
            {
                mbSimPaused = true;
                mSimTimer.SetRunning(false);
            }
            else if (liAction == 87)
            {
                mbSimPaused = false;
                mSimTimer.SetRunning(true);
            }
            const CgsModule::Event* lpCurrent = lpEvent;
            liAction = lpActionQueue->GetNextEvent(lpCurrent, &lpEvent, &liSize);
        }
        lpGameStateOutput->UnlockForRead();
    }

    // @ BrnGameModule.cpp:3542 - build this frame's simulation update-set bitmask from the
    // active flow-state machine + game flags.
    BrnUpdateSet BrnGameModule::ConstructUpdateSetFromFsm()
    {
        u32 luUpdateSet = 0x80;
        if (mMainFlowStateMachine.IsInGameState())
            luUpdateSet = 0x88;
        if (mMainFlowStateMachine.IsSaveLoadState())
            luUpdateSet = (luUpdateSet & 0xFF3F) | 0x40;
        if (mMainFlowStateMachine.IsVideoState())
            luUpdateSet |= 0x20;
        if (mbSimPaused || mMainFlowStateMachine.IsSaveLoadState())
            luUpdateSet |= 0x1;
        if (mbStalled)
            luUpdateSet |= 0x400;
        if (mbDiskError)
            luUpdateSet |= 0x200;
        return (BrnUpdateSet)luUpdateSet;
    }

    // ARTIST 0x823CBBB0. Search a GUI-model output event queue for the requested type.
    bool BrnGameModule::IsEventInGuiQueue(
        s32 liEventTypeToFind,
        const CgsGui::ModelIO::OutputBuffer::GuiEventQueue* lpEventQueue)
    {
        CGS_ASSERT(lpEventQueue != 0, "lpEventQueue != NULL");

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        s32 liEventType = lpEventQueue->GetFirstEvent(&lpEvent, &liEventSize);
        if (lpEvent == 0)
            return false;

        while (liEventType != liEventTypeToFind)
        {
            liEventType = lpEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
            if (lpEvent == 0)
                return false;
        }
        return true;
    }

    // ARTIST 0x823CBC48 is unnamed; DecFIGS declares this GuiEventQueueSmall overload.
    bool BrnGameModule::IsEventInGuiQueue(
        s32 liEventTypeToFind,
        const CgsGui::GuiEventQueueSmall* lpEventQueue)
    {
        CGS_ASSERT(lpEventQueue != 0, "lpEventQueue != NULL");

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        s32 liEventType = lpEventQueue->GetFirstEvent(&lpEvent, &liEventSize);
        if (lpEvent == 0)
            return false;

        while (liEventType != liEventTypeToFind)
        {
            liEventType = lpEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
            if (lpEvent == 0)
                return false;
        }
        return true;
    }

    // ARTIST 0x823DCB40. Keep this late, output-derived update set distinct from
    // ConstructUpdateSetFromFsm(), which is the set captured before the module legs.
    BrnUpdateSet BrnGameModule::ConstructUpdateSet(
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateModuleOutputBuffer,
        const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkModuleOutputBuffer,
        const CgsInput::InputIO::OutputBuffer* lpInputModuleOutputBuffer,
        const CgsGui::ModelIO::OutputBuffer* lpGuiModuleModelOutputBuffer,
        const BrnReplays::ReplayIO::OutputBuffer_PreSim* lpReplayOutputBuffer)
    {
        CGS_ASSERT(lpGameStateModuleOutputBuffer != 0,
                   "lpGameStateModuleOutputBuffer != NULL");
        CGS_ASSERT(lpInputModuleOutputBuffer != 0,
                   "lpInputModuleOutputBuffer != NULL");
        CGS_ASSERT(lpGuiModuleModelOutputBuffer != 0,
                   "lpGuiModuleModelOutputBuffer != NULL");

        BrnUpdateSet leUpdateSet = ConstructUpdateSetFromFsm();

        // ARTIST locks the game-state output here without reading from it.
        lpGameStateModuleOutputBuffer->LockForRead();
        lpGameStateModuleOutputBuffer->UnlockForRead();

        // Replay status bit 0 selects replay playback and the paired update bits 0x101.
        lpReplayOutputBuffer->LockForRead();
        if ((lpReplayOutputBuffer->GetStatusInterface()->mxStatusFlags & 1u) != 0)
            leUpdateSet |= 0x101;
        lpReplayOutputBuffer->UnlockForRead();

        // A pending pad-disconnect event contributes update bit 0x4. The typed queue
        // accessor returns the BaseEventQueue whose GetLength is the asm's +8 word.
        lpInputModuleOutputBuffer->LockForRead();
        if (lpInputModuleOutputBuffer->GetPadDisconnectedQueue()->GetLength() != 0)
            leUpdateSet |= 0x4;
        lpInputModuleOutputBuffer->UnlockForRead();

        CGS_ASSERT(lpNetworkModuleOutputBuffer != 0,
                   "lpNetworkModuleOutputBuffer != NULL");
        lpNetworkModuleOutputBuffer->LockForRead();
        if (IsEventInGuiQueue(44, lpNetworkModuleOutputBuffer->GetGuiEventQueue()))
            leUpdateSet |= 0x2;
        if (lpNetworkModuleOutputBuffer->IsInInvite())
            leUpdateSet |= 0x10;
        lpNetworkModuleOutputBuffer->UnlockForRead();

        lpGuiModuleModelOutputBuffer->LockForRead();
        if (IsEventInGuiQueue(66, lpGuiModuleModelOutputBuffer->GetGuiOutEvents()))
            leUpdateSet |= 0x40;
        if (IsEventInGuiQueue(69, lpGuiModuleModelOutputBuffer->GetGuiOutEvents()))
            leUpdateSet |= 0x20;
        if (mbDiskError)
            leUpdateSet |= 0x200;
        if (mbStreamingStalled)
            leUpdateSet |= 0x800;
        lpGuiModuleModelOutputBuffer->UnlockForRead();

        return leUpdateSet;
    }

    // @ BrnGameModule.cpp:1146 - the update-thread stage machine: drives GamePrepare (one-time
    // setup) -> GameMain (per-frame update, repeating) -> GameRelease (teardown). meGameUpdateStage
    // is PREPARE(0)/MAIN(1)/RELEASE(2). Returns true to keep the thread alive.
    // ARTIST 0x823C5480: the public frame entry brackets ThreadLayout::Update.
    // Massive preparation follows the profiling bracket once native modules are loaded.
    void BrnGameModule::Update()
    {
        CgsDev::PerfMonCpu::StartProfiling();
        mThreadLayout.Update();
        CgsDev::PerfMonCpu::StopProfiling();
        if (mHardware.mbEngineModulesLoaded)
            mHardware.mMassive.Prepare();
    }

    bool BrnGameModule::UpdateThread()
    {
        switch (meGameUpdateStage)
        {
        case E_GAMEUPDATESTAGE_PREPARE:
            if (!GamePrepare())
                break;
            meGameUpdateStage = E_GAMEUPDATESTAGE_MAIN;
            GameMain();
            break;

        case E_GAMEUPDATESTAGE_MAIN:
            meGameUpdateStage = E_GAMEUPDATESTAGE_MAIN;
            GameMain();
            break;

        case E_GAMEUPDATESTAGE_RELEASE:
            meGameUpdateStage = E_GAMEUPDATESTAGE_RELEASE;
            if (GameRelease())
                meGameUpdateStage = E_GAMEUPDATESTAGE_PREPARE;
            break;

        default:
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Invalid update stage\n",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnGameModule.cpp", 1199);
            CgsDev::Assert::EndAssert();
            break;
        }
        return true;
    }

    // @ BrnGameModule.cpp:3872 - debug helper: fill liNumBytes at lpDest with the repeating
    // 32-bit pattern luValue (head/tail handled byte-wise). Used by DebugMemoryInit to stamp
    // freed module memory with a marker value.
    void BrnGameModule::DebugSetMemoryToInt(void* lpDest, s32 liNumBytes, u32 luValue)
    {
        u8*       lpByte = static_cast<u8*>(lpDest);
        const u8* lpPattern = reinterpret_cast<const u8*>(&luValue);
        for (s32 i = 0; i < liNumBytes; ++i)
            lpByte[i] = lpPattern[i & 3];
    }

    // @ BrnGameModule.cpp:1515 - after the GPU vsync wait completes, ask the frame-rate
    // manager how many simulation steps this render frame must run (driving GameMain's
    // catch-up loop count).
    void BrnGameModule::OnCompletionOfVsyncWait()
    {
        miNumSimFramesRequired = mFrameRateManager.UpdatePostRenderWait(
            mi8ActualFrameRateMinStepsThisFrame, mi8ActualFrameRateMaxStepsThisFrame);

    }

    // @ BrnGameModule.cpp:1565 - render the on-screen assert overlay via the renderer module.
    void BrnGameModule::RenderAssert(const AssertData* lpAssertData)
    {
        mRenderModule.RenderAssert(lpAssertData);
    }

    // @ BrnGameModule.cpp:3981 - debug-menu "step frame" callback: begin single-stepping and
    // request one step. The void* context is the game-module instance.
    void BrnGameModule::StepFrameCB(void* lpContext)
    {
        BrnGameModule* lpThis = static_cast<BrnGameModule*>(lpContext);
        lpThis->mbSteppingFrames = true;
        lpThis->mbDoStep = true;
    }

    // @ BrnGameModule.cpp:3996 - debug-menu "play frame" callback: while single-stepping,
    // request a resume.
    void BrnGameModule::PlayFrameCB(void* lpContext)
    {
        BrnGameModule* lpThis = static_cast<BrnGameModule*>(lpContext);
        if (lpThis->mbSteppingFrames)
            lpThis->mbStopStepping = true;
    }

    // @ X360 0x823A8B38 - the disk-error worker thread body. Raises the system dirty-disc
    // error UI for the given user and never returns (the console holds on the error UI).
    // Tail-calls straight into the XDK entry point.
    void BrnGameModule::DiskErrorThreadProc(unsigned long dwUserIndex)
    {
        XShowDirtyDiscErrorUI(dwUserIndex);
    }

    // @ X360 0x823C0268 - return the embedded hardware object (the launch/soft-reboot data
    // block the platform fills at boot). The X360 forms this + 0x9A11C0, which is &mHardware.
    BrnHW::System360HW* BrnGameModule::GetSoftRebootData()
    {
        return &mHardware;
    }

    // @ X360 0x823C0288 - forward to the hardware object's invite-reboot query (the X360
    // hands it &mHardware as the System360HW `this` and tail-calls).
    int BrnGameModule::HasGameBeenRebootedDueToInvite()
    {
        return mHardware.HasGameBeenRebootedDueToInvite();
    }

    // @ X360 0x823C0278 - the soft-reboot flag. Reads a single byte at game-module + 0x9A1254,
    // which is mHardware + 0x94 (the launch-data flag byte BrnHW::System360HW::mPad94).
    int BrnGameModule::HasGameBeenSoftRebooted()
    {
        return mHardware.mPad94;
    }

    // @ X360 0x823A9030 - the game module's GameTalk "StopRenderMetrics" receiver. Walks
    // every key of the incoming message and, on the "StopRenderMetrics" key, clears the
    // render-metrics-requested flag. Same receiver shape as
    // BrnDirector::Camera::BehaviourRenderMetrics::GameTalkMessageReceiver: the context
    // module is delivered by pointer-to-pointer (lppModule).
    void BrnGameModule::RenderMetricsMessageHandler(EA::GameTalk::GameTalkMessage* lpMessage,
                                                    BrnGameModule** lppModule)
    {
        BrnGameModule* lpModule = *lppModule;
        for (s32 liIndex = 0; liIndex < lpMessage->GetNumKeys(); ++liIndex)
        {
            const char* lpcKey = lpMessage->GetKey(liIndex);
            if (_stricmp(lpcKey, "StopRenderMetrics") == 0)
                lpModule->muRenderMetricsCount = 0;
        }
    }
}
