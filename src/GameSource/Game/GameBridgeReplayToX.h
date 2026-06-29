#pragma once
// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeReplayToX.h
//
// Support types for the BrnGame::BrnGameModule replay-bridge family
// (GameSource/Unity/../Game/GameBridgeReplayToX.cpp). Each per-frame bridge reads
// the replay module's pre/post-sim OUTPUT buffer (BrnReplays::ReplayIO::
// OutputBuffer_PreSim / OutputBuffer_PostSim, the committed homes) and republishes
// its contents into a downstream subsystem's INPUT buffer.
//
// This header carries ONLY what the first reconstructed bridge (BridgeReplayToGui,
// X360 0x823E7210) needs that is not already homed elsewhere:
//
//   * BrnGui::GuiReplayStatusEvent -- the GUI event the bridge synthesises from the
//     replay status interface. DWARF home GameSource/Gui/BrnGuiEventTypeDefs.h:6261
//     declares it verbatim as
//         struct GuiReplayStatusEvent : public CgsGui::GuiEvent<514>
//         { BrnReplays::ReplayIO::StatusInterface mInterface; };
//     i.e. a GuiEvent<514> (CgsGuiEvent.h, the real event-id base) that boxes a copy
//     of the committed BrnReplays::ReplayIO::StatusInterface. Both the event-type id
//     (514) and the embedded interface type are REAL (attested by the DWARF + the
//     committed StatusInterface home) -- nothing here is fabricated. It is declared
//     in this bridge-local support header (mirroring how GameBridgeControllerToX.h
//     keeps its own GUI-event payloads bridge-local) rather than grown into the
//     narrow committed BrnGuiEventTypeDefs.h slice, so the heavy CgsGuiEvent.h +
//     BrnReplayStatusInterface.h includes are not forced onto every GUI TU.
//
//   * GetGuiInputEventQueue() -- reach the GUI module INPUT buffer's inbound event
//     queue (the 32768-byte GuiEventQueueBase<32768,16>) by name. The X360 accessor
//     (sub_8284F238) read/write-locks the CgsGui::CgsGuiModuleIO::InputBuffer and
//     returns its mInputQueue member at this+4 (DWARF CgsGuiModuleIO.h:118; committed
//     CgsGuiModuleIO.h InputBuffer::mInputQueue @ +0x0004). The real InputBuffer type
//     cannot be #included here because the keystone BrnGameModule.hpp defines an empty
//     CgsGui::CgsGuiModuleIO::InputBuffer placeholder (an ODR redefinition would
//     result); so the queue is reached by its proven +4 byte offset. FLAG: the lock
//     handshake the X360 accessor performs is not reproduced (the placeholder
//     InputBuffer has no lock byte surface); the +4 member offset and the queue type
//     are byte-faithful to the committed InputBuffer layout.
//
// The CgsGui::GuiModule placeholder + its AddGuiEvent<T>(event, outputBuffer) template
// the bridge pushes the event through are the committed ones from
// GameBridgeControllerToX.h (#included by the .cpp); this header does not redefine them.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"            // CgsGui::GuiEvent<514>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // CgsModule::VariableEventQueue<32768,16>
#include "GameSource/Replays/BrnReplayStatusInterface.h"       // BrnReplays::ReplayIO::StatusInterface

// Forward-declare the placeholder GUI input buffer (defined empty in BrnGameModule.hpp).
// We only ever take its address + apply the proven +4 member offset, so a forward
// declaration is sufficient and avoids depending on either the placeholder or the real
// (ODR-conflicting) CgsGuiModuleIO.h definition from this header.
namespace CgsGui { namespace CgsGuiModuleIO { struct InputBuffer; } }

namespace BrnGui
{
    // DWARF: GameSource/Gui/BrnGuiEventTypeDefs.h:6261. A GuiEvent<514> that boxes a
    // copy of the replay status interface. The bridge fills mInterface (operator= from
    // the replay output buffer's status interface) then enqueues the whole event.
    struct GuiReplayStatusEvent : public CgsGui::GuiEvent<514>
    {
        BrnReplays::ReplayIO::StatusInterface mInterface;  // DWARF BrnGuiEventTypeDefs.h:6263
    };
}

namespace BrnGame
{
    // Reach the GUI input buffer's inbound 32768-byte event queue (mInputQueue @ +4).
    // X360 sub_8284F238 returns this member after its lock handshake; modelled by the
    // proven +4 byte offset (see header note / FLAG above).
    inline CgsModule::VariableEventQueue<32768, 16>* GetGuiInputEventQueue(
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput)
    {
        return reinterpret_cast<CgsModule::VariableEventQueue<32768, 16>*>(
            reinterpret_cast<unsigned char*>(lpGuiInput) + 4);
    }
}
