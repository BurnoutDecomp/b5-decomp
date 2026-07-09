// ============================================================================
// b5-decomp/src/GameShared/GameClasses/Gui/CgsGuiModuleIO_AddGuiOutEvent_Inst.cpp
//
// The 18 X360-attested explicit instantiations of
// CgsGui::CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<T>. The single shared body lives in
// CgsGuiModuleIO.h; each instantiation compiles to the identical assert-locked-for-writing +
// mOutEvents.AddEvent(&event, T::GetEventType(), sizeof(T)) sequence over the module output
// buffer's VariableEventQueue<18432,16> GUI-out queue, differing only in the per-T (id,size).
// ============================================================================

#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"

namespace CgsGui
{
    template int CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<BrnGui::CalculateRoute>(BrnGui::CalculateRoute&);  // 0x8250C430
    template int CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<BrnGui::GuiAudioTriggerEvent>(BrnGui::GuiAudioTriggerEvent&);  // 0x8250BE70
    template int CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<BrnGui::GuiEventKeyboardResponse>(BrnGui::GuiEventKeyboardResponse&);  // 0x8250BDB8
    template int CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<BrnGui::GuiEventRequestCollisionWorldEvent>(BrnGui::GuiEventRequestCollisionWorldEvent&);  // 0x8250BD00
    template int CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<BrnGui::GuiEventRequestTraining>(BrnGui::GuiEventRequestTraining&);  // 0x8250BFE0
    template int CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<BrnGui::GuiNewBurnoutHudMessageEvent>(BrnGui::GuiNewBurnoutHudMessageEvent&);  // 0x8250C378
    template int CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<BrnGui::GuiOptionsBrightnessContrastPostFxControl>(BrnGui::GuiOptionsBrightnessContrastPostFxControl&);  // 0x82465D98
    template int CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<BrnGui::GuiOverlayRequest>(BrnGui::GuiOverlayRequest&);  // 0x8250BF28
    template int CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<BrnGui::GuiOverlayWaitFinishRequest>(BrnGui::GuiOverlayWaitFinishRequest&);  // 0x82512EC8
    template int CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<BrnGui::GuiPFXHookEnumeration>(BrnGui::GuiPFXHookEnumeration&);  // 0x8250C4E8
    template int CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<BrnGui::GuiReplayRegisterSerialiser>(BrnGui::GuiReplayRegisterSerialiser&);  // 0x8250BC48
    template int CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<BrnGui::GuiResponseTimeDateString>(BrnGui::GuiResponseTimeDateString&);  // 0x8250C150
    template int CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<CgsGui::GuiEvent<89>>(CgsGui::GuiEvent<89> &);  // 0x8250C2C0
    template int CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<CgsGui::GuiEventGetLanguage>(CgsGui::GuiEventGetLanguage&);  // 0x8250C098
    template int CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<CgsGui::GuiEventPlayMusicOnMenuStream>(CgsGui::GuiEventPlayMusicOnMenuStream&);  // 0x8250C208
    template int CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<CgsGui::GuiEventSetCircleButtonConfig>(CgsGui::GuiEventSetCircleButtonConfig&);  // 0x82859D70
    template int CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<CgsGui::GuiEventSetLanguageNotification>(CgsGui::GuiEventSetLanguageNotification&);  // 0x82866200
    template int CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<CgsGui::GuiEventSetSku>(CgsGui::GuiEventSetSku&);  // 0x82866148
}
