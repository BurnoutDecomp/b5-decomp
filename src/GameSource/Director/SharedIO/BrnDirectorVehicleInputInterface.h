#pragma once

// Canonical home of BrnDirector::BrnDirectorVehicleInputInterface (DWARF home
// GameSource/Director/SharedIO/BrnDirectorVehicleInputInterface.h:46).
//
// GROWN from the prior 256-byte NOMINAL slice to the real DWARF layout: the type
// wraps exactly one EventQueue<BrnDirector::NewVehicleEvent,50> (member :67; the
// queue typedef comes from BrnDirectorQueues.h:33). Both the element type
// (BrnDirectorEvents.h) and the queue instantiation TU
// (EventQueue_NewVehicleEvent_50.cpp) are committed, so the by-value embedders
// (RaceCarEntityModuleIO::OutputBuffer_PostPhysics, BrnWorldIO::UpdateOutputBuffer)
// now see the real member instead of an opaque blob. Existing users only return
// &member or call the methods below, so the grow is layout-transparent to them.
//
// alignas is supplied by the queue's element buffer (NewVehicleEvent is alignas(16),
// see BrnDirectorEvents.h -- the X360 copies elements at a 16-byte stride).

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"    // CgsModule::EventQueue<T,N>
#include "GameSource/Director/SharedIO/BrnDirectorEvents.h" // BrnDirector::NewVehicleEvent (+ Attribute::Key)

namespace BrnDirector
{
    // DWARF: BrnDirectorVehicleInputInterface.h:46
    // (struct BrnDirector::BrnDirectorVehicleInputInterface). Wraps a
    // CgsModule::EventQueue<NewVehicleEvent,50> new-vehicle event queue.
    struct BrnDirectorVehicleInputInterface
    {
        // BrnDirectorQueues.h:33
        typedef CgsModule::EventQueue<NewVehicleEvent, 50> NewVehicleEventQueue;

        // DWARF :51. Replace-merge the source interface's pending new-vehicle events
        // into this one. The X360 build inlines this whole body into the world buffer's
        // SetDirectorVehicleInputInterface @ 0x827AD1A0: it resets the queue length
        // (`stw 0, +8` == BaseEventQueue::Clear) then calls the out-of-line
        // EventQueue<NewVehicleEvent,50>::Append on the source queue. The clear MUST
        // live here (not in the caller): the queue member is private and the DWARF
        // declares no Clear() on this interface, so Append is the only public mutator
        // that can produce the observed store.
        void Append(const BrnDirectorVehicleInputInterface* lpSource)
        {
            mNewVehicleQueue.Clear();
            mNewVehicleQueue.Append(lpSource->mNewVehicleQueue);
        }

        // ---- methods bodied by this type's own TU ----
        void Construct();                                               // DWARF :54
        const NewVehicleEventQueue* GetNewVehicleEventQueue() const;    // DWARF :57
        s32  NewVehicle(Attribute::Key lAttribsKey, s32 liEntityIndex); // DWARF :63

    private:
        NewVehicleEventQueue mNewVehicleQueue;   // DWARF :67
    };
}
