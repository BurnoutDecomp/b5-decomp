#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

namespace CgsGui
{
    // DecFIGS CgsGuiEventTypeDefs.h:41; ARTIST 8285EDD0..EE2C
    // compares the original word against 0/1, independently of LanguageManager's
    // different ParameterFormatType vocabulary.
    enum LocaliseFormat : s32
    {
        E_LOCALISEFORMAT_TIME_MINS_AND_SECS = 0,
        E_LOCALISEFORMAT_TIME_SECS = 1,
        E_LOCALISEFORMAT_COUNT = 2,
    };

    // DecFIGS CgsGuiEventTypeDefs.h:156..162, gated by ARTIST's actual
    // pointer/pointer/word/f32 loads at 8285EDD0..EE44. This is the flat runtime
    // payload, with no 12-byte GuiEvent<N> header. Host pointers retain their
    // native width; no serialized Apt record layout is introduced here.
    struct GuiEventUpdateLocalisedCpt : public CgsModule::Event
    {
        static const u32 KU_MAX_STRING_LENGTH = 128;
        const char* macName;
        const char* macVariable;
        LocaliseFormat meFormatType;
        f32 mfTimeInSecs;

        s32 GetEventType() const { return 10; }
    };
}
