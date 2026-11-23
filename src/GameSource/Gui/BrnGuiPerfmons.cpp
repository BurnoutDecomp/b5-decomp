#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.h"
#include "GameSource/Gui/BrnGuiPerfmons.h"
#include "GameShared/GameClasses/Gui/CgsGuiModule.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"
#include "types.hpp"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // CgsDev::PerfMonCpu::AddMonitor

// Reconstructed from BURNOUT_X360_ARTIST.XEX (BrnGui::GuiPerfmons::Initialise @0x824EF050).
//
// Registers the 33 GUI monitors and publishes the shared handle aliases. ARTIST uses
// r3/r4/r5/f1/r7 for AddMonitor; the apparent r6 parent argument is a decompiler
// artefact. Indentation belongs to the displayed name, not a separate parent tree.

// The original secondary stores in Initialise824EF5F0..664/670/67C
// publish into these shared owners. They are not private alias variables.
namespace CgsGui
{
s32 GuiModule::miGuiPM;
s32 GuiModule::miModelUpdatePM;
s32 GuiModule::miModelViewBridgePM;
s32 GuiModule::miViewUpdatePM;
s32 GuiModule::miGuiRenderPM;
s32 ModelModule::miUpdateInputPM;
s32 ModelModule::miUpdateMainPM;
s32 ModelModule::miUpdateOutputPM;
s32 ViewModule::miAptAuxUpdatePM;
s32 ViewModule::miViewProcessIncomingEventsPM;
}

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























