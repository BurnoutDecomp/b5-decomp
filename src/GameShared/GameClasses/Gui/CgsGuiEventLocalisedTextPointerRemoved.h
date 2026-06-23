#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::Event (empty event base)

// ===========================================================================
// CgsGui::GuiEventLocalisedTextPointerRemoved
//   Home: GameShared/GameClasses/Gui/CgsGuiEventLocalisedTextPointerRemoved.{h,cpp}
//
// A GUI event published when a localised-text pointer is removed: it carries the
// hash of the removed string's id so observers can tell whether the string they
// hold is the one that went away. It is GuiEvent<13> in the GUI event vocabulary
// (DWARF CgsGuiEventTypeDefs.h:185 `GuiEventLocalisedTextPointerRemoved :
// public GuiEvent<13>`).
//
// LAYOUT (X360 asm @ 0x82541B38 IsThisTheRemovedString / 0x828467E0
// SetStringRemoved): both methods read / write the hash word at offset 0, i.e.
// the GuiEvent<13> base contributes no data members here (the DWARF base
// `GuiEvent<13> : public Event` has only GetEventType()), so:
//     +0x00  muStringHash   (u32) -- CgsHash of the removed string's id
// Modelled directly over the empty CgsModule::Event base so muStringHash lands at
// offset 0 exactly as the X360 build observes (the header-bearing CgsGui::GuiEvent<N>
// reconstruction is a different, larger event base and is intentionally not used
// here).
// ===========================================================================

namespace CgsGui
{
    struct GuiEventLocalisedTextPointerRemoved : public CgsModule::Event
    {
        // CgsGuiEventTypeDefs.h:199 -- the add-id string length cap.
        static const s32 KI_MAX_ADD_ID_STRING_LENGTH = 128;

        // @ X360 0x828467E0 -- hash lpacString and store it as the removed-string id.
        // Returns the computed hash.
        u32 SetStringRemoved(const char* lpacString);

        // @ X360 0x82541B38 -- true when lpacString hashes to this event's stored id.
        bool IsThisTheRemovedString(const char* lpacString) const;

    private:
        u32 muStringHash;   // +0x00
    };
}
