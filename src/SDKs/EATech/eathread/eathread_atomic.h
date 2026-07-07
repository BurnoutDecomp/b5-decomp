#ifndef EA_THREAD_EAATOMIC_H
#define EA_THREAD_EAATOMIC_H

#include "types.hpp"
#include <cstdint>

// ===========================================================================
// SDKs/EATech/eathread/eathread_atomic.h
//
// EA::Thread::AtomicInt<T> -- the EAThread lock-free integer wrapper used across
// BURNOUT_X360_ARTIST.XEX. Shape reconstructed from the DecFIGS DWARF
// (eathread_atomic.h: `AtomicPointer : public AtomicInt<std::int32_t>`) and the
// Feb-2007 vendor source (EAThread 1.09.00 eathread_atomic.h + the powerpc
// specialisations), which pin the single `volatile ValueType mValue` member, the
// method set (GetValue/GetValueRaw/SetValue/SetValueConditional/Increment/Decrement/
// Add + the operator sugar), and the AtomicPointer void* facade.
//
// `EA::Thread` is a vendor library boundary, so its identifiers (AtomicInt,
// GetValue, SetValueConditional, ...) are preserved verbatim per the naming
// convention.
//
// HOST-PRIMITIVE FLAG: on the X360 every accessor is a masked-interrupt
// lwarx/stwcx. (32-bit) / ldarx/stdcx. (64-bit) reservation loop -- an atomic RMW
// primitive with no by-name host equivalent. On the MSVC/x64 host the bodies below
// are the plain, single-threaded-equivalent operations on mValue (the same
// treatment the sibling data headers apply to the AtomicInt32 fields they model as
// s32). They are SEMANTICALLY faithful (same read/modify/write result) but not
// hardware-atomic; that is sufficient for the reconstruction/compile gate and for
// the single-threaded PC bring-up path.
//
// This header is intentionally self-contained (only types.hpp / <cstdint>): the
// `namespace Futex` facade in eathread/Futex.h and the DataStream result/command
// readers pull it directly for `AtomicInt<std::uint64_t>` and must not drag in the
// whole eathread.h umbrella.
// ===========================================================================

namespace EA
{
namespace Thread
{
    template <class T>
    class AtomicInt
    {
    public:
        typedef T ValueType;

        // Feb-2007 leaves mValue uninitialised in the default ctor (the object is
        // typically constructed with an explicit seed or SetValue'd before use).
        AtomicInt() {}
        AtomicInt(ValueType n) : mValue(n) {}

        // Raw read (no reservation on the host) and the "atomic" read; on the host
        // both are a plain load of mValue.
        ValueType GetValueRaw() const { return mValue; }
        ValueType GetValue()    const { return mValue; }

        // Store n, return the previous value.
        ValueType SetValue(ValueType n)
        {
            const ValueType nOriginalValue = mValue;
            mValue = n;
            return nOriginalValue;
        }

        // If mValue == condition, store n and return true; else leave it and return
        // false (the compare-and-swap used by the Futex/DataStream status words).
        bool SetValueConditional(ValueType n, ValueType condition)
        {
            if (mValue == condition)
            {
                mValue = n;
                return true;
            }
            return false;
        }

        // Post-modify accessors return the NEW value (matching the X360 lwarx/stwcx.
        // loops, which leave the freshly-written value in the result register).
        ValueType Increment()      { mValue = static_cast<ValueType>(mValue + 1); return mValue; }
        ValueType Decrement()      { mValue = static_cast<ValueType>(mValue - 1); return mValue; }
        ValueType Add(ValueType n) { mValue = static_cast<ValueType>(mValue + n); return mValue; }

        inline operator const ValueType() const { return GetValue(); }
        inline ValueType operator =(ValueType n) { return SetValue(n); }
        inline ValueType operator+=(ValueType n) { return Add(n); }
        inline ValueType operator-=(ValueType n) { return Add(static_cast<ValueType>(ValueType() - n)); }
        inline ValueType operator++()    { return Increment(); }
        inline ValueType operator++(int) { return static_cast<ValueType>(Increment() - 1); }
        inline ValueType operator--()    { return Decrement(); }
        inline ValueType operator--(int) { return static_cast<ValueType>(Decrement() + 1); }

    protected:
        volatile ValueType mValue;
    };

    // Canonical EAThread atomic typedefs (Feb-2007 eathread_atomic.h). AtomicUint64
    // is the type the eathread/Futex.h facade re-exports as Futex::AtomicUint64.
    typedef AtomicInt<int32_t>  AtomicInt32;
    typedef AtomicInt<uint32_t> AtomicUint32;
    typedef AtomicInt<int64_t>  AtomicInt64;
    typedef AtomicInt<uint64_t> AtomicUint64;

    typedef AtomicInt64  AtomicIWord;
    typedef AtomicUint64 AtomicUWord;
    typedef AtomicInt32  AtomicIntPtr;
    typedef AtomicUint32 AtomicUintPtr;

    // AtomicPointer : public AtomicInt<std::int32_t> (DecFIGS DWARF
    // eathread_atomic.h:332; Feb-2007 derives from AtomicIntPtr == AtomicInt32).
    // The X360 is a 32-bit-pointer target, so the pointer is stored in the 32-bit
    // base value verbatim. HOST FLAG: on the x64 gate a full host pointer does not
    // fit in the int32_t base (the pervasive Apt x64-port truncation rule -- see
    // AptInit.cpp); the casts below reproduce the Feb-2007 vendor conversions
    // faithfully and compile (pointer-truncation warnings only, never errors).
    class AtomicPointer : public AtomicInt<int32_t>
    {
    public:
        typedef void* PointerValueType;

        AtomicPointer(void* p = 0)
            : AtomicInt<int32_t>(static_cast<ValueType>(reinterpret_cast<uintptr_t>(p))) {}

        AtomicPointer& operator=(void* p)
            { AtomicInt<int32_t>::operator=(static_cast<ValueType>(reinterpret_cast<uintptr_t>(p))); return *this; }

        operator const void*() const
            { return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(AtomicInt<int32_t>::GetValue()))); }

        void* GetValue() const
            { return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(AtomicInt<int32_t>::GetValue()))); }

        void* GetValueRaw() const
            { return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(AtomicInt<int32_t>::GetValueRaw()))); }

        void* SetValue(void* p)
            { return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(AtomicInt<int32_t>::SetValue(static_cast<ValueType>(reinterpret_cast<uintptr_t>(p)))))); }

        bool SetValueConditional(void* p, void* pCondition)
            { return AtomicInt<int32_t>::SetValueConditional(static_cast<ValueType>(reinterpret_cast<uintptr_t>(p)),
                                                             static_cast<ValueType>(reinterpret_cast<uintptr_t>(pCondition))); }
    };
}
}

#endif // EA_THREAD_EAATOMIC_H
