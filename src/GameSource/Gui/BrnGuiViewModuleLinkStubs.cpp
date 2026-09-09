// ===========================================================================
// BrnGuiViewModuleLinkStubs.cpp -- the CgsGui::GuiEventQueueBase<N,A> member
// specialisations the GUI module group needs and no other TU instantiates. Each is a
// thin forwarder to the homed CgsModule::VariableEventQueue<N,A> base body (the console
// emits one body per instantiation). FLAG for the un-homed GUI-specific override, if any.
// ===========================================================================

#include "types.hpp"

// Pull the real CgsGui::ImRendererSet / FontCollection struct definitions BEFORE
// BrnFlaptManager.h (which only forward-declares them, as `class`), so this TU mangles
// FlaptManager::Construct's struct parameters (PEAU/PEBU) identically to the referencing
// BrnGui::ViewModule TU -- otherwise the class/struct tag flips the decorated name.
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"              // CgsGui::ImRendererSet / FontCollection (struct)
#include "GameSource/Gui/Flapt/BrnFlaptManager.h"                          // BrnFlapt::FlaptManager, FlaptFiles
#include "GameSource/Gui/Flapt/BrnFlaptFileInstance.h"                     // BrnFlapt::FlaptFileInstance
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"                // BrnFlapt::MovieClipInstance
#include "GameSource/Gui/Flapt/BrnFlaptRenderer.h"                         // BrnFlapt::FlaptRenderer
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                          // BrnFlapt::FileRef
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"                     // BrnFlapt::MovieClipRef
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                     // BrnFlapt::TextFieldRef
#include "GameSource/Gui/BrnGuiAlwaysAvailableComponentsManager.h"         // BrnGui::AlwaysAvailableComponentsManager
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"     // CgsResource::ResourceHandle
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                        // CgsGui::GuiEventQueueBase<N,A>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"           // CgsModule::VariableEventQueue<N,A> (forward target)

// <32768,16>: the GUI module input buffer's event queue (BrnGuiModule::Prepare constructs
// it; HudMessageAnalyzer::Update reads it; BrnGuiModule::Update's model-input drain clears it).
template <> void CgsGui::GuiEventQueueBase<32768, 16>::Construct()
{
    this->CgsModule::VariableEventQueue<32768, 16>::Construct();
}
template <> s32 CgsGui::GuiEventQueueBase<32768, 16>::GetFirstEvent(
    const CgsModule::Event** lppEvent, s32* lpiSize) const
{
    return this->CgsModule::VariableEventQueue<32768, 16>::GetFirstEvent(lppEvent, lpiSize);
}
template <> s32 CgsGui::GuiEventQueueBase<32768, 16>::GetNextEvent(
    const CgsModule::Event* lpEvent, const CgsModule::Event** lppNextEvent, s32* lpiSize) const
{
    return this->CgsModule::VariableEventQueue<32768, 16>::GetNextEvent(lpEvent, lppNextEvent, lpiSize);
}
template <> void CgsGui::GuiEventQueueBase<32768, 16>::Clear()
{
    this->CgsModule::VariableEventQueue<32768, 16>::Clear();
}

// <4096,16>: the GUI flow controller's load-request queue, and the event queue embedded in
// CgsGui::CustomRendererManager (whose Destruct is this instantiation's Destruct).
template <> void CgsGui::GuiEventQueueBase<4096, 16>::Construct()
{
    this->CgsModule::VariableEventQueue<4096, 16>::Construct();
}
template <> void CgsGui::GuiEventQueueBase<4096, 16>::Clear()
{
    this->CgsModule::VariableEventQueue<4096, 16>::Clear();
}
template <> void CgsGui::GuiEventQueueBase<4096, 16>::Destruct()
{
    this->CgsModule::VariableEventQueue<4096, 16>::Destruct();
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

// <18432,16>: the GUI resource module's IO pair (CgsGuiResourceModuleIO::InputBuffer::
// mLoadRequests + OutputBuffer::mLoadNotifications), wired by BrnGuiModule::DispatchGuiResourceModule.
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

// <256,16>: the view module's output queue (mOutputEventQueue); CgsGuiEvent.h declares
// these out-of-line.
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
