#pragma once

#include "types.hpp"

#include <eathread/eathread_rwmutex.h>

namespace CgsSceneManager
{
class OverlapGenerationModule
{
public:
    OverlapGenerationModule();

private:
    u32                 muModuleVTable;
    EA::Thread::RWMutex mInputMutex;
    EA::Thread::RWMutex mOutputMutex;
    u8                  mPad0[272];
    u32                 muOverlapListVTable;
};
}
