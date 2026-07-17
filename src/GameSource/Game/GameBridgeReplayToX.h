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
// The GUI event sink is the REAL CgsGui::GuiModule::AddGuiEvent<T> and the REAL
// CgsGuiModuleIO::InputBuffer::GetGuiEvents() @0x8284F238 now (the former empty
// InputBuffer placeholder in BrnGameModule.hpp is deleted, so the real header is
// includable); the bridge-local +4 offset hop is retired.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"            // CgsGui::GuiEvent<514>
#include "GameSource/Replays/BrnReplayStatusInterface.h"       // BrnReplays::ReplayIO::StatusInterface

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
