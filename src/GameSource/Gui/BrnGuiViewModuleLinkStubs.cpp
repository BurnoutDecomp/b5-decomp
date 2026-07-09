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
// AUDIT (2026-07-09b, vs the on-disk tree + the exe source list):
//   - BrnFlapt::MovieClipInstance::{Construct,GotoFrame,Update,Render} -- the
//     timeline layer the homed registration/drive path reaches (SetData @0x82471620
//     constructs + rewinds the root clip; FlaptFileInstance::Update/Render
//     @0x82471820/@0x82472480 tick/draw it). Their real bodies are the big
//     unreconstructed timeline TUs. No-op: a registered instance goes active but its
//     clip tree neither composes nor draws until they land.
//   - BrnGui::AlwaysAvailableComponentsManager::PrepareFlapt -- has a real body in
//     BrnGuiAlwaysAvailableComponentsManager.cpp, but that body dereferences the far
//     embedded manager (GuiModule + 0x17D670) and the PC-minimal GuiModule does NOT
//     CONTAIN that component block at all -- the accessor's offset `this` points
//     outside the object, so enabling the real body is STRUCTURALLY GATED on
//     reconstructing the GuiModule component block (not just on this stub). No-op'd
//     here; the FLAPT-load notification path stays benign (the offset `this` is
//     never dereferenced).
//   - CgsGui::GuiEventQueueBase<256,16>::{Construct,Prepare,Release} -- the tiny GUI
//     output queue the view module owns (mOutputEventQueue). CgsGuiEvent.h declares them
//     out-of-line; no TU instantiates them. Forwarded to the homed CgsModule::
//     VariableEventQueue<256,16> base bodies (the queue's real lifecycle) via explicit
//     member specialisation -- faithful for the lifecycle, marked FLAG for the un-homed
//     GUI-specific override.
//   (FlaptManager Construct/Prepare/Release/Destruct, FlaptFileInstance Update/Render,
//   FlaptRenderer::StartRenderingFrame and the giFlapt* monitor handles were homed to
//   their real TUs -- BrnFlaptManager.cpp / BrnFlaptFileInstance.cpp /
//   BrnFlaptRenderer.cpp -- and no longer live here.)
// ===========================================================================

#include "types.hpp"

// Pull the real CgsGui::ImRendererSet / FontCollection struct definitions BEFORE
// BrnFlaptManager.h (which only forward-declares them, as `class`), so this TU mangles
// FlaptManager::Construct's struct parameters (PEAU/PEBU) identically to the referencing
// BrnGui::ViewModule TU -- otherwise the class/struct tag flips the decorated name.
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"              // CgsGui::ImRendererSet / FontCollection (struct)
#include "GameSource/Gui/Flapt/BrnFlaptManager.h"                          // BrnFlapt::FlaptManager, FlaptFiles
#include "GameSource/Gui/Flapt/BrnFlaptFileInstance.h"                     // BrnFlapt::FlaptFileInstance
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"                // BrnFlapt::MovieClipInstance (Update/Render stubs below)
#include "GameSource/Gui/Flapt/BrnFlaptRenderer.h"                         // BrnFlapt::FlaptRenderer
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                          // BrnFlapt::FileRef (PrepareFlapt param)
#include "GameSource/Gui/BrnGuiAlwaysAvailableComponentsManager.h"         // BrnGui::AlwaysAvailableComponentsManager
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"     // CgsResource::ResourceHandle (RegisterFlaptFile by-value param)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                        // CgsGui::GuiEventQueueBase<N,A>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"           // CgsModule::VariableEventQueue<N,A> (forward target)

namespace BrnFlapt
{
    // --- Timeline bodies referenced by the homed FlaptFileInstance --------------
    // (SetData @0x82471620 constructs/rewinds the root clip; Update/Render drive it.)
    void MovieClipInstance::Construct(const MovieClip*, const char*, MovieClipInstance*,
                                      CgsMemory::LinearMalloc*, const FlaptRenderer*,
                                      const RGBA*, s32) {}
    void MovieClipInstance::GotoFrame(u32) {}
    void MovieClipInstance::Update(f32) {}
    void MovieClipInstance::Render(FlaptRenderer*) {}
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
