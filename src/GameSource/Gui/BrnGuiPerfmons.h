#pragma once

#include "types.hpp"

namespace BrnGui
{
// Perfmon handle tree for the GUI module's CPU performance monitors. Each handle
// is the int returned by CgsDev::PerfMonCpu::AddMonitor and is used as the parent
// of its child monitors (see BrnGuiPerfmons.cpp).
struct GuiPerfmons
{
public:
    // Registers the full GUI-module perfmon monitor tree (34 monitors).
    void Initialise();

    // --- indent 0 (root) monitors ---
    static int32_t miGuiModulePreWorldUpdate;       // "GUI MODULE PRE-WORLD UPDATE"
    static int32_t miGuiModuleUpdate;               // "ENTIRE GUI MODULE UPDATE"
    static int32_t miGuiModuleRender;               // "ENTIRE GUI MODULE RENDER"

    // --- UPDATE subtree ---
    static int32_t miGuiModuleEventPump;            // "  Gui Module Event Pump"
    static int32_t miGuiUpdate;                      // "  CgsGui - Update Total"
    static int32_t miModelUpdate;                    // "    CgsGui - Model Update"
    static int32_t miModelUpdate_Input;             // "      Model Update INPUT"
    static int32_t miModelUpdate_Main;              // "      Model Update MAIN"
    static int32_t miModelUpdate_Output;            // "      Model Update OUTPUT"
    static int32_t miHudUpdate;                      // "        Gui - HudFlow Update"
    static int32_t miHudStateUpdate;                 // "          HUD state Update"
    static int32_t miSatNavUpdate;                   // "            SatNav Update"
    static int32_t miMapIconMgrUpdate;               // "              MapIconMgr Update"
    static int32_t miMapIconMgrUpdate2;              // "              MapIconMgr Upd 2"
    static int32_t miPlayerPosTableUpdate;           // "            PlayerPosTbl Update"
    static int32_t miScreenUpdate;                   // "        Gui - ScreenFlow Update"
    static int32_t miModelViewBridge;                // "    CgsGui - Model View Bridge"
    static int32_t miViewUpdate;                     // "    CgsGui - View Update"
    static int32_t miFlaptUpdate;                    // "      FLApt - Update"
    static int32_t miViewProcessIncomingEvents;      // "      Process Incoming Events"
    static int32_t miAptAuxUpdate;                   // "      ViewModuleApt"
    static int32_t miAptAuxUpdateTarget;             // "        AptAux - Upd Tgt"
    static int32_t miAptAuxUpdateComponents;         // "        AptAux - Upd Comps"
    static int32_t miAptAuxUpdateFlashComponentRes;  // "        AptAux - Upd Flsh Res"
    static int32_t miAptAuxUpdateFlashComponentNRes; // "        AptAux - Upd Flsh NRes"
    static int32_t miCgsLanguageManager;             // "      Language Manager"

    // --- RENDER subtree ---
    static int32_t miFlaptRender;                    // "  FLApt - Render"
    static int32_t miGuiRender;                      // "  CgsGui - Render"
    static int32_t miAptAuxRenderTarget;             // "    AptAux - Rndr Tgt"
    static int32_t miCustomRender;                   // "    Custom renderer Render"
    static int32_t miCgsAptFlashRender;              // "        Flash Object render"
    static int32_t miCgsAptStringRender;             // "      Flash String render"
    static int32_t miCgsAptDrawRenderingUnit;        // "      Flash Draw Rendering Unit"
};
} // namespace BrnGui
