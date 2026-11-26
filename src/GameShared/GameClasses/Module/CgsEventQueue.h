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

        // CgsEventQueue.h:119 (DWARF: 99 `bool Append<SourceLength>(const EventQueue<T,SourceLength>&
        // lSourceQueue)` instantiations; LegacySrc body). A member template that forwards to the base
        // queue's out-of-line Append through GetBaseEventQueue(); ARTIST inlines it, e.g.
        // BrnGameModule::BridgeSoundToWorld 0x823CDC98 keeps the source block's base in r31 across the
        // world-side accessor call and forms the queue address only for the Append (0x823CDD10).
        template <s32 SourceLength>
        bool Append(const EventQueue<T, SourceLength>& lSourceQueue)
        {
            return BaseEventQueue<T>::Append(*lSourceQueue.GetBaseEventQueue());
        }

        // CgsEventQueue.h:160 / :177 (DWARF).
        const BaseEventQueue<T>* GetBaseEventQueue() const
        {
            return static_cast<const BaseEventQueue<T>*>(this);
        }

        BaseEventQueue<T>* GetBaseEventQueue()
        {
            return static_cast<BaseEventQueue<T>*>(this);
        }

        // CgsEventQueue.h:194 (DWARF `EventQueue& operator=(const EventQueue&)`). The PS3 bodies
        // (e.g. EventQueue<BrnWorld::CrashIO::CrashingTrafficUpdateEvent,24>::operator= 0x684844)
        // clear miLength and append the source's events; ARTIST inlines it as the length store
        // then the out-of-line BaseEventQueue::Append (RootInputBuffer::SetPhysicalTrafficStates
        // 0x823C9124..0x823C9130).
        EventQueue& operator=(const EventQueue& lRhs)
        {
            BaseEventQueue<T>::Clear();
            BaseEventQueue<T>::Append(*lRhs.GetBaseEventQueue());
            return *this;
        }

    protected:
        T maEvents[N];
    };
}
