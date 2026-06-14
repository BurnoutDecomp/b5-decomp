#pragma once

#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"

namespace CgsModule
{
    // Fixed-capacity event queue: the N events live inline in maEvents, and the
    // base queue is pointed at that inline storage by Construct(). Recovered from
    // the X360 spine with member names/types from the DecFIGS DWARF
    // (CgsEventQueue.h). N is the compile-time capacity (KI_LENGTH).
    template <typename T, s32 N>
    class EventQueue : public BaseEventQueue<T>
    {
    public:
        static const s32 KI_LENGTH = N;

        void Construct()
        {
            BaseEventQueue<T>::Construct(maEvents, KI_LENGTH);
        }

    protected:
        T maEvents[N];
    };
}
