#pragma once

#include "types.hpp"

// BrnGame::BrnCpuMonitors - the game's per-frame CPU performance-monitor handle block. Each
// field is an int handle (from CgsDev::PerfMonCpu::AddMonitor) that brackets one timed
// region of the update/render frame; Construct() registers/zeroes them. The owning
// BrnGameModule starts/stops these around the corresponding work in GameMain/DispatchThread.
// Field set + order recovered from the DecFIGS DWARF (GameSource/Game/BrnGlobalCpuMonitors.h).
namespace BrnGame
{
    struct BrnCpuMonitors
    {
        s32 miUT_TotalUpdate;       // +0x00
        s32 miUT_EachUpdate;        // +0x04
        // X360 +0x08: Construct stores the 39 monitor handles at {0,4,0xC..0x9C},
        // skipping +0x08 (asm hole) -> there is a non-monitor reserved word here that
        // Construct leaves untouched. Modelled so the 39 named handles land at their
        // true offsets (miUT_NetworkAIRaceCar@0xC .. miUT_SoundUpdate@0x9C; sizeof 160).
        s32 mReserved08;
        s32 miUT_NetworkAIRaceCar;  // +0x0C
        s32 miUT_Network;
        s32 miUT_GameState;
        s32 miUT_GUI;
        s32 miUT_Director;
        s32 miUT_Sound;
        s32 miUT_Effects;
        s32 miUT_AI;
        s32 miUT_RaceCar;
        s32 miUT_Traffic;
        s32 miUT_Triggers;
        s32 miUT_CrashManager;
        s32 miUT_Physics;
        s32 miUT_World;
        s32 miUT_Resource;
        s32 miUT_DebugManager;
        s32 miUT_RenderAll;
        s32 miUT_FrustumTesting;
        s32 miUT_RenderMainScreen;
        s32 miUT_RenderShadowMap;
        s32 miUT_RenderEnvMap;
        s32 miUT_RenderFX;
        s32 miUT_RenderGUI;
        s32 miDT_DispatchToGpu;
        s32 miUT_ThreadSync;
        s32 miUT_WaitOnDispatch;
        s32 miUT_RaceCar_SQ;
        s32 miUT_Traffic_SQ;
        s32 miUT_Triggers_SQ;
        s32 miUT_Director_SQ;
        s32 miUT_GameState_Bridge;
        s32 miUT_GUI_Bridge;
        s32 miUT_AI_Bridge;
        s32 miUT_RaceCar_Bridge;
        s32 miUT_Traffic_Bridge;
        s32 miUT_Director_Bridge;
        s32 miUT_SoundUpdate;

        void Construct();
    };
}
