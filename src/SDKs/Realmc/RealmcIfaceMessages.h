#pragma once

#include "types.hpp"
#include "SDKs/Realmc/RealmcCore.h"   // RealmcCore (allocation/base conventions)

// RealmcIface -- the game-facing Realmc interface message records. HOME for the
// MessageBootupDone task (the "Realmc bootup finished" notification dispatched
// into the interface handler). Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// The runnable-task base MessageBootupDone derives, RealmcCore::IRunnableTask, is
// now homed in full in RealmcCore.h (included above): the recovered ctor @0x82C45170
// proves it is a refcounted RefCount subclass with its own task virtuals, so this
// file no longer carries the earlier minimal placeholder slice for it.
//
// FLAG (minimal slice): the handler interface below is modelled only as far as the
// two X360-emitted MessageBootupDone bodies need. Its dispatch slot is pinned by
// Apply @0x82B55220 (`lwz r11,0x3C(vtbl)` == slot 15), following the committed
// IRealmcMessageTarget reserved-slot convention (RealmcCore.h).
namespace RealmcCore
{
    // The Realmc sized free (the deleting destructors' `FreeMemSize(this, size)`
    // callee). ADDITIVE GROW: declaration-only (its body is the allocator TU's).
    void FreeMemSize(void* lpBlock, u32 luSize);
}

namespace RealmcIface
{
    // The handler Apply dispatches into: its vtable slot 15 (+0x3C) accepts the
    // bootup-done task. Reserved virtuals pin the slot, per the RealmcCore.h
    // IRealmcMessageTarget precedent.
    class IRealmcIfaceHandler
    {
    public:
        virtual ~IRealmcIfaceHandler() {}                                 // +0x00
        virtual void Reserved01() = 0;  virtual void Reserved02() = 0;    // +0x04 +0x08
        virtual void Reserved03() = 0;  virtual void Reserved04() = 0;    // +0x0C +0x10
        virtual void Reserved05() = 0;  virtual void Reserved06() = 0;    // +0x14 +0x18
        virtual void Reserved07() = 0;  virtual void Reserved08() = 0;    // +0x1C +0x20
        virtual void Reserved09() = 0;  virtual void Reserved10() = 0;    // +0x24 +0x28
        virtual void Reserved11() = 0;  virtual void Reserved12() = 0;    // +0x2C +0x30
        virtual void Reserved13() = 0;  virtual void Reserved14() = 0;    // +0x34 +0x38
        // +0x3C (slot 15) -- apply the bootup-done notification.
        virtual int OnBootupDone(class MessageBootupDone* lpMessage) = 0; // FLAG: slot name inferred
    };

    // The "bootup finished" task/message.
    class MessageBootupDone : public RealmcCore::IRunnableTask
    {
    public:
        // @0x82B55220 (class TU; body in RealmcIfaceMessages.cpp) -- dispatch this
        // notification into the handler (its vtable slot 15).
        int Apply(IRealmcIfaceHandler* lpHandler);

        // Backs the X360 `scalar deleting destructor` @0x82B55240 (base-dtor chain
        // + the sized Realmc free when the delete flag is set): the virtual dtor +
        // the class operator delete below reproduce it compiler-generated.
        virtual ~MessageBootupDone() {}
        static void operator delete(void* lpBlock, size_t luSize)
        {
            RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
        }
    };
}
