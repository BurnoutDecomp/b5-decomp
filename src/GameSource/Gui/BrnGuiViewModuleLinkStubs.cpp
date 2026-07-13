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
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"                     // BrnFlapt::MovieClipRef (lookup-stub out handles)
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                     // BrnFlapt::TextFieldRef (FindChildTextField stub out handle)
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

    // --- Timeline lookup/playback bodies referenced by the homed Ref layer ------
    // (BrnFlaptMovieClipRef.cpp / BrnFlaptFileRef.cpp / the FlaptComponents, pulled
    // in by the overlay-flow closure 2026-07-12.) Real bodies are the same big
    // unreconstructed timeline TUs as the block above:
    //   FindChildMovieClip @0x8246B848 / FindChildTextField @0x8246BAC0 /
    //   FindChildMovieClipOnFrame @0x8246BD50 / TryFindChildComponentRecursively
    //   @0x8246C020 / GetParent @0x8246C250 / GetTriggerParameters @0x8246C610 /
    //   ResetTimeline @0x8246B710 / SetFrameTriggerCallback @0x8246B740 /
    //   GotoAndPlayLabel @0x8246F228 / GotoAndStopLabel @0x8246F2D8 /
    //   FlaptFileInstance::FindComponent @0x8246E958.
    // The sret-out lookups write the INVALID handle (never stack garbage) so the
    // Ref layer's own "mpMovieClipInst" asserts report the un-driven overlay
    // deterministically; the playback entries no-op (the clip tree neither
    // composes nor draws until the timeline TUs land -- the documented visual debt).
    void MovieClipInstance::FindChildMovieClip(u32, MovieClipRef* lpOutRef, const char*)
    {
        lpOutRef->SetInvalid();
    }
    void MovieClipInstance::FindChildMovieClipOnFrame(u32, MovieClipRef* lpOutRef, const char*)
    {
        lpOutRef->SetInvalid();
    }
    void MovieClipInstance::FindChildTextField(u32, TextFieldRef* lpOutRef, const char*)
    {
        lpOutRef->SetInvalid();
    }
    bool MovieClipInstance::TryFindChildComponentRecursively(u32, MovieClipRef* lpOutRef, const char*)
    {
        lpOutRef->SetInvalid();
        return false;
    }
    void MovieClipInstance::GetParent(MovieClipRef* lpOutRef)
    {
        lpOutRef->SetInvalid();
    }
    void MovieClipInstance::ResetTimeline() {}
    void MovieClipInstance::GotoAndPlayLabel(u32, const char*) {}
    void MovieClipInstance::GotoAndStopLabel(u32, const char*) {}
    void MovieClipInstance::SetFrameTriggerCallback(FrameTriggerCallback, void*) {}
    const TriggerParameters* MovieClipInstance::GetTriggerParameters() const
    {
        return 0;
    }

    MovieClipRef* FlaptFileInstance::FindComponent(u32, MovieClipRef* lpOutRef, const char*) const
    {
        lpOutRef->SetInvalid();
        return lpOutRef;
    }
}

namespace BrnGui
{
    // See file-header audit: the real body (BrnGuiAlwaysAvailableComponentsManager.cpp)
    // dereferences the far embedded manager the minimal GuiModule does not model, so it
    // stays out of the build; the empty body keeps the offset `this` untouched.
    void AlwaysAvailableComponentsManager::PrepareFlapt(const BrnFlapt::FileRef&) {}
}

// The ModelIO buffer queues the GUI flow controller's IO pair constructs/drains
// (BrnGuiModule::Prepare / ServiceFsmBundleRequests): the 32768 inbound event queue and
// the 4096 load-request queue. Same thin forwarders to the VariableEventQueue base as
// the <256,16> family below (the X360 emits one body per instantiation).
template <> void CgsGui::GuiEventQueueBase<32768, 16>::Construct()
{
    this->CgsModule::VariableEventQueue<32768, 16>::Construct();
}
template <> void CgsGui::GuiEventQueueBase<4096, 16>::Construct()
{
    this->CgsModule::VariableEventQueue<4096, 16>::Construct();
}
template <> void CgsGui::GuiEventQueueBase<4096, 16>::Clear()
{
    this->CgsModule::VariableEventQueue<4096, 16>::Clear();
}
template <> s32 CgsGui::GuiEventQueueBase<4096, 16>::GetFirstEvent(
    const CgsModule::Event** lppEvent, s32* lpiSize) const
{
    return this->CgsModule::VariableEventQueue<4096, 16>::GetFirstEvent(lppEvent, lpiSize);
}
template <> s32 CgsGui::GuiEventQueueBase<4096, 16>::GetNextEvent(
    const CgsModule::Event* lpEvent, const CgsModule::Event** lppNextEvent, s32* lpiSize) const
{
    return this->CgsModule::VariableEventQueue<4096, 16>::GetNextEvent(lpEvent, lppNextEvent, lpiSize);
}

// The GuiResourceModuleIO buffer queues the GUI resource module's IO pair constructs/
// drains (CgsGuiResourceModuleIO::InputBuffer::mLoadRequests +
// OutputBuffer::mLoadNotifications, both GuiEventQueueBase<18432,16>). The module was
// "not yet wired" until BrnGuiModule::DispatchGuiResourceModule wired it, so this 18432
// specialisation had never been instantiated; same thin forwarders to the
// VariableEventQueue base as the families above (the X360 emits one body per instantiation).
template <> void CgsGui::GuiEventQueueBase<18432, 16>::Construct()
{
    this->CgsModule::VariableEventQueue<18432, 16>::Construct();
}
template <> void CgsGui::GuiEventQueueBase<18432, 16>::Clear()
{
    this->CgsModule::VariableEventQueue<18432, 16>::Clear();
}
template <> s32 CgsGui::GuiEventQueueBase<18432, 16>::GetFirstEvent(
    const CgsModule::Event** lppEvent, s32* lpiSize) const
{
    return this->CgsModule::VariableEventQueue<18432, 16>::GetFirstEvent(lppEvent, lpiSize);
}
template <> s32 CgsGui::GuiEventQueueBase<18432, 16>::GetNextEvent(
    const CgsModule::Event* lpEvent, const CgsModule::Event** lppNextEvent, s32* lpiSize) const
{
    return this->CgsModule::VariableEventQueue<18432, 16>::GetNextEvent(lpEvent, lppNextEvent, lpiSize);
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
