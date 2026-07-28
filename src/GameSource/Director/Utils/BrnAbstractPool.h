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
#include <new>         // placement new (AbstractPool::AllocateVoid constructs in-slot)
#include "GameShared/GameClasses/Containers/CgsObjectPool.h" // CgsContainers::ObjectPool
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT

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

    // Hand the slot back to the owning pool. X360-attested from
    // BehaviourManager::ReleaseBehaviours @0x8221FDE8, which inlines it as
    //     freeIface = handle.mpFreeObjectInterface;  (*(*freeIface))(freeIface, handle.miIndex)
    // -- i.e. a virtual FreeObject on the owning pool with the slot index. (The console's
    // interface has FreeObject in vtable slot 0, so it has no virtual destructor there; the
    // host interface keeps one, which shifts the slot but not the semantics.) The console
    // does NOT clear the handle afterwards, so neither does this.
    bool Release()
    {
        if (mpFreeObjectInterface != 0)
        {
            mpFreeObjectInterface->FreeObject(miIndex);
        }
        return true;
    }

    // The pooled object / slot size. The X360 reads both as plain field loads at every call
    // site (`lwz r3, 0(handle)` / `lwz rN, 0xC(handle)`); named here so no consumer does.
    void*       Get()             { return mpObject; }
    const void* Get() const       { return mpObject; }
    s32         GetSize() const   { return miSize; }

    bool        operator==(const AbstractPoolVoidHandle& lrOther) const;

protected:
    // ---- layout (X360 store offsets; DWARF BrnAbstractPool.h:77-80) ----
    void*                    mpObject;              // +0x00  the pooled object
    IAbstractPoolFreeObject* mpFreeObjectInterface; // +0x04  owning pool (free callback)
    s32                      miIndex;               // +0x08  slot index within the pool
    s32                      miSize;                // +0x0C  slot/object byte size
};

// Fixed-capacity typed object pool, modelled on the Feb-2007 BrnAbstractPool.h template
// but updated to the SHIPPED X360 shape: it derives the IAbstractPoolFreeObject interface
// (so a handed-out AbstractPoolVoidHandle can free its slot back through FreeObject) and
// AllocateVoid<T> returns the four-word AbstractPoolVoidHandle (object, free-interface,
// index, size) rather than the old nested VoidHandle.
//
// Storage is one CgsContainers::ObjectPool of `num_buckets` buckets, each bucket a
// `unit_type[units_in_bucket]` blob big enough to hold the largest object the pool stores.
// DWARF (BrnAbstractPool.h): MomentController's pool is AbstractPool<70,20,Vector4>, i.e.
// ObjectPool<rw::math::vpu::Vector4[70],20,int32_t>.
//
// Layout note: x64 host build -- the vptr/pointer widths differ from the X360, so no
// absolute member offset is pinned (semantic parity, not byte-matching).
template <u32 units_in_bucket, u32 num_buckets, class unit_type = char>
class AbstractPool : public IAbstractPoolFreeObject
{
public:
    typedef s32 IndexType;

    // One pool slot: a bucket of `units_in_bucket` unit_type units (the X360 sizes a slot
    // to hold the largest moment/behaviour the pool serves).
    typedef unit_type Bucket[units_in_bucket];

    // Lifecycle. X360-attested from the two owners that inline the pool init:
    //   * BehaviourManager::Construct @0x82251778 -- ONE zero store per pool, landing on the
    //     underlying ObjectPool's occupancy word (manager +32056 / +64168 / +75048, each
    //     exactly `freeQueue + N*4 + 4` for its pool) and NOTHING else. That is
    //     ObjectPool::Construct()'s documented shape (occupancy only, no free-queue refill).
    //   * BehaviourManager::Prepare @0x8223DBE0 -- per pool: zero the same occupancy word,
    //     refill the free queue DESCENDING (N-1 .. 0 stored front-to-back), then store the
    //     count N. That is exactly ObjectPool::Clear().
    // Forwarded to the named ObjectPool operations so no owner reaches the queue by offset.
    void Construct() { mObjectPool.Construct(); }
    bool Prepare()   { mObjectPool.Clear(); return true; }
    bool Release()   { return true; }
    void Destruct()  { mObjectPool.Construct(); }   // the owner's Destruct clears occupancy only

    bool IsObjectAllocated(IndexType liIndex) { return mObjectPool.IsObjectAllocated(liIndex); }

    // Free-slot count of the underlying object pool. The X360 reads this word directly for its
    // pre-allocation "ran out of slots" guard (BehaviourManager::AllocateBehaviour); forwarded
    // to the ObjectPool's named counter so callers never reach it by raw offset.
    s32 GetNumFreeObjects() const { return mObjectPool.GetNumFreeObjects(); }

    void*       operator[](IndexType liIndex)       { return mObjectPool[liIndex]; }
    const void* operator[](IndexType liIndex) const { return mObjectPool[liIndex]; }

    // The slot-release callback the AbstractPoolVoidHandle calls (IAbstractPoolFreeObject).
    void FreeObject(s32 liIndex) override { mObjectPool.FreeObject(static_cast<IndexType>(liIndex)); }

    // Allocate a slot, construct a T in it, and hand back a type-erased handle. Faithful to
    // the SHIPPED X360 AllocateVoid<T> shape (returns AbstractPoolVoidHandle): assert the
    // object fits the bucket, pop a free slot (asserting space), construct the object in the
    // bucket, and Prepare the handle with (this-as-free-interface, object, index, sizeof(T)).
    template <class T>
    AbstractPoolVoidHandle AllocateVoid()
    {
        CGS_ASSERT(sizeof(Bucket) >= sizeof(T), "AbstractPool : object is too large");

        const IndexType liIndex = mObjectPool.AllocateObject();
        CGS_ASSERT(liIndex >= 0, "AbstractPool : out of space");

        void* lpSlot = static_cast<void*>(mObjectPool[liIndex]);
        T* lpObject = new (lpSlot) T();   // construct a fresh T in the slot (X360 default-init)

        AbstractPoolVoidHandle lHandle;
        lHandle.Prepare(this, static_cast<void*>(lpObject), liIndex,
                        static_cast<s32>(sizeof(T)));
        return lHandle;
    }

private:
    CgsContainers::ObjectPool<Bucket, static_cast<s32>(num_buckets), IndexType> mObjectPool;
};

} // namespace BrnDirector
