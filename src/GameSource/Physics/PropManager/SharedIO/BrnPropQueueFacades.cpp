#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // BeginAssert / FireAssert / EndAssert

// ============================================================================
// BrnPhysics::Props — out-of-line template-instance FACADES (X360 ARTIST build)
//
// IDA demangles these six symbols under "BrnPhysics::Props" with TRUNCATED names
// (Pr/Upd/UpdatePro/Prop/PropGraph/operator<<). The bodies (asm-authoritative) show
// they are NOT methods of a `BrnPhysics::Props` class — `BrnPhysics::Props` is a
// namespace of prop-event payload types (see BrnPropEvents.h). They are the per-
// instantiation out-of-line emissions of generic container/smart-pointer accessors,
// whose GENERIC homes are already committed:
//   - CgsModule::BaseEventQueue<T>  (GameShared/.../Module/CgsBaseEventQueue.h)
//   - CgsResource::ResourcePtr<T>   (GameShared/.../System/Resource/CgsResourcePtr.h)
//
// They are bodied here as standalone facades that reproduce the X360 store order /
// strides / bit tests EXACTLY, operating on the raw object layout (mpEvents @+0,
// miMaxLength @+4, miLength @+8 for the queue; mpResourceMemory @+0 for the ptr).
// Pointer arithmetic uses byte strides taken directly from the asm.
//
// FLAG (mis-demangled symbols): the IDA names are truncations of longer mangled
// template symbols; the real demangled identities are annotated per-function below.
// FLAG (un-homed element types): the queue element strides (64 and 112 bytes) and the
// resource-pointer pointee types are NOT pinned to a committed payload struct in this
// batch, so the facades take/return raw byte pointers rather than typed T&. They are
// bit-identical to the X360 4-byte-pointer ABI; swap in the typed generic instantiation
// once the exact element/resource types for these call sites are homed.
// ============================================================================

namespace BrnPhysics
{
namespace Props
{
    // ---- minimal raw view of CgsModule::BaseEventQueue<T> (layout-faithful) -------
    // Matches CgsBaseEventQueue.h member order: T* mpEvents; s32 miMaxLength; s32 miLength.
    struct RawEventQueue
    {
        u8* mpEvents;     // +0x00
        s32 miMaxLength;  // +0x04
        s32 miLength;     // +0x08
    };

    // ---- minimal raw view of CgsResource::BaseResourcePtr (first word only) -------
    struct RawResourcePtr
    {
        void* mpResourceMemory; // +0x00
    };

    // 0x825BC878 — CgsModule::BaseEventQueue<T (stride 64)>::AddEvent()  [reserve-slot]
    // Reserves the next event slot and returns a pointer to it, bumping miLength.
    // asm: assert mpEvents!=NULL (CgsBaseEventQueue.h:360); assert miLength<miMaxLength
    // (:361); result = (miLength<<6) + mpEvents; miLength = miLength+1.
    // The <<6 stride proves sizeof(T) == 64 for this instantiation.
    void* AddEvent_Stride64(RawEventQueue* lpQueue)
    {
        CGS_ASSERT(lpQueue->mpEvents != nullptr, "mpEvents != NULL");
        CGS_ASSERT(lpQueue->miLength < lpQueue->miMaxLength,
                   "GetLength() < GetMaxLength()");
        const s32 liSlot = lpQueue->miLength;
        void* lpResult = lpQueue->mpEvents + (static_cast<u32>(liSlot) << 6);
        lpQueue->miLength = liSlot + 1;
        return lpResult;
    }

    // 0x822AD048 — CgsModule::BaseEventQueue<T (stride 112)>::GetEvent(int) const
    // asm: assert mpEvents!=NULL (CgsBaseEventQueue.h:272); assert liIndex<miLength
    // (:274); assert liIndex>=0 (:275); result = 112*liIndex + mpEvents.
    // The *112 stride proves sizeof(T) == 112 for this instantiation.
    const void* GetEvent_Stride112_Const(const RawEventQueue* lpQueue, s32 liIndex)
    {
        CGS_ASSERT(lpQueue->mpEvents != nullptr, "mpEvents != NULL");
        CGS_ASSERT(liIndex < lpQueue->miLength, "liIndex < GetLength()");
        CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
        return lpQueue->mpEvents + 112 * static_cast<u32>(liIndex);
    }

