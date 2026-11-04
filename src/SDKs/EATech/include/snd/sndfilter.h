#pragma once

#include "SDKs/EATech/include/snd/sndmix.h"

namespace Snd9
{
namespace Mix
{
    typedef s32 (*FilterProcess)(FilterState*, s32, f32*, f32*, s32);
    typedef void (*FilterRestore)(FilterState*);
    typedef u32 (*FilterPosition)(FilterState*);

    struct FilterState
    {
        FilterProcess mpProcess;    // +0x00
        FilterRestore mpRestore;    // +0x04
        FilterState* mpNext;        // +0x08
        u8 mauUnknown0C[4];
        FilterState* mpPrevious;    // +0x10
        u8 mauUnknown14[4];
        u16 muPriority;             // +0x18
        u8 mucFlags;                // +0x1A
        u8 mucPad1B;
    };

    struct VoiceStartParams
    {
        u8* mpSource;               // +0x00
        u8* mpLoopSource;           // +0x04
        u32 muUnknown08;
        u32 muSampleCount;          // +0x0C
        u32 muLoopStart;            // +0x10
        u32 muLoopEnd;              // +0x14
        s32 miVoice;                // +0x18
        u32 muUnknown1C;
        s32 miDecodeMode;           // +0x20
        FilterPosition mpPosition;  // +0x24
    };
}
}
