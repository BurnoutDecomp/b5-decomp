#pragma once
// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGUIToX.h
//
// BrnGame::BrnGameModule GUI-output bridge family (DWARF home
// GameSource/Game/GameBridgeGUIToX.cpp). Each per-frame bridge walks the GUI
// output buffer's out-event queue (CgsGui::CgsGuiModuleIO::OutputBuffer's
// mOutEvents, a VariableEventQueue<18432,16> @ +0x814) and re-publishes /
// translates the queued GUI events into a downstream subsystem's INPUT event queue.
//
// GUI-OUTPUT-QUEUE REACH (ODR): CgsGui::CgsGuiModuleIO::OutputBuffer is an empty
// PLACEHOLDER in BrnGameModule.hpp, so this header cannot #include the real
// CgsGuiModuleIO.h. We reach the out-event queue by its proven +0x814 byte offset
// (committed CgsGuiModuleIO_OutputBuffer.cpp: mOutEvents @0x0814), mirroring the
// GetGuiInputEventQueue precedent in GameBridgeReplayToX.h.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

namespace CgsGui { namespace CgsGuiModuleIO { struct OutputBuffer; } }

namespace BrnGame
{
    inline const CgsModule::VariableEventQueue<18432, 16>* GetGuiOutEventQueue(
        const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer)
    {
        return reinterpret_cast<const CgsModule::VariableEventQueue<18432, 16>*>(
            reinterpret_cast<const unsigned char*>(lpGuiOutputBuffer) + 0x814);
    }
}
