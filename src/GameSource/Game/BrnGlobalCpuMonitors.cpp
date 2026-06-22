#include "GameSource/Game/BrnGlobalCpuMonitors.h"

namespace BrnGame
{
    // BrnGame::BrnCpuMonitors::Construct @ 0x823A90A8
    // Initialise the CPU perfmon handle block: every monitor handle is set to the
    // -1 "no monitor registered" sentinel. The X360 Construct does ONLY this -- the
    // actual CgsDev::PerfMonCpu::AddMonitor registration that fills these handles
    // happens later in the owning BrnGameModule spine, not here. Store-for-store: 39
    // stw(-1) at offsets {0,4,0xC..0x9C} -- the +0x8 word (mReserved08) is deliberately
    // NOT written (asm hole), so touching only the 39 named handles reproduces the asm.
    void BrnCpuMonitors::Construct()
    {
        miUT_TotalUpdate      = -1;
        miUT_EachUpdate       = -1;
        miUT_NetworkAIRaceCar = -1;
        miUT_Network          = -1;
        miUT_GameState        = -1;
        miUT_GUI              = -1;
        miUT_Director         = -1;
        miUT_Sound            = -1;
        miUT_Effects          = -1;
        miUT_AI               = -1;
        miUT_RaceCar          = -1;
        miUT_Traffic          = -1;
        miUT_Triggers         = -1;
        miUT_CrashManager     = -1;
        miUT_Physics          = -1;
        miUT_World            = -1;
        miUT_Resource         = -1;
        miUT_DebugManager     = -1;
        miUT_RenderAll        = -1;
        miUT_FrustumTesting   = -1;
        miUT_RenderMainScreen = -1;
        miUT_RenderShadowMap  = -1;
        miUT_RenderEnvMap     = -1;
        miUT_RenderFX         = -1;
        miUT_RenderGUI        = -1;
        miDT_DispatchToGpu    = -1;
        miUT_ThreadSync       = -1;
        miUT_WaitOnDispatch   = -1;
        miUT_RaceCar_SQ       = -1;
        miUT_Traffic_SQ       = -1;
        miUT_Triggers_SQ      = -1;
        miUT_Director_SQ      = -1;
        miUT_GameState_Bridge = -1;
        miUT_GUI_Bridge       = -1;
        miUT_AI_Bridge        = -1;
        miUT_RaceCar_Bridge   = -1;
        miUT_Traffic_Bridge   = -1;
        miUT_Director_Bridge  = -1;
        miUT_SoundUpdate      = -1;
    }
}
