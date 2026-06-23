// BrnAbstractPool.h
// Director object-pool primitives. This slice homes two pieces:
//   * BrnDirector::IAbstractPoolFreeObject -- the abstract "free this slot" callback
//     interface an AbstractPool implements (vptr-only base; DWARF BrnAbstractPool.h).
//   * BrnDirector::AbstractPoolVoidHandle -- the type-erased handle a pool hands back
//     from AllocateVoid<T>(): a (object, owning-pool, slot-index, slot-size) tuple.
//
// Layout authority: the X360 BURNOUT_X360_ARTIST.XEX. AbstractPoolVoidHandle::Prepare
// @0x821F76C0 stores its four arguments store-for-store into offsets +0/+4/+8/+0xC,
// which fixes the member order/offsets below (and matches the DECFIGS DWARF member list
// mpObject/mpFreeObjectInterface/miIndex/miSize at BrnAbstractPool.h:77-80).
//
// The AbstractPool<N,ALIGN,T> template itself (and BrnDirector::MomentController, which
// drives these pools) is NOT homed here -- see BrnAbstractPool.cpp / the keystone note.

#pragma once

#include "types.hpp"   // s32, bool

namespace BrnDirector
{

// Abstract callback an AbstractPool implements so a handle can release its slot back to
// the owning pool. vptr-only base (no data members); DWARF BrnAbstractPool.h. Modeled as
// an abstract interface; the pool template (out of scope here) provides FreeObject.
class IAbstractPoolFreeObject
{
public:
    virtual ~IAbstractPoolFreeObject() {}

    // The pool frees the object occupying slot liIndex. Pure virtual: every concrete
    // AbstractPool<N,ALIGN,T> overrides it. (Signature is the minimal shape the handle
    // needs; the concrete arity lives with the pool template.)
    virtual void FreeObject(s32 liIndex) = 0;
};

// Type-erased allocation handle. sizeof == 0x10 (four 4-byte words); members at the
// X360-attested offsets +0/+4/+8/+0xC. "Void" == the object pointer is stored as void*
// (the templated AllocateVoid<T> casts on the way out).
class AbstractPoolVoidHandle
{
public:
    // X360 @0x821F76C0. Initialise the handle from a fresh allocation: stash the object
    // pointer, the owning pool's free-object interface, and the slot index/size. Each
    // argument is CGS_ASSERT range/null-guarded (non-fatal: the X360 still stores and
    // returns true even on a failed guard). Returns true.
    bool Prepare(IAbstractPoolFreeObject* lpFreeObjectInterface, void* lpObject,
                 s32 liIndex, s32 liSize);

    // Remaining handle API (declared-only in this slice; bodies live with the pool work).
    bool        Release();
    void*       Get();
    const void* Get() const;
    s32         GetSize() const;
    bool        operator==(const AbstractPoolVoidHandle& lrOther) const;

protected:
    // ---- layout (X360 store offsets; DWARF BrnAbstractPool.h:77-80) ----
    void*                    mpObject;              // +0x00  the pooled object
    IAbstractPoolFreeObject* mpFreeObjectInterface; // +0x04  owning pool (free callback)
    s32                      miIndex;               // +0x08  slot index within the pool
    s32                      miSize;                // +0x0C  slot/object byte size
};

} // namespace BrnDirector
