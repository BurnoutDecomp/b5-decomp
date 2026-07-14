// ===========================================================================
// BrnGuiViewModuleLinkStubs.cpp -- residual GUI view-module link homes.
//
// Out-of-line definitions still needed by the homed CgsGui::ViewModule /
// BrnGui::ViewModule ownership slice. The Flapt lifecycle, timeline update, named
// lookups, and always-available component preparation now live in their real TUs.
// Only rendering remains outside the current boot milestone; the queue methods below
// are faithful specialisations of their VariableEventQueue base lifecycle.
//
// AUDIT (2026-07-14, vs the on-disk tree + the exe source list):
//   - BrnFlapt::MovieClipInstance::Render is the sole residual timeline body here.
//     Construct/GotoFrame/Update and the child/trigger/keyframe machinery are homed
//     in BrnFlaptMovieClipInstance.cpp.
//   - CgsGui::GuiEventQueueBase<256,16>::{Construct,Prepare,Release} -- the tiny GUI
//     output queue the view module owns (mOutputEventQueue). CgsGuiEvent.h declares them
//     out-of-line; no TU instantiates them. Forwarded to the homed CgsModule::
//     VariableEventQueue<256,16> base bodies (the queue's real lifecycle) via explicit
//     member specialisation -- faithful for the lifecycle, marked FLAG for the un-homed
//     GUI-specific override.
//   (FlaptManager, FlaptFileInstance, FlaptRenderer, and the always-available manager
//   are homed to their real TUs and no longer have definitions here.)
// ===========================================================================

#include "types.hpp"

// Pull the real CgsGui::ImRendererSet / FontCollection struct definitions BEFORE
// BrnFlaptManager.h (which only forward-declares them, as `class`), so this TU mangles
// FlaptManager::Construct's struct parameters (PEAU/PEBU) identically to the referencing
// BrnGui::ViewModule TU -- otherwise the class/struct tag flips the decorated name.
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"              // CgsGui::ImRendererSet / FontCollection (struct)
#include "GameSource/Gui/Flapt/BrnFlaptManager.h"                          // BrnFlapt::FlaptManager, FlaptFiles
#include "GameSource/Gui/Flapt/BrnFlaptFileInstance.h"                     // BrnFlapt::FlaptFileInstance
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"                // BrnFlapt::MovieClipInstance (Render below)
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
    void MovieClipInstance::Render(FlaptRenderer*) {}

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