// (layout: the next definition sits on its DecFIGS line, BrnGuiPerfmons.cpp:91)
void GuiPerfmons::Initialise()
{
    // --- Root: GUI MODULE PRE-WORLD UPDATE ---
    miGuiModulePreWorldUpdate = CgsDev::PerfMonCpu::AddMonitor("GUI MODULE PRE-WORLD UPDATE", CgsDev::E_PMP_3, false, 0.1f, true);

    // --- Root: ENTIRE GUI MODULE UPDATE + subtree ---
    miGuiModuleUpdate = CgsDev::PerfMonCpu::AddMonitor("ENTIRE GUI MODULE UPDATE", CgsDev::E_PMP_3, false, 1.9f, true);
    miGuiModuleEventPump = CgsDev::PerfMonCpu::AddMonitor("  Gui Module Event Pump", CgsDev::E_PMP_3, false, 0.050000001f, true);
    miGuiUpdate = CgsDev::PerfMonCpu::AddMonitor("  CgsGui - Update Total", CgsDev::E_PMP_3, false, 1.7f, true);
    miModelUpdate = CgsDev::PerfMonCpu::AddMonitor("    CgsGui - Model Update", CgsDev::E_PMP_3, false, 0.40000001f, true);
    miModelUpdate_Input = CgsDev::PerfMonCpu::AddMonitor("      Model Update INPUT", CgsDev::E_PMP_3, false, 0.0099999998f, true);
    miModelUpdate_Main = CgsDev::PerfMonCpu::AddMonitor("      Model Update MAIN", CgsDev::E_PMP_3, false, 0.34999999f, true);
    miHudUpdate = CgsDev::PerfMonCpu::AddMonitor("        Gui - HudFlow Update", CgsDev::E_PMP_3, false, 0.30000001f, true);
    miHudStateUpdate = CgsDev::PerfMonCpu::AddMonitor("          HUD state Update", CgsDev::E_PMP_3, false, 0.2f, true);
    miSatNavUpdate = CgsDev::PerfMonCpu::AddMonitor("            SatNav Update", CgsDev::E_PMP_3, false, 0.02f, true);
    miMapIconMgrUpdate = CgsDev::PerfMonCpu::AddMonitor("              MapIconMgr Update", CgsDev::E_PMP_3, false, 0.0099999998f, true);
    miMapIconMgrUpdate2 = CgsDev::PerfMonCpu::AddMonitor("              MapIconMgr Upd 2", CgsDev::E_PMP_3, false, 0.0099999998f, true);
    miPlayerPosTableUpdate = CgsDev::PerfMonCpu::AddMonitor("            PlayerPosTbl Update", CgsDev::E_PMP_3, false, 0.0099999998f, true);
    miScreenUpdate = CgsDev::PerfMonCpu::AddMonitor("        Gui - ScreenFlow Update", CgsDev::E_PMP_3, false, 0.050000001f, true);
    miModelUpdate_Output = CgsDev::PerfMonCpu::AddMonitor("      Model Update OUTPUT", CgsDev::E_PMP_3, false, 0.0099999998f, true);
    miModelViewBridge = CgsDev::PerfMonCpu::AddMonitor("    CgsGui - Model View Bridge", CgsDev::E_PMP_3, false, 0.02f, true);
    miViewUpdate = CgsDev::PerfMonCpu::AddMonitor("    CgsGui - View Update", CgsDev::E_PMP_3, false, 1.2f, true);
    miFlaptUpdate = CgsDev::PerfMonCpu::AddMonitor("      FLApt - Update", CgsDev::E_PMP_3, false, 0.5f, true);
    miViewProcessIncomingEvents = CgsDev::PerfMonCpu::AddMonitor("      Process Incoming Events", CgsDev::E_PMP_3, false, 0.0f, true);
    miAptAuxUpdate = CgsDev::PerfMonCpu::AddMonitor("      ViewModuleApt", CgsDev::E_PMP_3, false, 1.1f, true);
    miAptAuxUpdateFlashComponentRes = CgsDev::PerfMonCpu::AddMonitor("        AptAux - Upd Flsh Res", CgsDev::E_PMP_3, false, 0.1f, true);
    miAptAuxUpdateFlashComponentNRes = CgsDev::PerfMonCpu::AddMonitor("        AptAux - Upd Flsh NRes", CgsDev::E_PMP_3, false, 0.1f, true);
    miAptAuxUpdateComponents = CgsDev::PerfMonCpu::AddMonitor("        AptAux - Upd Comps", CgsDev::E_PMP_3, false, 0.44999999f, true);
    miAptAuxUpdateTarget = CgsDev::PerfMonCpu::AddMonitor("        AptAux - Upd Tgt", CgsDev::E_PMP_3, false, 0.44999999f, true);
    miCgsLanguageManager = CgsDev::PerfMonCpu::AddMonitor("      Language Manager", CgsDev::E_PMP_3, false, 0.1f, true);

    // --- Root: ENTIRE GUI MODULE RENDER + subtree ---
    miGuiModuleRender = CgsDev::PerfMonCpu::AddMonitor("ENTIRE GUI MODULE RENDER", CgsDev::E_PMP_3, false, 2.0f, true);
    miFlaptRender = CgsDev::PerfMonCpu::AddMonitor("  FLApt - Render", CgsDev::E_PMP_3, false, 1.5f, false);
    miGuiRender = CgsDev::PerfMonCpu::AddMonitor("  CgsGui - Render", CgsDev::E_PMP_3, false, 0.94999999f, false);
    miCustomRender = CgsDev::PerfMonCpu::AddMonitor("    Custom renderer Render", CgsDev::E_PMP_3, false, 50.0f, false);
    miAptAuxRenderTarget = CgsDev::PerfMonCpu::AddMonitor("    AptAux - Rndr Tgt", CgsDev::E_PMP_3, false, 0.89999998f, false);
    miCgsAptStringRender = CgsDev::PerfMonCpu::AddMonitor("      Flash String render", CgsDev::E_PMP_3, false, 0.40000001f, true);
    miCgsAptDrawRenderingUnit = CgsDev::PerfMonCpu::AddMonitor("      Flash Draw Rendering Unit", CgsDev::E_PMP_3, false, 0.40000001f, true);
    miCgsAptFlashRender = CgsDev::PerfMonCpu::AddMonitor("        Flash Object render", CgsDev::E_PMP_3, false, 0.15000001f, true);

    // --- Alias copies (X360 dword_82F330xx / 82F331xx = primary handles) ---
    CgsGui::GuiModule::miGuiPM = miGuiUpdate;
    CgsGui::GuiModule::miModelUpdatePM = miModelUpdate;
    CgsGui::GuiModule::miModelViewBridgePM = miModelViewBridge;
    CgsGui::GuiModule::miViewUpdatePM = miViewUpdate;
    CgsGui::GuiModule::miGuiRenderPM = miGuiRender;
    CgsGui::AptRenderHandler::miCgsAptFlashRender = miCgsAptFlashRender;
    CgsGui::AptRenderHandler::miCgsAptStringRender = miCgsAptStringRender;
    CgsGui::AptRenderHandler::miCgsAptDrawRenderingUnit = miCgsAptDrawRenderingUnit;
    CgsGui::ModelModule::miUpdateInputPM = miModelUpdate_Input;
    CgsGui::ModelModule::miUpdateMainPM = miModelUpdate_Main;
    CgsGui::ModelModule::miUpdateOutputPM = miModelUpdate_Output;
    CgsGui::ViewModule::miAptAuxUpdatePM = miAptAuxUpdate;
    CgsGui::ViewModule::miViewProcessIncomingEventsPM = miViewProcessIncomingEvents;
    CgsGui::AptAux::miAptAuxRenderTargetPM = miAptAuxRenderTarget;
    CgsGui::AptAux::miAptAuxUpdateTargetPM = miAptAuxUpdateTarget;
    CgsGui::AptAux::miAptAuxUpdateComponentsPM = miAptAuxUpdateComponents;
    CgsGui::AptAux::miAptAuxUpdateFlashComponentReservedPM = miAptAuxUpdateFlashComponentRes;
    CgsGui::AptAux::miAptAuxUpdateFlashComponentNonReservedPM = miAptAuxUpdateFlashComponentNRes;
}
} // namespace BrnGui
