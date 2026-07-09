// ============================================================================
// b5-decomp/src/GameShared/GameClasses/Gui/View/CgsGuiViewModule_AddViewState_Inst.cpp
//
// The two X360-attested explicit instantiations of the member template
//   CgsGui::ViewModule::AddViewState<BUFSIZE,16>(const GuiEventQueueBase<BUFSIZE,16>*,
//                                                ViewIO::InputBuffer*)
//     AddViewState<32768,16> @ 0x82859E28
//     AddViewState<65536,16> @ 0x82859F80
// (GuiModule::BridgeFromInputToView drives them: append one frame's inbound GUI-event
// queue into the view input buffer's view-state queue.)
//
// The single shared body is defined out-of-line here (the owning header CgsGuiViewModule.h
// keeps it declaration-only -- the body needs the InputBuffer + VariableEventQueue collaborator
// homes that must not cascade into that skeleton header). Both instantiations compile to the
// identical sequence, differing only in the source queue capacity BUFSIZE (which selects the
// VariableEventQueue<65536,16>::AppendSafe<BUFSIZE,16> the destination view-state queue runs).
// Reconstructed store-for-store from the X360 pseudocode+asm.
// ============================================================================

#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"     // CgsGui::ViewModule (owner)
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModuleIO.h"   // ViewIO::InputBuffer + ViewStateQueue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // AppendSafe / OutputQueueContents
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT

namespace CgsGui
{
    // AddViewState<BUFSIZE,ALIGN> @ 0x82859E28 (<32768,16>) / 0x82859F80 (<65536,16>).
    // Assert the source queue + input buffer are valid, fetch the input buffer's view-state
    // queue (write-locked handle), then AppendSafe the whole source queue into it. On overflow
    // (AppendSafe returns false) the X360 dumps the destination's contents for the log; either
    // way the trailing assert fires when the append did not take.
    template <s32 BUFSIZE, s32 ALIGN>
    void ViewModule::AddViewState(const GuiEventQueueBase<BUFSIZE, ALIGN>* lpGuiEvents,
                                  ViewIO::InputBuffer* lpInput)
    {
        CGS_ASSERT(lpGuiEvents != nullptr,
                   "Invalid gui queue pointer in ViewModule::AddViewState");
        CGS_ASSERT(lpInput != nullptr,
                   "ViewModule is most likely not locked for output in ViewModule::AddViewState");

        ViewIO::InputBuffer::ViewStateQueue& lrViewStateQueue = lpInput->GetViewStateQueue();

        bool lbAppended = false;
        if (lrViewStateQueue.AppendSafe(*lpGuiEvents))
            lbAppended = true;
        else
            lrViewStateQueue.OutputQueueContents();

        CGS_ASSERT(lbAppended,
                   "lpInput->GetViewStateQueue()->AppendGuiQueueSafe( *lpGuiViewStateQueue )");
    }

    template void ViewModule::AddViewState<32768, 16>(const GuiEventQueueBase<32768, 16>*, ViewIO::InputBuffer*);  // 0x82859E28
    template void ViewModule::AddViewState<65536, 16>(const GuiEventQueueBase<65536, 16>*, ViewIO::InputBuffer*);  // 0x82859F80
}
