#pragma once

#include "types.hpp"

namespace CgsSceneManager
{
    struct Event {};

    // DecFIGS CgsSceneManagerTypes.h:114.
    enum EErrorOrigin
    {
        E_ERROR_ORIGIN_SCENE_MANAGER = 0,
        E_ERROR_ORIGIN_FINE_TEST_MODULE,
        E_ERROR_ORIGIN_SCENE_GRAPH_MODULE,
        E_ERROR_ORIGIN_COUNT
    };

    // DecFIGS CgsSceneManagerTypes.h:125. ARTIST OutputBuffer::Construct
    // (0x828C7CA0) places 128 of these records between +0x30C4C and +0x3504C.
    struct ErrorEvent : public Event
    {
        char macMessage[128];
        s32 miUserData;
        EErrorOrigin meOrigin;
    };
}
