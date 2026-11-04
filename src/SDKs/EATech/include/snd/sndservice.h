#pragma once
#include "types.hpp"

namespace Snd9
{
    typedef void (*ServiceCallback)();
    typedef void (*VariableTimerCallback)(void* apUser);
    struct VariableTimerNode
    {
        VariableTimerNode* mpNext;
        VariableTimerNode* mpPrev;
        VariableTimerCallback mpCallback;
        void* mpUser;
    };
    extern VariableTimerNode* gpVariableTimerHead; // ARTIST 0x83277CB0
    extern ServiceCallback gapfn100HzService[6];   // ARTIST 0x83277870
    extern ServiceCallback gapfnService[10];      // ARTIST 0x83277888
    extern u8 gucServiceFlag65;                   // ARTIST 0x83277865; count is signed on use
    extern u8 gucServiceFlag66;                   // ARTIST 0x83277866; count is signed on use
}
