#include "SDKs/XAudio/XAudioBatchAllocator.h"

#include "SDKs/XAudio/XAudioXMemMemoryManager.h" // gXMemMemoryManager

// ===========================================================================
// XAUDIO::CBatchAllocator -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// See XAudioBatchAllocator.h for the two-vptr MI layout, the X360 member-offset
// map, and the asm notes. No reference source and no DWARF exist for this TU.
//
// The class is a bump-pointer allocator over a single XMem-backed heap: a base
// pointer (+0x18), a capacity (+0x10), a cursor (+0x14) and the XMem attribute
// word (+0x0C). CreateHeap binds the backing allocation, Alloc hands out
// 4-byte-aligned slices, GrowHeap raises the capacity ceiling, GetFreeHeapSize
// reports the remaining room, and the destructor releases the heap.
//
// The `AddRef'/`Release' virtual entry points are `adjustor{4}' thunks onto the
// CObjectRefCount secondary subobject (the compiler synthesises the `this -= 4`
// adjustment), so they are inherited rather than written here.
// ===========================================================================

namespace XAUDIO
{

// Round a byte count up to the next 4-byte multiple (the X360 `addi r,r,3 ;
// clrrwi r,r,2` idiom used by Create- and Grow-time sizing and by Alloc).
static inline u32 AlignUp4(u32 uSize)
{
    return (uSize + 3u) & 0xFFFFFFFCu;
}

// HRESULT-style status words the X360 build returns from CreateHeap.
static const s32 KS_OK            = 0;
static const s32 KS_OUTOFMEMORY   = static_cast<s32>(0x8007000Eu); // E_OUTOFMEMORY

// @ 0x82C312E0:
//   stw  off_821B5694, 0(this)   ; primary vptr settle (CBatchAllocatedObject)
//   stw  off_821B5680, 4(this)   ; secondary vptr settle (intermediate)
//   v2 = mpHeapBase (lwz 0x18)
//   if (v2) { XMemFree(&unk_83222C40, mpHeapBase, muXMemAttributes); mpHeapBase = 0; }
//   stw  off_82108FF4, 4(this)   ; secondary vptr final settle (base subobject)
//   return this
//
// In portable C++ the two staged vptr re-seats are the derived-then-base
// destruction the compiler emits; this body contributes the heap release and
// the base dtors chain afterwards.
CBatchAllocator::~CBatchAllocator()
{
    if (mpHeapBase) // cmplwi cr6, r4, 0 ; beq
    {
        // bl XMemFree(this=&unk_83222C40, pAddress=mpHeapBase, attributes=muXMemAttributes)
        gXMemMemoryManager.XMemFree(mpHeapBase, muXMemAttributes);
        mpHeapBase = 0; // li r11, 0 ; stw r11, 0x18(this)
    }
}

// @ 0x82C311F8:
//   r5 = a2 ; r4 = muCapacity (+0x10)
//   if (!muCapacity) return 0
//   muXMemAttributes = a2                                  ; stw r5, 0xC
//   v4 = XMemAlloc(&unk_83222C40, muCapacity, a2)          ; bl XMemAlloc
//   mpHeapBase = v4                                        ; stw r3, 0x18
//   return v4 ? 0 : 0x8007000E
// (cmplwi r3,0 ; bne -> loc returns 0 (S_OK) when the alloc SUCCEEDED; the
// fall-through returns E_OUTOFMEMORY (0x8007000E) when XMemAlloc gave null.)
s32 CBatchAllocator::CreateHeap(u32 uXMemAttributes)
{
    if (!muCapacity) // lwz r4, 0x10 ; cmplwi ; bne -> proceed, else return 0
        return KS_OK;

    muXMemAttributes = uXMemAttributes;                  // stw r5, 0xC
    mpHeapBase = gXMemMemoryManager.XMemAlloc(muCapacity, // r4 = muCapacity
                                              uXMemAttributes); // r5 = a2

    if (mpHeapBase == 0)            // cmplwi cr6, r3, 0
        return KS_OUTOFMEMORY;      // 0x8007000E
    return KS_OK;                   // 0
}

// @ 0x82C31270:
//   v3 = (a2 + 3) & ~3                                    ; addi/clrrwi
//   if (v3 > (*(*this+0x10))(this)) return 0              ; virtual GetFreeHeapSize
//   v5 = muCursor (+0x14)
//   result = mpHeapBase (+0x18) + v5
//   muCursor = v5 + v3
//   return result
void* CBatchAllocator::Alloc(u32 uSize)
{
    u32 uAligned = AlignUp4(uSize); // r30 = (a2 + 3) & ~3

    // (*(*this + 0x10))(this) -- the free-size check dispatches virtually through
    // the primary vtable; GetFreeHeapSize is that slot.
    if (uAligned > GetFreeHeapSize()) // cmplw cr6, r30, r3 ; ble -> proceed
        return 0;                     // li r3, 0

    u32 uCursor = muCursor;                                          // lwz 0x14
    void* pResult = static_cast<u8*>(mpHeapBase) + uCursor;          // add r10+r11
    muCursor = uCursor + uAligned;                                   // stw 0x14
    return pResult;
}

// @ 0x82C311E0:
//   muCapacity += (a2 + 3) & ~3
//   return this
void CBatchAllocator::GrowHeap(u32 uSize)
{
    muCapacity += AlignUp4(uSize); // addi/clrrwi ; lwz 0x10 ; add ; stw 0x10
}

// @ 0x82C313B8:
//   lwz r11, 0x10(this) ; lwz r10, 0x14(this) ; subf r3, r10, r11 ; blr
//   return muCapacity - muCursor
u32 CBatchAllocator::GetFreeHeapSize()
{
    return muCapacity - muCursor;
}

} // namespace XAUDIO
