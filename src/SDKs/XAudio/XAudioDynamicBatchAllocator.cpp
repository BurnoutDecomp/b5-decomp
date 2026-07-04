#include "SDKs/XAudio/XAudioDynamicBatchAllocator.h"

#include "SDKs/XAudio/XAudioXMemMemoryManager.h"   // gXMemMemoryManager

#include <new>   // placement new (in-place MI construction over the XMem block)

// ===========================================================================
// XAUDIO::CDynamicBatchAllocator -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// See XAudioDynamicBatchAllocator.h for the (base-identical) layout and asm notes.
// No reference source and no DWARF exist for this TU.
// ===========================================================================

namespace XAUDIO
{

// HRESULT-style status words the X360 build returns from CreateObject.
static const s32 KS_OK          = 0;
static const s32 KS_OUTOFMEMORY = static_cast<s32>(0x8007000Eu); // E_OUTOFMEMORY

// The XMem allocate attribute word for this class (li r5,1), distinct from the
// 0x61820000 the CBatchAllocatedObject/CObjectRefCount teardown paths bake in.
static const u32 KU_XMEM_ALLOC_ATTRIBUTES = 1u;

// @ 0x82C31570 -- factory: XMem-allocate a 28-byte (0x1C) CDynamicBatchAllocator,
// run its two-vptr MI construction (mRefCount=1, empty bump heap), publish it
// through the out-parameter, and return an HRESULT-style status.
//
//   v2 = XMemAlloc(&unk_83222C40, 0x1C=28, 1)                 ; bl XMemAlloc
//   if (!v2) return 0x8007000E                                ; E_OUTOFMEMORY
//   ... staged vptr/refcount writes == the MI constructor with mRefCount = 1 ...
//   *a1 = v2                                                  ; stw r11, 0(r31)
//   return 0
s32 CDynamicBatchAllocator::CreateObject(CDynamicBatchAllocator** appObject)
{
    void* pRaw = gXMemMemoryManager.XMemAlloc(sizeof(CDynamicBatchAllocator),
                                              KU_XMEM_ALLOC_ATTRIBUTES); // 28, 1
    if (pRaw == nullptr)                        // cmplwi cr6, r11, 0 ; beq
        return KS_OUTOFMEMORY;                  // 0x8007000E

    CDynamicBatchAllocator* pObject = new (pRaw) CDynamicBatchAllocator();

    *appObject = pObject;                       // stw r11, 0(r31)
    return KS_OK;                               // 0
}

// The plain virtual destructor. Body is empty: CDynamicBatchAllocator adds no owned
// resources. The base ~CBatchAllocator() (heap release) chains automatically, and the
// flagged XMemFree(&unk_83222C40, this, 1) tail becomes `operator delete` at the call
// site. The X360 vector deleting destructor @0x82C314A8 + its adjustor{4}' thunk
// @0x82C313C8 are compiler-synthesised over this dtor and are not emitted as source.
CDynamicBatchAllocator::~CDynamicBatchAllocator()
{
}

} // namespace XAUDIO
