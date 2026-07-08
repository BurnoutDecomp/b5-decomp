// ===========================================================================
// BrnGuiViewModuleLinkStubs.cpp -- FLAG (GUI view-module / Flapt link stubs).
//
// Minimal out-of-line definitions so the game exe LINKS the real CgsGui::ViewModule /
// BrnGui::ViewModule ownership slice (CgsGuiViewModule.cpp / BrnGuiViewModule.cpp /
// BrnFlaptManager.cpp) without pulling in the still-unreconstructed BrnFlapt engine
// bodies. Every symbol here is referenced by that slice but its real TU is not yet
// homed; each is stubbed so the reference resolves and the current loading-screen ->
// title -> menu boot behaves EXACTLY as it did before the ownership move (the Flapt
// overlay is simply not driven -- the same "missing menu text" visual debt that was
// already noted). Delete each stub (and add the real TU to the source list) as its
// body lands.
//
// AUDIT (2026-07-08, vs the on-disk tree + the exe source list):
//   - BrnFlapt::FlaptManager::{Construct,Prepare,Release,Destruct,RegisterFlaptFile}
//     -- declared in BrnFlaptManager.h; only GetFile/Update/Render are homed (in
//     BrnFlaptManager.cpp). Prepare/Release RETURN TRUE so the view module's prepare/
//     release state machines still advance to DONE.
//   - BrnFlapt::FlaptFileInstance::{Update,Render} and FlaptRenderer::StartRenderingFrame
//     -- referenced by the homed FlaptManager::Update/Render; their own TUs
//     (BrnFlaptFileInstance.cpp / BrnFlaptRenderer.cpp) home OTHER members only and stay
//     out of the build. No-op: the manager ticks/draws nothing until they land.
//   - BrnFlapt::giFlapt{Update,Render}Monitor[Total] -- the PerfMon CPU-monitor handles
//     FlaptManager::Update/Render bracket their timing with. Initialised to -1 (an
//     invalid handle) so PerfMonCpu::Start/StopMonitor no-op instead of scribbling a
//     valid slot (they guard on IsValidHandle). The real handles are AddMonitor'd in the
//     unreconstructed FlaptManager registration TU.
//   - BrnGui::AlwaysAvailableComponentsManager::PrepareFlapt -- has a real body in
//     BrnGuiAlwaysAvailableComponentsManager.cpp, but that body dereferences the far
//     embedded manager (GuiModule + 0x17D670, not modelled by the minimal GuiModule) and
//     calls into more unreconstructed component bodies, so it stays out of the build and
//     is no-op'd here. This keeps the FLAPT-load notification path benign: the offset
//     `this` returned by GetAlwaysAvailableComponentsManager is never dereferenced.
//   - CgsGui::GuiEventQueueBase<256,16>::{Construct,Prepare,Release} -- the tiny GUI
//     output queue the view module owns (mOutputEventQueue). CgsGuiEvent.h declares them
//     out-of-line; no TU instantiates them. Forwarded to the homed CgsModule::
//     VariableEventQueue<256,16> base bodies (the queue's real lifecycle) via explicit
//     member specialisation -- faithful for the lifecycle, marked FLAG for the un-homed
//     GUI-specific override.
// ===========================================================================

#include "types.hpp"

// Pull the real CgsGui::ImRendererSet / FontCollection struct definitions BEFORE
// BrnFlaptManager.h (which only forward-declares them, as `class`), so this TU mangles
// FlaptManager::Construct's struct parameters (PEAU/PEBU) identically to the referencing
// BrnGui::ViewModule TU -- otherwise the class/struct tag flips the decorated name.
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"              // CgsGui::ImRendererSet / FontCollection (struct)
#include "GameSource/Gui/Flapt/BrnFlaptManager.h"                          // BrnFlapt::FlaptManager, FlaptFiles
#include "GameSource/Gui/Flapt/BrnFlaptFileInstance.h"                     // BrnFlapt::FlaptFileInstance
#include "GameSource/Gui/Flapt/BrnFlaptRenderer.h"                         // BrnFlapt::FlaptRenderer
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                          // BrnFlapt::FileRef (PrepareFlapt param)
#include "GameSource/Gui/BrnGuiAlwaysAvailableComponentsManager.h"         // BrnGui::AlwaysAvailableComponentsManager
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"     // CgsResource::ResourceHandle (RegisterFlaptFile by-value param)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                        // CgsGui::GuiEventQueueBase<N,A>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"           // CgsModule::VariableEventQueue<N,A> (forward target)

namespace BrnFlapt
{
    void FlaptManager::RegisterFlaptFile(FlaptFiles, CgsResource::ResourceHandle) {}

    // --- Per-frame drive referenced by the homed FlaptManager::Update/Render --------
    void FlaptFileInstance::Update(f32) {}
    void FlaptFileInstance::Render(FlaptRenderer*) {}
}

namespace BrnGui
{
    // See file-header audit: the real body (BrnGuiAlwaysAvailableComponentsManager.cpp)
    // dereferences the far embedded manager the minimal GuiModule does not model, so it
    // stays out of the build; the empty body keeps the offset `this` untouched.
    void AlwaysAvailableComponentsManager::PrepareFlapt(const BrnFlapt::FileRef&) {}
}

template <> void CgsGui::GuiEventQueueBase<256, 16>::Construct()
{
    this->CgsModule::VariableEventQueue<256, 16>::Construct();
}
template <> bool CgsGui::GuiEventQueueBase<256, 16>::Prepare()
{
    return this->CgsModule::VariableEventQueue<256, 16>::Prepare();
}
template <> bool CgsGui::GuiEventQueueBase<256, 16>::Release()
{
    return this->CgsModule::VariableEventQueue<256, 16>::Release();
}
