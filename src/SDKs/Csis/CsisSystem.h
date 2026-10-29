#pragma once
#include "SDKs/Csis/CsisDefinition.h"
namespace EA { namespace Allocator { class ICoreAllocator; } }
namespace Csis
{
class System
{
public:
    static int SetAllocator(EA::Allocator::ICoreAllocator*);
    static void* Allocate(size_t, const char*, u32);
    static void Free(void*);
    static int Init();
    static int IsInited();
    static int Lock();
    static int Unlock();
    static int Subscribe(SystemContent*);
    static int Unsubscribe(SystemContent*);
};
}
