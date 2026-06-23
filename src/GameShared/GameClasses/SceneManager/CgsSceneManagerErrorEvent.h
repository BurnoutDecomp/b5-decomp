#pragma once

// Minimal owning home for the queue element CgsSceneManager::ErrorEvent -- the
// per-event payload stored in the EventQueue<CgsSceneManager::ErrorEvent, 128>
// scene-manager error queue (embedded in SceneManagerIO::OutputBuffer, whose
// Construct calls the queue's Construct). This header exists so the explicit-
// instantiation TU for that queue's Construct (EventQueue_ErrorEvent_128.cpp) can
// see a COMPLETE element type (the queue embeds ErrorEvent maEvents[128] inline).
//
// LAYOUT NOTE (HONEST PLACEHOLDER -- flagged): no DWARF/source layout for
// CgsSceneManager::ErrorEvent is available in this export, and the recovered
// EventQueue<ErrorEvent,128>::Construct @ 0x828C4B10 stores only the three base-queue
// header words (mpEvents = this+0xC, miMaxLength = 128, miLength = 0). The mpEvents
// store landing at offset 0xC proves the 12-byte BaseEventQueue header takes NO
// padding before maEvents, i.e. ErrorEvent's alignment is <= 4 (NOT 16-aligned).
// Beyond that the element's fields/size are not observable here, so the element is
// modelled as a 4-aligned reserved span; its true field layout belongs to ErrorEvent's
// own (un-exported) home. The queue-Construct parity (the three header stores) holds
// for any such element, so this placeholder does not affect the reconstructed behaviour.

#include "types.hpp"

namespace CgsSceneManager
{
    // Per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image), matching the empty SceneManagerIO::Event bases used by
    // the other SceneManager IO event payloads.
    struct Event {};

    // CgsSceneManager::ErrorEvent -- scene-manager error record queued into the
    // 128-deep error queue. Reserved-span placeholder element (4-aligned; see header
    // note). FLAGGED: element field layout/size is a documented placeholder.
    struct ErrorEvent : public Event
    {
        u32 muErrorCode;       // placeholder leading field (4-aligned -> header has no pad)
        u32 maReserved[3];     // placeholder reserved payload
    };
}
