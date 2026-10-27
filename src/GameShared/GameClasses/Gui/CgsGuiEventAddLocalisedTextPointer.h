#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"

namespace CgsGui
{
    // Names/types: DecFIGS CgsGuiEventTypeDefs.h:166..180. ARTIST's
    // receiver 8285ED58..EDA4 and producers 825663E8..408/825664E8..500
    // establish the target order: operation, borrowed pointer, inline id.
    // The PS3 declaration's id-first ordering does not override those loads.
    // This is the flat event payload, with native host pointer width and no
    // data-bearing GuiEvent<N> header. The existing PC layout is preserved.
    struct GuiEventAddLocalisedTextPointer : public CgsModule::Event
    {
        static const s32 KI_MAX_ADD_ID_STRING_LENGTH = 128;

        enum EOperation : s32
        {
            E_OPERATION_ADD = 0,
            E_OPERATION_REMOVE = 1,
            E_OPERATION_COUNT = 2,
        };

        EOperation meOperation;
        const CgsUnicode::CgsUtf8* mpString;
        char macIdToAddToDatabase[KI_MAX_ADD_ID_STRING_LENGTH];

        s32 GetEventType() const { return 12; }
    };
}
