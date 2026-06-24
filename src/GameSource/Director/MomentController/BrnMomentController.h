#pragma once

// Home for the BrnDirector::MomentController nested helper types that the
// "director-moment" group needs as complete types:
//   * BrnDirector::MomentController::MomentHandle  -- a 24-byte slot describing one
//     live director moment owned by the controller. Its accessor GetMoment is bodied
//     out-of-line in BrnMomentController.cpp.
//   * BrnDirector::MomentDescription               -- a 16-byte POD record describing a
//     candidate moment; held by value in the MomentSelector's Array<MomentDescription,10>.
//
// Layout authority: BURNOUT_X360_ARTIST.XEX.
//   MomentController::MomentHandle::GetMoment @0x821F5798 reads mbIsAllocated (a bool at
//     this+0x00, asserted "mbIsAllocated" against BrnMomentController.h:150) then returns
//     the moment pointer at this+0x04.
//   The 24-byte element stride is pinned by Array<MomentController::MomentHandle,10>::Append
//     @0x821FD990 (count word @+0xF0 == 10*0x18; per-element copy of six 4-byte words).
//   The 16-byte MomentDescription stride is pinned by Array<MomentDescription,10>::Append
//     @0x821FD858 (count word @+0xA0 == 10*0x10; per-element copy of four 4-byte words).

#include "types.hpp"

namespace BrnDirector
{
    class Moment;   // returned (by pointer) from MomentController::MomentHandle::GetMoment

    // A 16-byte candidate-moment record held by value in the selector's
    // Array<MomentDescription,10>. Array<...>::Append @0x821FD858 copies it as four 4-byte
    // words and reads no field, so only the size/alignment is attested here. FLAG: the four
    // 4-byte fields' names/types are NOT recovered (no field is read in any bodied TU); the
    // record is modelled as four opaque 4-byte words to pin sizeof==0x10 exactly.
    struct MomentDescription
    {
        u32 mauOpaque[4];   // +0x00..+0x0C : opaque payload (4 * 4 = 16 bytes). FLAG: fields unknown.
    };

    // No pointer members, so the X360 0x10 size holds on the x64 host build too.
    static_assert(sizeof(MomentDescription) == 0x10, "MomentDescription layout drift");

    // MomentController owns the live director moments. Only its nested MomentHandle is
    // homed in this group (the controller class itself is out of scope -- see
    // BrnAbstractPool.h's keystone note). Modelled as a namespace-scoping shell so the
    // nested type spells BrnDirector::MomentController::MomentHandle exactly.
    class MomentController
    {
    public:
        // One controller slot: an allocated flag plus the held Moment pointer, then a
        // further 16 bytes of controller bookkeeping. sizeof == 0x18 (24 bytes), pinned by
        // the Array<MomentHandle,10>::Append stride (@0x821FD990).
        class MomentHandle
        {
        public:
            // X360 @0x821F5798. Asserts mbIsAllocated (BrnMomentController.h:150) then
            // returns the held moment pointer (this+0x04). Bodied in BrnMomentController.cpp.
            Moment* GetMoment() const;

        private:
            bool    mbIsAllocated;     // +0x00 : asserted before GetMoment returns
            u8      mPad1[3];          // +0x01 : alignment pad before the pointer at +0x04
            Moment* mpMoment;          // +0x04 : the held moment (returned by GetMoment)
            // FLAG: the remaining 16 bytes are controller bookkeeping not read by any bodied
            // TU; modelled as opaque to pin sizeof==0x18 (matches the Append 24-byte stride).
            u32     mauOpaque[4];      // +0x08..+0x14 : opaque (4 * 4 = 16 bytes). FLAG: fields unknown.
        };
    };

} // namespace BrnDirector
