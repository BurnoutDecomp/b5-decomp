#ifndef EA_THREAD_EAATOMIC_H
#define EA_THREAD_EAATOMIC_H

#include "types.hpp"
#include <xtl.h>

namespace EA
{
namespace Thread
{
    // ARTIST uses native masked-interrupt reservation operations for writes.
    // Aligned32/64-bit reads are plain lwz/ld, as the frame/data-stream sites show.
    template <class T>
    class AtomicInt
    {
    public:
        typedef T ValueType;
        CGS_STATIC_ASSERT((sizeof(ValueType) == 4 || sizeof(ValueType) == 8), "EA atomic word width");

        AtomicInt() {}
        AtomicInt(ValueType n) { SetValue(n); }
        ValueType GetValueRaw() const { return mValue; }
        ValueType GetValue() const { return mValue; }

        ValueType SetValue(ValueType n)
        {
            if (sizeof(ValueType) == 4)
                return static_cast<ValueType>(_InterlockedExchange(
                    reinterpret_cast<volatile LONG*>(&mValue), static_cast<LONG>(n)));
            return static_cast<ValueType>(_InterlockedExchange64(
                reinterpret_cast<volatile LONG64*>(&mValue), static_cast<LONG64>(n)));
        }

        bool SetValueConditional(ValueType n, ValueType condition)
        {
            if (sizeof(ValueType) == 4)
                return static_cast<u32>(_InterlockedCompareExchange(
                    reinterpret_cast<volatile LONG*>(&mValue), static_cast<LONG>(n),
                    static_cast<LONG>(condition))) == static_cast<u32>(condition);
            return static_cast<u64>(_InterlockedCompareExchange64(
                reinterpret_cast<volatile LONG64*>(&mValue), static_cast<LONG64>(n),
                static_cast<LONG64>(condition))) == static_cast<u64>(condition);
        }

        ValueType Increment()
        {
            if (sizeof(ValueType) == 4)
                return static_cast<ValueType>(_InterlockedIncrement(reinterpret_cast<volatile LONG*>(&mValue)));
            return static_cast<ValueType>(_InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&mValue)));
        }

        ValueType Decrement()
        {
            if (sizeof(ValueType) == 4)
                return static_cast<ValueType>(_InterlockedDecrement(reinterpret_cast<volatile LONG*>(&mValue)));
            return static_cast<ValueType>(_InterlockedDecrement64(reinterpret_cast<volatile LONG64*>(&mValue)));
        }

        ValueType Add(ValueType n)
        {
            if (sizeof(ValueType) == 4)
            {
                const u32 nOld = static_cast<u32>(_InterlockedExchangeAdd(
                    reinterpret_cast<volatile LONG*>(&mValue), static_cast<LONG>(n)));
                return static_cast<ValueType>(nOld + static_cast<u32>(n));
            }
            const u64 nOld = static_cast<u64>(_InterlockedExchangeAdd64(
                reinterpret_cast<volatile LONG64*>(&mValue), static_cast<LONG64>(n)));
            return static_cast<ValueType>(nOld + static_cast<u64>(n));
        }

        operator const ValueType() const { return GetValue(); }
        ValueType operator=(ValueType n) { SetValue(n); return n; }
        ValueType operator+=(ValueType n) { return Add(n); }
        ValueType operator-=(ValueType n) { return Add(static_cast<ValueType>(u64(0) - static_cast<u64>(n))); }
        ValueType operator++() { return Increment(); }
        ValueType operator++(int) { return static_cast<ValueType>(static_cast<u64>(Increment()) - 1); }
        ValueType operator--() { return Decrement(); }
        ValueType operator--(int) { return static_cast<ValueType>(static_cast<u64>(Decrement()) + 1); }

    protected:
        volatile ValueType mValue;
    };

    typedef AtomicInt<int32_t> AtomicInt32;
    typedef AtomicInt<uint32_t> AtomicUint32;
    typedef AtomicInt<int64_t> AtomicInt64;
    typedef AtomicInt<uint64_t> AtomicUint64;
    typedef AtomicInt64 AtomicIWord;
    typedef AtomicUint64 AtomicUWord;
    typedef AtomicInt32 AtomicIntPtr;
    typedef AtomicUint32 AtomicUintPtr;

    class AtomicPointer : public AtomicInt<int32_t>
    {
    public:
        typedef void* PointerValueType;

        // The base zero-initialization and pointer store are both present in
        // ARTIST ThreadLayout::ThreadLayout828E15C8, including for a null pointer.
        AtomicPointer(void* p = 0) : AtomicInt<int32_t>(0) { SetValue(p); }
        AtomicPointer& operator=(void* p) { SetValue(p); return *this; }
        operator const void*() const { return GetValue(); }
        void* GetValue() const { return reinterpret_cast<void*>(static_cast<u32>(AtomicInt<int32_t>::GetValue())); }
        void* GetValueRaw() const { return reinterpret_cast<void*>(static_cast<u32>(AtomicInt<int32_t>::GetValueRaw())); }
        void* SetValue(void* p)
        {
            return reinterpret_cast<void*>(static_cast<u32>(AtomicInt<int32_t>::SetValue(
                static_cast<ValueType>(reinterpret_cast<uintptr_t>(p)))));
        }
        bool SetValueConditional(void* p, void* condition)
        {
            return AtomicInt<int32_t>::SetValueConditional(
                static_cast<ValueType>(reinterpret_cast<uintptr_t>(p)),
                static_cast<ValueType>(reinterpret_cast<uintptr_t>(condition)));
        }
    };

    CGS_STATIC_ASSERT((sizeof(AtomicInt32) == 4), "ARTIST atomic32");
    CGS_STATIC_ASSERT((sizeof(AtomicUint64) == 8), "ARTIST atomic64");
    CGS_STATIC_ASSERT((__alignof(AtomicUint64) == 8), "ARTIST atomic64 alignment");
    CGS_STATIC_ASSERT((sizeof(AtomicPointer) == 4), "ARTIST atomic pointer");
}
}

#endif
