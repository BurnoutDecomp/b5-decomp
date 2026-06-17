#include "GameSource/Game/BrnGlobalCpuMonitors.h"

#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // PerfMonCpu::Construct / AddMonitor

namespace BrnGame
{
    using namespace CgsDev;

    // Bring up the CPU perfmon registry and register each timed region of the frame. Each AddMonitor
    // returns the handle the update/render spine brackets with Start/StopMonitor and the perfmon
    // overlay draws as a bar. The X360 assigns each monitor a page + CPU budget from data; the bounded
    // reconstruction registers them all on the general page with no budget (the page grouping + budgets
    // are the perfmon follow-on). Field set/order from the DecFIGS DWARF (BrnGlobalCpuMonitors.h).
    void BrnCpuMonitors::Construct()
    {
        PerfMonCpu::Construct(64, nullptr);

        miUT_TotalUpdate       = PerfMonCpu::AddMonitor("TotalUpdate",       E_PMP_GENERAL, false, 0.0f, false);
        miUT_EachUpdate        = PerfMonCpu::AddMonitor("EachUpdate",        E_PMP_GENERAL, false, 0.0f, false);
        miUT_NetworkAIRaceCar  = PerfMonCpu::AddMonitor("NetworkAIRaceCar",  E_PMP_GENERAL, false, 0.0f, false);
        miUT_Network           = PerfMonCpu::AddMonitor("Network",           E_PMP_GENERAL, false, 0.0f, false);
        miUT_GameState         = PerfMonCpu::AddMonitor("GameState",         E_PMP_GENERAL, false, 0.0f, false);
        miUT_GUI               = PerfMonCpu::AddMonitor("GUI",               E_PMP_GENERAL, false, 0.0f, false);
        miUT_Director          = PerfMonCpu::AddMonitor("Director",          E_PMP_GENERAL, false, 0.0f, false);
        miUT_Sound             = PerfMonCpu::AddMonitor("Sound",             E_PMP_GENERAL, false, 0.0f, false);
        miUT_Effects           = PerfMonCpu::AddMonitor("Effects",           E_PMP_GENERAL, false, 0.0f, false);
        miUT_AI                = PerfMonCpu::AddMonitor("AI",                 E_PMP_GENERAL, false, 0.0f, false);
        miUT_RaceCar           = PerfMonCpu::AddMonitor("RaceCar",           E_PMP_GENERAL, false, 0.0f, false);
        miUT_Traffic           = PerfMonCpu::AddMonitor("Traffic",           E_PMP_GENERAL, false, 0.0f, false);
        miUT_Triggers          = PerfMonCpu::AddMonitor("Triggers",          E_PMP_GENERAL, false, 0.0f, false);
        miUT_CrashManager      = PerfMonCpu::AddMonitor("CrashManager",      E_PMP_GENERAL, false, 0.0f, false);
        miUT_Physics           = PerfMonCpu::AddMonitor("Physics",           E_PMP_GENERAL, false, 0.0f, false);
        miUT_World             = PerfMonCpu::AddMonitor("World",             E_PMP_GENERAL, false, 0.0f, false);
        miUT_Resource          = PerfMonCpu::AddMonitor("Resource",          E_PMP_GENERAL, false, 0.0f, false);
        miUT_DebugManager      = PerfMonCpu::AddMonitor("DebugManager",      E_PMP_GENERAL, false, 0.0f, false);
        miUT_RenderAll         = PerfMonCpu::AddMonitor("RenderAll",         E_PMP_GENERAL, false, 0.0f, false);
        miUT_FrustumTesting    = PerfMonCpu::AddMonitor("FrustumTesting",    E_PMP_GENERAL, false, 0.0f, false);
        miUT_RenderMainScreen  = PerfMonCpu::AddMonitor("RenderMainScreen",  E_PMP_GENERAL, false, 0.0f, false);
        miUT_RenderShadowMap   = PerfMonCpu::AddMonitor("RenderShadowMap",   E_PMP_GENERAL, false, 0.0f, false);
        miUT_RenderEnvMap      = PerfMonCpu::AddMonitor("RenderEnvMap",      E_PMP_GENERAL, false, 0.0f, false);
        miUT_RenderFX          = PerfMonCpu::AddMonitor("RenderFX",          E_PMP_GENERAL, false, 0.0f, false);
        miUT_RenderGUI         = PerfMonCpu::AddMonitor("RenderGUI",         E_PMP_GENERAL, false, 0.0f, false);
        miDT_DispatchToGpu     = PerfMonCpu::AddMonitor("DispatchToGpu",     E_PMP_GENERAL, false, 0.0f, false);
        miUT_ThreadSync        = PerfMonCpu::AddMonitor("ThreadSync",        E_PMP_GENERAL, false, 0.0f, false);
        miUT_WaitOnDispatch    = PerfMonCpu::AddMonitor("WaitOnDispatch",    E_PMP_GENERAL, false, 0.0f, false);
        miUT_RaceCar_SQ        = PerfMonCpu::AddMonitor("RaceCar_SQ",        E_PMP_GENERAL, false, 0.0f, false);
        miUT_Traffic_SQ        = PerfMonCpu::AddMonitor("Traffic_SQ",        E_PMP_GENERAL, false, 0.0f, false);
        miUT_Triggers_SQ       = PerfMonCpu::AddMonitor("Triggers_SQ",       E_PMP_GENERAL, false, 0.0f, false);
        miUT_Director_SQ       = PerfMonCpu::AddMonitor("Director_SQ",       E_PMP_GENERAL, false, 0.0f, false);
        miUT_GameState_Bridge  = PerfMonCpu::AddMonitor("GameState_Bridge",  E_PMP_GENERAL, false, 0.0f, false);
        miUT_GUI_Bridge        = PerfMonCpu::AddMonitor("GUI_Bridge",        E_PMP_GENERAL, false, 0.0f, false);
        miUT_AI_Bridge         = PerfMonCpu::AddMonitor("AI_Bridge",         E_PMP_GENERAL, false, 0.0f, false);
        miUT_RaceCar_Bridge    = PerfMonCpu::AddMonitor("RaceCar_Bridge",    E_PMP_GENERAL, false, 0.0f, false);
        miUT_Traffic_Bridge    = PerfMonCpu::AddMonitor("Traffic_Bridge",    E_PMP_GENERAL, false, 0.0f, false);
        miUT_Director_Bridge   = PerfMonCpu::AddMonitor("Director_Bridge",   E_PMP_GENERAL, false, 0.0f, false);
        miUT_SoundUpdate       = PerfMonCpu::AddMonitor("SoundUpdate",       E_PMP_GENERAL, false, 0.0f, false);
    }
}
