// ===========================================================================
// CgsGuiEventQueue.cpp -- out-of-line members of CgsGui::GuiEventQueueBase<N,A>,
// declared by CgsGuiEvent.h. GuiEventQueueBase adds no storage and no behaviour of its
// own over CgsModule::VariableEventQueue<N,A>: each member forwards to the base body,
// which is homed in GameShared\GameClasses\Module\VariableEventQueue_<N>_16.cpp.
// One block per instantiation the GUI module group uses.
// ===========================================================================

#include "types.hpp"

#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                // CgsGui::GuiEventQueueBase<N,A>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::VariableEventQueue<N,A> (forward target)

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
