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

        // CgsEventQueue.h:194 (DWARF `EventQueue& operator=(const EventQueue&)`). The PS3 bodies
        // (e.g. EventQueue<BrnWorld::CrashIO::CrashingTrafficUpdateEvent,24>::operator= 0x684844)
        // clear miLength and append the source's events; ARTIST inlines it as the length store
        // then the out-of-line BaseEventQueue::Append (RootInputBuffer::SetPhysicalTrafficStates
        // 0x823C9124..0x823C9130).
        EventQueue& operator=(const EventQueue& lRhs)
        {
            this->Clear();
            this->Append(lRhs);
            return *this;
        }

    protected:
        T maEvents[N];
    };
}
