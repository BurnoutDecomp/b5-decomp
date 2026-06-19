#include "GameSource/Gui/BrnGuiPerfmons.h"
#include "types.hpp"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // CgsDev::PerfMonCpu::AddMonitor (6-param overload)

// Reconstructed from BURNOUT_X360_ARTIST.XEX (BrnGui::GuiPerfmons::Initialise @0x824EF050).
//
// Registers the GUI module's 33-monitor CPU perfmon tree, storing each handle in a
// GuiPerfmons static member, then mirrors selected handles into a set of file-static
// aliases. Parent wiring (the decompiler-lost 5th AddMonitor arg) is recovered from the
// monitor name indentation (2 spaces per tree level): a monitor's parent is the nearest
// preceding monitor one level shallower; the 3 root monitors pass the -1 "no parent"
// sentinel. Call order is the exact X360 order (so every parent handle is registered
// before its children). The X360 build's 6-param AddMonitor(name, colour, min, budget,
// parent, flags) is the overload declared in the committed CgsPerfMonCpu.h (its real home).

namespace BrnGui
{
// --- Primary monitor handles (DWARF-named static members) ---
int32_t GuiPerfmons::miGuiModulePreWorldUpdate;
int32_t GuiPerfmons::miGuiModuleUpdate;
int32_t GuiPerfmons::miGuiModuleRender;
int32_t GuiPerfmons::miGuiModuleEventPump;
int32_t GuiPerfmons::miGuiUpdate;
int32_t GuiPerfmons::miModelUpdate;
int32_t GuiPerfmons::miModelUpdate_Input;
int32_t GuiPerfmons::miModelUpdate_Main;
int32_t GuiPerfmons::miModelUpdate_Output;
int32_t GuiPerfmons::miHudUpdate;
int32_t GuiPerfmons::miHudStateUpdate;
int32_t GuiPerfmons::miSatNavUpdate;
int32_t GuiPerfmons::miMapIconMgrUpdate;
int32_t GuiPerfmons::miMapIconMgrUpdate2;
int32_t GuiPerfmons::miPlayerPosTableUpdate;
int32_t GuiPerfmons::miScreenUpdate;
int32_t GuiPerfmons::miModelViewBridge;
int32_t GuiPerfmons::miViewUpdate;
int32_t GuiPerfmons::miFlaptUpdate;
int32_t GuiPerfmons::miViewProcessIncomingEvents;
int32_t GuiPerfmons::miAptAuxUpdate;
int32_t GuiPerfmons::miAptAuxUpdateTarget;
int32_t GuiPerfmons::miAptAuxUpdateComponents;
int32_t GuiPerfmons::miAptAuxUpdateFlashComponentRes;
int32_t GuiPerfmons::miAptAuxUpdateFlashComponentNRes;
int32_t GuiPerfmons::miCgsLanguageManager;
int32_t GuiPerfmons::miFlaptRender;
int32_t GuiPerfmons::miGuiRender;
int32_t GuiPerfmons::miAptAuxRenderTarget;
int32_t GuiPerfmons::miCustomRender;
int32_t GuiPerfmons::miCgsAptFlashRender;
int32_t GuiPerfmons::miCgsAptStringRender;
int32_t GuiPerfmons::miCgsAptDrawRenderingUnit;

// --- Alias handle copies (X360 dword_82F330xx / 82F331xx; addresses decompiler-named) ---
static int32_t giAliasUpdateTotal;        // dword_82F330A8 = miGuiUpdate
static int32_t giAliasModelUpdate;        // dword_82F330AC = miModelUpdate
static int32_t giAliasModelViewBridge;    // dword_82F330B0 = miModelViewBridge
static int32_t giAliasViewUpdate;         // dword_82F330B4 = miViewUpdate
static int32_t giAliasGuiRender;          // dword_82F330B8 = miGuiRender
static int32_t giAliasFlashObjectRender;  // dword_82F33168 = miCgsAptFlashRender
static int32_t giAliasFlashStringRender;  // dword_82F3316C = miCgsAptStringRender
static int32_t giAliasFlashDrawRendUnit;  // dword_82F33170 = miCgsAptDrawRenderingUnit
static int32_t giAliasModelUpdateInput;   // dword_82F330BC = miModelUpdate_Input
static int32_t giAliasModelUpdateMain;    // dword_82F330C0 = miModelUpdate_Main
static int32_t giAliasModelUpdateOutput;  // dword_82F330C4 = miModelUpdate_Output
static int32_t giAliasViewModuleApt;      // dword_82F33128 = miAptAuxUpdate
static int32_t giAliasProcessIncoming;    // dword_82F3312C = miViewProcessIncomingEvents
static int32_t giAliasAptAuxRndrTgt;      // dword_82F33130 = miAptAuxRenderTarget
static int32_t giAliasAptAuxUpdTgt;       // dword_82F33134 = miAptAuxUpdateTarget
static int32_t giAliasAptAuxUpdComps;     // dword_82F33138 = miAptAuxUpdateComponents
static int32_t giAliasAptAuxUpdFlshRes;   // dword_82F33140 = miAptAuxUpdateFlashComponentRes
static int32_t giAliasAptAuxUpdFlshNRes;  // dword_82F3313C = miAptAuxUpdateFlashComponentNRes

void GuiPerfmons::Initialise()
{
    // Each AddMonitor: (name, colour=3, min=0, budget_ms, parentHandle, flags).
    // Parent recovered from name indentation; the 3 roots have no parent (-1).
    static const int KI_NO_PARENT = -1;

    // --- Root: GUI MODULE PRE-WORLD UPDATE ---
    miGuiModulePreWorldUpdate = CgsDev::PerfMonCpu::AddMonitor("GUI MODULE PRE-WORLD UPDATE", 3, 0, 0.1, KI_NO_PARENT, 1);

    // --- Root: ENTIRE GUI MODULE UPDATE + subtree ---
    miGuiModuleUpdate = CgsDev::PerfMonCpu::AddMonitor("ENTIRE GUI MODULE UPDATE", 3, 0, 1.9, KI_NO_PARENT, 1);
    miGuiModuleEventPump = CgsDev::PerfMonCpu::AddMonitor("  Gui Module Event Pump", 3, 0, 0.050000001, miGuiModuleUpdate, 1);
    miGuiUpdate = CgsDev::PerfMonCpu::AddMonitor("  CgsGui - Update Total", 3, 0, 1.7, miGuiModuleUpdate, 1);
    miModelUpdate = CgsDev::PerfMonCpu::AddMonitor("    CgsGui - Model Update", 3, 0, 0.40000001, miGuiUpdate, 1);
    miModelUpdate_Input = CgsDev::PerfMonCpu::AddMonitor("      Model Update INPUT", 3, 0, 0.0099999998, miModelUpdate, 1);
    miModelUpdate_Main = CgsDev::PerfMonCpu::AddMonitor("      Model Update MAIN", 3, 0, 0.34999999, miModelUpdate, 1);
    miHudUpdate = CgsDev::PerfMonCpu::AddMonitor("        Gui - HudFlow Update", 3, 0, 0.30000001, miModelUpdate_Main, 1);
    miHudStateUpdate = CgsDev::PerfMonCpu::AddMonitor("          HUD state Update", 3, 0, 0.2, miHudUpdate, 1);
    miSatNavUpdate = CgsDev::PerfMonCpu::AddMonitor("            SatNav Update", 3, 0, 0.02, miHudStateUpdate, 1);
    miMapIconMgrUpdate = CgsDev::PerfMonCpu::AddMonitor("              MapIconMgr Update", 3, 0, 0.0099999998, miSatNavUpdate, 1);
    miMapIconMgrUpdate2 = CgsDev::PerfMonCpu::AddMonitor("              MapIconMgr Upd 2", 3, 0, 0.0099999998, miSatNavUpdate, 1);
    miPlayerPosTableUpdate = CgsDev::PerfMonCpu::AddMonitor("            PlayerPosTbl Update", 3, 0, 0.0099999998, miHudStateUpdate, 1);
    miScreenUpdate = CgsDev::PerfMonCpu::AddMonitor("        Gui - ScreenFlow Update", 3, 0, 0.050000001, miModelUpdate_Main, 1);
    miModelUpdate_Output = CgsDev::PerfMonCpu::AddMonitor("      Model Update OUTPUT", 3, 0, 0.0099999998, miModelUpdate, 1);
    miModelViewBridge = CgsDev::PerfMonCpu::AddMonitor("    CgsGui - Model View Bridge", 3, 0, 0.02, miGuiUpdate, 1);
    miViewUpdate = CgsDev::PerfMonCpu::AddMonitor("    CgsGui - View Update", 3, 0, 1.2, miGuiUpdate, 1);
    miFlaptUpdate = CgsDev::PerfMonCpu::AddMonitor("      FLApt - Update", 3, 0, 0.5, miViewUpdate, 1);
    miViewProcessIncomingEvents = CgsDev::PerfMonCpu::AddMonitor("      Process Incoming Events", 3, 0, 0.0, miViewUpdate, 1);
    miAptAuxUpdate = CgsDev::PerfMonCpu::AddMonitor("      ViewModuleApt", 3, 0, 1.1, miViewUpdate, 1);
    miAptAuxUpdateFlashComponentRes = CgsDev::PerfMonCpu::AddMonitor("        AptAux - Upd Flsh Res", 3, 0, 0.1, miAptAuxUpdate, 1);
    miAptAuxUpdateFlashComponentNRes = CgsDev::PerfMonCpu::AddMonitor("        AptAux - Upd Flsh NRes", 3, 0, 0.1, miAptAuxUpdate, 1);
    miAptAuxUpdateComponents = CgsDev::PerfMonCpu::AddMonitor("        AptAux - Upd Comps", 3, 0, 0.44999999, miAptAuxUpdate, 1);
    miAptAuxUpdateTarget = CgsDev::PerfMonCpu::AddMonitor("        AptAux - Upd Tgt", 3, 0, 0.44999999, miAptAuxUpdate, 1);
    miCgsLanguageManager = CgsDev::PerfMonCpu::AddMonitor("      Language Manager", 3, 0, 0.1, miViewUpdate, 1);

    // --- Root: ENTIRE GUI MODULE RENDER + subtree ---
    miGuiModuleRender = CgsDev::PerfMonCpu::AddMonitor("ENTIRE GUI MODULE RENDER", 3, 0, 2.0, KI_NO_PARENT, 1);
    miFlaptRender = CgsDev::PerfMonCpu::AddMonitor("  FLApt - Render", 3, 0, 1.5, miGuiModuleRender, 0);
    miGuiRender = CgsDev::PerfMonCpu::AddMonitor("  CgsGui - Render", 3, 0, 0.94999999, miGuiModuleRender, 0);
    miCustomRender = CgsDev::PerfMonCpu::AddMonitor("    Custom renderer Render", 3, 0, 50.0, miGuiRender, 0);
    miAptAuxRenderTarget = CgsDev::PerfMonCpu::AddMonitor("    AptAux - Rndr Tgt", 3, 0, 0.89999998, miGuiRender, 0);
    miCgsAptStringRender = CgsDev::PerfMonCpu::AddMonitor("      Flash String render", 3, 0, 0.40000001, miAptAuxRenderTarget, 1);
    miCgsAptDrawRenderingUnit = CgsDev::PerfMonCpu::AddMonitor("      Flash Draw Rendering Unit", 3, 0, 0.40000001, miAptAuxRenderTarget, 1);
    miCgsAptFlashRender = CgsDev::PerfMonCpu::AddMonitor("        Flash Object render", 3, 0, 0.15000001, miCgsAptDrawRenderingUnit, 1);

    // --- Alias copies (X360 dword_82F330xx / 82F331xx = primary handles) ---
    giAliasUpdateTotal = miGuiUpdate;
    giAliasModelUpdate = miModelUpdate;
    giAliasModelViewBridge = miModelViewBridge;
    giAliasViewUpdate = miViewUpdate;
    giAliasGuiRender = miGuiRender;
    giAliasFlashObjectRender = miCgsAptFlashRender;
    giAliasFlashStringRender = miCgsAptStringRender;
    giAliasFlashDrawRendUnit = miCgsAptDrawRenderingUnit;
    giAliasModelUpdateInput = miModelUpdate_Input;
    giAliasModelUpdateMain = miModelUpdate_Main;
    giAliasModelUpdateOutput = miModelUpdate_Output;
    giAliasViewModuleApt = miAptAuxUpdate;
    giAliasProcessIncoming = miViewProcessIncomingEvents;
    giAliasAptAuxRndrTgt = miAptAuxRenderTarget;
    giAliasAptAuxUpdTgt = miAptAuxUpdateTarget;
    giAliasAptAuxUpdComps = miAptAuxUpdateComponents;
    giAliasAptAuxUpdFlshRes = miAptAuxUpdateFlashComponentRes;
    giAliasAptAuxUpdFlshNRes = miAptAuxUpdateFlashComponentNRes;
}
} // namespace BrnGui