    // 0x825BC7D0 — CgsModule::BaseEventQueue<T (stride 112)>::GetEvent(int)  [non-const]
    // Identical to the const overload but baked at CgsBaseEventQueue.h:292/294/295.
    void* GetEvent_Stride112(RawEventQueue* lpQueue, s32 liIndex)
    {
        CGS_ASSERT(lpQueue->mpEvents != nullptr, "mpEvents != NULL");
        CGS_ASSERT(liIndex < lpQueue->miLength, "liIndex < GetLength()");
        CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
        return lpQueue->mpEvents + 112 * static_cast<u32>(liIndex);
    }

    // 0x822868E0 — CgsResource::ResourcePtr<T>::operator->()  (baked CgsResourcePtr.h:544)
    // asm: assert mpResourceMemory != NULL ("Can not instance resource pointer - it has
    // no main memory resource"); return mpResourceMemory. (The X360 assert path builds
    // the message via the StrStream front-end; modelled as the house CGS_ASSERT, which
    // is the semantic-parity form already used in the CgsResourcePtr.h generic.)
    void* ResourcePtrDeref_PropInstance(RawResourcePtr* lpPtr)
    {
        CGS_ASSERT(lpPtr->mpResourceMemory != nullptr,
                   "Can not instance resource pointer - it has no main memory resource\n");
        return lpPtr->mpResourceMemory;
    }

    // 0x822CA460 — CgsResource::ResourcePtr<T>::operator->()  (baked CgsResourcePtr.h:544)
    // Same accessor as above for a different resource type (the prop-graphics-list path,
    // called by PropEntityModule::CachePropGraphicsLists); the only asm difference is the
    // baked assert FILE string. Bodied identically.
    void* ResourcePtrDeref_PropGraphics(RawResourcePtr* lpPtr)
    {
        CGS_ASSERT(lpPtr->mpResourceMemory != nullptr,
                   "Can not instance resource pointer - it has no main memory resource\n");
        return lpPtr->mpResourceMemory;
    }

    // ---- CellId stream-out facade (0x822A3FD0) ------------------------------------
    // FLAG (un-homed): this is `StrStream& operator<<(StrStream&, CellId)`. The stream
    // object is reached through a vtable (`(*(vtbl+4))(stream, literal)` is
    // StrStream::operator<<(const char*)) and the coordinate insert is the un-homed
    // helper sub_821F0E50 (StrStream::operator<<(unsigned short)). Neither the StrStream
    // vtable layout nor sub_821F0E50 is homed in this batch, so they are modelled as
    // opaque function-pointer dispatch / an extern declaration that reproduces the exact
    // call sequence and argument order from the asm. CellId is a 32-bit pair: the HIGH
    // 16 bits are the X coordinate, the LOW 16 bits the Z coordinate; either == 0xFFFF
    // marks the id invalid.
    //
    // Opaque StrStream view: first word is the vtable pointer; slot +1 (byte +4) is
    // operator<<(const char*).
    struct RawStrStream
    {
        void* (**mpVTable)(); // [0] vtable; entry at index 1 (+4 bytes) = op<<(char*)
    };
    typedef void* (*StrStreamPutString)(RawStrStream*, const char*);

    // sub_821F0E50: StrStream::operator<<(unsigned short) — un-homed; declared extern so
    // the facade links against the real emission once that TU is homed. Returns the
    // chained StrStream* (asm reuses the return value for the next insert).
    extern "C" RawStrStream* BrnPhysics_Props_StrStreamPutU16(RawStrStream* lpStream,
                                                              u16 luValue);

    RawStrStream* StreamOutCellId(RawStrStream* lpStream, u32 luCellId)
    {
        // asm: v3 = HIWORD (X), v4 = LOWORD (Z); valid unless either half == 0xFFFF.
        const u16 luX = static_cast<u16>(luCellId >> 16);
        const u16 luZ = static_cast<u16>(luCellId);

        // `if ( HIWORD==0xFFFF || LOWORD==0xFFFF ) invalid` (asm short-circuit form).
        const bool lbValid = !(luX == 0xFFFFu || luZ == 0xFFFFu);

        StrStreamPutString lpfnPutString =
            reinterpret_cast<StrStreamPutString>(lpStream->mpVTable[1]);

        if (lbValid)
        {
            lpfnPutString(lpStream, "X: ");
            RawStrStream* lpChained = BrnPhysics_Props_StrStreamPutU16(lpStream, luX);
            StrStreamPutString lpfnPutString2 =
                reinterpret_cast<StrStreamPutString>(lpChained->mpVTable[1]);
            lpfnPutString2(lpChained, " Z: ");
            BrnPhysics_Props_StrStreamPutU16(lpChained, luZ);
        }
        else
        {
            lpfnPutString(lpStream, "Invalid Cell Id");
        }
        return lpStream;
    }
}
}
