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
        // FIXED LAYOUT (was: a mReserved08 hole @+0x08 pushing every later field down one
        // slot). The X360 BrnGameModule::Construct (0x823C9EA8) AddMonitor-registers FORTY
        // consecutive handles at +0x00..+0x9C -- including "Network + AI + Racecar" at +0x08
        // -- so there is NO hole; the +0x08 gap in the sentinel fill (Construct 0x823A90A8
        // stores -1 at {0,4,0xC..0x9C}) just means miUT_NetworkAIRaceCar is not sentinel-
        // seeded (it is AddMonitor'd immediately after, so the miss is benign). The 40th
        // field is miUT_Replay (@+0x40, the "      Replay" monitor): an ARTIST merge-window
        // addition absent from the DecFIGS DWARF's 39-field list.
        s32 miUT_TotalUpdate;       // +0x00  "UT: Total simulation"
        s32 miUT_EachUpdate;        // +0x04  "UT: Each sim step"
        s32 miUT_NetworkAIRaceCar;  // +0x08  "      Network + AI + Racecar" (no sentinel fill)
        s32 miUT_Network;           // +0x0C  "         Network"
        s32 miUT_GameState;         // +0x10  "      GameState"
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
        s32 miUT_World;             // +0x3C  "      World"
        s32 miUT_Replay;            // +0x40  "      Replay" (X360-only; not in the DecFIGS 39-field DWARF)
        s32 miUT_Resource;          // +0x44  "UT: ResourceSystem"
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
