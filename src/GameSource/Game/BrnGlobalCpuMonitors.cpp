#include "GameSource/Game/BrnGlobalCpuMonitors.h"

namespace BrnGame
{
    // BrnGame::BrnCpuMonitors::Construct @ 0x823A90A8
    // Initialise the CPU perfmon handle block: every monitor handle is set to the
    // -1 "no monitor registered" sentinel. The X360 Construct does ONLY this -- the
    // AddMonitor registration that fills all 40 handles happens right after, in
    // BrnGameModule::Construct (0x823C9EA8). Store-for-store: 39 stw(-1) at offsets
    // {0,4,0xC..0x9C} -- +0x08 (miUT_NetworkAIRaceCar, per the corrected 40-field
    // layout) is NOT sentinel-seeded by the asm; it is AddMonitor'd immediately
    // after, so the miss is benign and reproduced here.
    void BrnCpuMonitors::Construct()
    {
        miUT_TotalUpdate      = -1;
        miUT_EachUpdate       = -1;
        // (miUT_NetworkAIRaceCar deliberately not written -- the X360 sentinel fill skips +0x08.)
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
        miUT_Replay           = -1;
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
