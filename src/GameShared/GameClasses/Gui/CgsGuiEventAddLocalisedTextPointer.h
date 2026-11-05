#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::Event (empty event base)
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"               // CgsUnicode::CgsUtf8

// ===========================================================================
// CgsGui::GuiEventAddLocalisedTextPointer
//   Home: GameShared/GameClasses/Gui/CgsGuiEventAddLocalisedTextPointer.{h,cpp}
//
// The GUI event published to register a pointer to a runtime-resolved localised
// string under a string id, so the front end can substitute it into a label. It
// is the partner of GuiEventLocalisedTextPointerRemoved (GuiEvent<13>); in the GUI
// event vocabulary this is the "add" side.
//
// LAYOUT and action flag recovered from the X360 producers and consumer:
//   UpdateDownloadingTOS @0x82566320 stores 0, then mpText and the 128-byte id.
//   AnswerAgreeTOS @0x82566478 stores 1, then mpText and the same id.
//   ViewModule::ProcessIncomingLanguageEvent @0x8285ED00 treats 0 as add and
//   nonzero as remove; removal also posts GuiEventLocalisedTextPointerRemoved (13).
// Thus the +0 word is a mode marker, not a validity bit. Its field name below is a
// local label; ARTIST/DWARF does not name it.
//     +0x00  miMarker (s32) -- 0 adds; nonzero removes
//     +0x04  mpText    (CgsUnicode::CgsUtf8*) -- caller-owned text pointer
//     +0x08  macId[128] (char) -- NUL-terminated string id
//
// Modelled directly over the empty CgsModule::Event base so the first word lands at
// offset 0. The larger header-bearing CgsGui::GuiEvent<N> reconstruction is not used
// here. BrnNetworkModule::AddOutputGuiEvent @0x825658C8 attests AddEvent(&event, 12, 136).
// ===========================================================================

namespace CgsGui
{
    struct GuiEventAddLocalisedTextPointer : public CgsModule::Event
    {
        // The add-id string-length cap (mirrors GuiEventLocalisedTextPointerRemoved's
        // KI_MAX_ADD_ID_STRING_LENGTH; the X360 publish site strncpy's the id with a 128 cap).
        static const s32 KI_MAX_ADD_ID_STRING_LENGTH = 128;

        s32                  miMarker;                          // +0x00 (publish site stores 1)
        CgsUnicode::CgsUtf8* mpText;                            // +0x04
        char                 macId[KI_MAX_ADD_ID_STRING_LENGTH]; // +0x08

        // The queued event-type id. Not GuiEvent<N>-derived, so the id is carried here;
        // X360-attested by BrnNetworkModule::AddOutputGuiEvent<GuiEventAddLocalisedTextPointer>
        // @0x825658C8 -> AddEvent(&event, 12, 136).
        s32 GetEventType() const { return 12; }
    };
}
