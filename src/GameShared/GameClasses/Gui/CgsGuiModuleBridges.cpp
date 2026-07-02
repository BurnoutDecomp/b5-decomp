#include "GameShared/GameClasses/Gui/CgsGuiModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"             // CgsGuiModuleIO::{Input,Output}Buffer
#include "GameShared/GameClasses/Gui/Model/CgsModelModuleIO.h"     // ModelIO::OutputBuffer
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"      // ViewModule / ViewIO::InputBuffer

// CgsGui::GuiModule -- the per-frame module bridges. Reconstructed from
// BURNOUT_X360_ARTIST.XEX (DWARF primary file
// gameshared/gameclasses/gui/CgsGuiModuleBridges.cpp, cited by the X360 assert paths).
//
// Bodied here (2 ledger functions):
//   GuiModule::BridgeFromInputToView   @0x8285B088  (called by GuiModule::Update)
//   GuiModule::BridgeFromModelToOutput @0x8285DD40  (called by GuiModule::Update and
//                                                    GuiModule::PreWorldUpdate)
//
// BridgeFromInputToView (asm walk): assert the module input buffer (cpp:42; the
// StrStream message build folds to CGS_ASSERT; no early-out), then hand its inbound
// GUI-event queue to the view module at this+0x1B400:
//   ViewModule::AddViewState<32768,16>(lpInput->GetGuiEvents(), lpViewInput).
//
// BridgeFromModelToOutput (asm walk): assert lpOutput (cpp:94, no early-out); assert
// the model output's resource-request queue handle (cpp:96 -- the truncated X360
// symbol "GetGui" is GetGuiResourceRequestQueue, proven by the queue types: its result
// feeds VariableEventQueue<2048,16>::GetLength and the <2048>-appending
// SetGuiResourceRequestQueue). When that queue is non-empty, forward it into the
// module output buffer; then always forward the model's GUI out-events
// (CgsGuiModuleIO::OutputBuffer::AddGuiOutEvents, the 18432-source overload
// @0x8285AE20) and accumulate the model's load notifications into this module's own
// queue at this+0x1B41C (VariableEventQueue<18432,16>::Append<4096,16>).

namespace CgsGui
{
    // @ 0x8285B088
    void GuiModule::BridgeFromInputToView(ViewIO::InputBuffer* lpViewInput,
                                          const CgsGuiModuleIO::InputBuffer* lpInput)
    {
        CGS_ASSERT(lpInput != NULL, "Invalid input queue in GuiModule::BridgeFromInputToView");

        mpViewModule->AddViewState<32768, 16>(lpInput->GetGuiEvents(), lpViewInput);
    }

    // @ 0x8285DD40
    void GuiModule::BridgeFromModelToOutput(CgsGuiModuleIO::OutputBuffer* lpOutput,
                                            const ModelIO::OutputBuffer* lpModelOutput)
    {
        CGS_ASSERT(lpOutput != NULL, "lpOutput != NULL");
        CGS_ASSERT(lpModelOutput->GetGuiResourceRequestQueue() != NULL,
                   "lpModelOutput->GetGuiResourceRequestQueue()");

        // Forward the model's resource requests only when there are any this frame.
        if (lpModelOutput->GetGuiResourceRequestQueue()->GetLength() > 0)
        {
            lpOutput->SetGuiResourceRequestQueue(
                reinterpret_cast<const CgsGuiModuleIO::OutputBuffer::GuiResourceRequestQueueStorage*>(
                    lpModelOutput->GetGuiResourceRequestQueue()));
        }

        // Always forward the model's GUI out-events and accumulate its load
        // notifications into this module's own queue.
        lpOutput->AddGuiOutEvents(lpModelOutput->GetGuiOutEvents());
        mLoadNotifications.Append(*lpModelOutput->GetLoadNotifications());
    }
}
