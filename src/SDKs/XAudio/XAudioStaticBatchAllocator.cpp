#include "SDKs/XAudio/XAudioStaticBatchAllocator.h"

#include "SDKs/XAudio/XAudioXMemMemoryManager.h"   // gXMemMemoryManager

#include <new>   // placement new (in-place MI construction over the XMem block)

// ===========================================================================
// XAUDIO::CStaticBatchAllocator -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// See XAudioStaticBatchAllocator.h for the (base-identical) layout, the inline
// co-allocated-heap strategy, and the teardown notes. No reference source and no
// DWARF exist for this TU.
// ===========================================================================

namespace XAUDIO
{

// HRESULT-style status words the X360 build returns from CreateObject.
static const s32 KS_OK          = 0;
static const s32 KS_OUTOFMEMORY = static_cast<s32>(0x8007000Eu); // E_OUTOFMEMORY

// @ 0x82C31620 -- factory. asm:
//   r3 = &gXMemMemoryManager ; r4 = a2 + 0x1C ; r5 = a1(attributes)
//   v6 = XMemAlloc(&mgr, heapSize + 0x1C, attributes)          ; bl XMemAlloc
//   if (!v6) return 0x8007000E                                 ; E_OUTOFMEMORY
//   *(v6+4)  = off_82108FF4          ; base CObjectRefCount vptr (MI ctor stage)
//   *(v6+8)  = 1                     ; mRefCount = 1
//   *(v6+0)  = off_821B56EC          ; primary vptr (this class)
//   *(v6+4)  = off_821B56D8          ; secondary vptr (this class) -- final settle
//   *(v6+0xC)= a1                    ; muXMemAttributes
//   *(v6+0x14)= 0                    ; muCursor
//   *(v6+0x10)= a2                   ; muCapacity = heapSize
//   *(v6+0x18)= v6 + 0x1C            ; mpHeapBase = object + sizeof (INLINE heap)
//   *a3 = v6 ; return 0
// The 0x1C literal is the X360 (32-bit) object size; reconstructed as sizeof so the
// inline heap follows the object on the host's wider layout (semantic parity).
s32 CStaticBatchAllocator::CreateObject(u32 uXMemAttributes, u32 uHeapSize,
                                        CStaticBatchAllocator** appObject)
{
    void* pRaw = gXMemMemoryManager.XMemAlloc(sizeof(CStaticBatchAllocator) + uHeapSize,
                                              uXMemAttributes);   // a2 + 0x1C, a1
    if (pRaw == nullptr)                        // cmplwi cr6, r11, 0 ; beq -> OOM
        return KS_OUTOFMEMORY;                  // 0x8007000E

    CStaticBatchAllocator* pObject =
        new (pRaw) CStaticBatchAllocator(uXMemAttributes, uHeapSize);

    *appObject = pObject;                       // stw r11, 0(r29)
    return KS_OK;                               // 0
}

// The inline-heap constructor (folded into CreateObject on the X360). The vptr
// stores are the compiler's MI construction; this body contributes the field seats.
CStaticBatchAllocator::CStaticBatchAllocator(u32 uXMemAttributes, u32 uHeapSize)
{
    mRefCount        = 1;                        // *(this+8) = 1
    muXMemAttributes = uXMemAttributes;         // *(this+0xC) = a1
    muCapacity       = uHeapSize;               // *(this+0x10) = a2
    muCursor         = 0;                        // *(this+0x14) = 0
    // *(this+0x18) = this + 0x1C -- the bump heap begins immediately after the object.
    mpHeapBase       = reinterpret_cast<u8*>(this) + sizeof(CStaticBatchAllocator);
}

// @ 0x82C31520 -- the `vector deleting destructor' body == ~CStaticBatchAllocator().
// asm:
//   stw off_821B56EC, 0(this)   ; primary vptr settle (this class, MI dtor stage)
//   stw off_821B56D8, 4(this)   ; secondary vptr settle (this class)
//   stw 0,           0x18(this) ; mpHeapBase = 0
//   bl  ~CBatchAllocator        ; chain base dtor
//   return this
// The two vptr re-seats are the compiler's derived-destruction staging. The load-
// bearing side effect is nulling mpHeapBase BEFORE the base dtor: the heap is
// co-allocated inline with the object, so ~CBatchAllocator (which XMemFree's a
// non-null mpHeapBase) must find it null and skip the free -- the whole block is
// released once, by AbsoluteRelease.
CStaticBatchAllocator::~CStaticBatchAllocator()
{
    mpHeapBase = nullptr; // stw r9(0), 0x18(this) -- base ~CBatchAllocator then no-ops
}

// @ 0x82C313D8 -- destruct then free the whole co-allocated block. asm:
//   v2 = *(this+8)                 ; on the +4 CObjectRefCount subobject this is the
//                                  ; full object's muXMemAttributes (+0xC); read up front
//   v1 = this - 4                  ; full object (allocation base)
//   result = (*vtable[0])(this, 1) ; virtual deleting dtor -- for this class frees
//                                  ; nothing (heap inline), just runs the dtor chain
//   if (v1) return XMemFree(&mgr, v1, v2)   ; free the single block (always taken)
//   return result
// Modelled with `this` as the full object (the X360 `this - 4` is the MI subobject
// adjustment the compiler applies to reach the allocation base); XMemFree(this, ...)
// therefore frees the same block.
void* CStaticBatchAllocator::AbsoluteRelease()
{
    u32 uAttributes = muXMemAttributes; // v2 = *(this+8) -- captured before teardown

    // (*vtable[0])(this, 1): virtual-destruct in place. ~CStaticBatchAllocator nulls
    // the inline heap and the base dtors chain; nothing is freed here (the deleting
    // flag is inert for this co-allocated class).
    this->~CStaticBatchAllocator();

    // XMemFree(&gXMemMemoryManager, this /*full block*/, uAttributes): release the
    // single allocation (object + inline heap) with the recorded attribute word.
    gXMemMemoryManager.XMemFree(this, uAttributes);

    return this; // X360 tail-returns XMemFree's r3; the freed object pointer here
}

} // namespace XAUDIO
