#pragma once

#include "types.hpp"

namespace CgsMemory
{
    struct DataStreamCommandPoster;
    struct DataStreamResultReader;

    // DecFIGS RelocatorCommon.h:94; ARTIST 82AD2A08..28 copies these four words.
    struct RelocationTestParams
    {
        struct CGS_ALIGN(16) Command
        {
            char macString[32];
            s32 miNumber;
        };

        struct CGS_ALIGN(128) Result
        {
            char macInputSting[32];
            s32 miJobId;
            s32 miNumber;
            u32 muGetCommandUS;
            u32 muExecuteCommandUS;
            u32 muAddResultUS;
        };

        void* mpDummy;
        DataStreamCommandPoster* mpCommandPoster;
        DataStreamResultReader* mpResultReader;
        s32 miCounter;
    };
}
