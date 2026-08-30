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
// The former empty OutputBuffer placeholder has been retired. Reach the queue through
// the real X360 accessor so its read-lock contract remains part of every bridge.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"

namespace BrnGame
{
    inline const CgsModule::VariableEventQueue<18432, 16>* GetGuiOutEventQueue(
        const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer)
    {
        return lpGuiOutputBuffer->GetOutEventQueue();
    }
}
