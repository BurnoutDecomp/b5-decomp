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
// LAYOUT recovered from the X360 asm at the publish site
// (BrnNetwork::LoginManagerBase::AnswerAgreeTOS @ 0x82566478 /
//  ::UpdateDownloadingTOS @ 0x82566320), which builds the event on the stack before
// AddOutputGuiEvent enqueues it:
//     stw  r11(=1),  +0x00       -> miMarker      (a 1-word "valid/add" marker)
//     stw  mpText,   +0x04       -> mpText        (CgsUtf8* to the resolved string)
//     strncpy(+0x08, "TOS_TEXT", 128) -> macId    (the 128-byte string id, capped)
// i.e.:
//     +0x00  miMarker   (s32)  -- stored 1 at the publish site
//     +0x04  mpText     (CgsUnicode::CgsUtf8*)
//     +0x08  macId[128] (char) -- NUL-terminated string id (KI_MAX_ADD_ID_STRING_LENGTH)
//
// Modelled directly over the empty CgsModule::Event base so miMarker lands at offset 0
// exactly as the X360 build observes (the header-bearing CgsGui::GuiEvent<N> base is a
// different, larger reconstruction and is intentionally not used here, matching the
// sibling GuiEventLocalisedTextPointerRemoved). FLAGGED: the field *names* are not in the
// available DWARF -- only the publish-site stores ground the layout; miMarker's enum/role
// is modelled as the grounded raw word.
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
    };
}
