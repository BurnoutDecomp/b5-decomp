#pragma once
// ============================================================================
// b5-decomp/src/GameSource/Gui/Events/BrnGuiEventReplayStatus.h
//
// Canonical home for BrnGui::GuiReplayStatusEvent -- the GUI event the replay
// bridge synthesises once per frame from the replay module's status interface.
//
// The record is the status interface itself: the publisher copies the interface
// into a stack event with StatusInterface::operator= and hands that object
// straight to the GUI input queue as (id 524, size 1560), and sizeof(
// BrnReplays::ReplayIO::StatusInterface) is 0x618 == 1560. So the interface sits
// at +0x00 and there is no 12-byte GuiEvent header in front of it.
// ============================================================================

#include "types.hpp"
#include "GameSource/Replays/BrnReplayStatusInterface.h"   // BrnReplays::ReplayIO::StatusInterface

namespace BrnGui
{
    struct GuiReplayStatusEvent
    {
        BrnReplays::ReplayIO::StatusInterface mInterface;   // +0x00

        s32 GetEventType() const { return 524; }
    };
    static_assert(sizeof(GuiReplayStatusEvent) == 1560, "replay status record is 1560 bytes (id 524)");
}
