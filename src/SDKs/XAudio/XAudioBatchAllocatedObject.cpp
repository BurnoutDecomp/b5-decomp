#include "SDKs/XAudio/XAudioBatchAllocatedObject.h"

#include "SDKs/XAudio/XAudioXMemMemoryManager.h" // gXMemMemoryManager (+ singleton definition)

// ===========================================================================
// XAUDIO::CBatchAllocatedObject -- bodies reconstructed from
// BURNOUT_X360_ARTIST.XEX. See XAudioBatchAllocatedObject.h for the layout and
// asm notes.
//
// This TU also provides the single definition of the XAudio-wide memory-manager
// singleton (gXMemMemoryManager, the X360 &unk_83222C40 instance) declared in
// XAudioXMemMemoryManager.h; the deleting destructors of the XAudio objects in
// this SDK route their frees through it.
// ===========================================================================

namespace XAUDIO
{

// The shared XAudio memory-manager instance (X360 &unk_83222C40). Defined once
// here; declared extern in XAudioXMemMemoryManager.h.
CXMemMemoryManager gXMemMemoryManager;

// @ 0x82C2DBB8 destructor work: release the held object (+8) via its vtable
// slot 1 and null the slot.
//   lwz r3, 8(r31)        ; v4 = mpHeld
//   stw <derived vtbl>,0  ; (compiler-emitted vptr reset)
//   if (v4) { (*(*v4+4))(v4); stw 0, 8(r31); }
CBatchAllocatedObject::~CBatchAllocatedObject()
{
    if (mpHeld)            // cmplwi cr6, r3, 0 ; beq
    {
        mpHeld->Vf1();     // lwz 0(r3); lwz 4(r11); bctrl  -- held vtable slot 1
        mpHeld = 0;        // li r11, 0 ; stw r11, 8(r31)
    }
}

// @ 0x82C2D828:
//   v2 = mpHeld;                          ; lwz r30, 8(r31)
//   if (v2) (**v2)();                     ; v2->vtable[0](v2)
//   result = (**this)(this, 1);           ; this->vtable[0](this, 1) -> delete this
//   if (v2) return (*(*v2+4))(v2);        ; v2->vtable[1](v2)
//   return result;
//
// The `this->vtable[0](this, 1)` is the virtual deleting destructor invoked with
// the delete flag -- in portable C++ that is exactly `delete this`. The held
// object captured in a local survives the delete, so its slot-1 call still runs
// afterwards (the lock-style acquire / delete-self / release sequence).
s32 CBatchAllocatedObject::AbsoluteRelease()
{
    IBatchHeld* pHeld = mpHeld; // r30 = *(this+8), captured before the delete

    if (pHeld)                  // cmplwi cr6, r30, 0 ; beq
        pHeld->Vf0();           // (**v2)()  -- held vtable slot 0

    delete this;                // (**a1)(a1, 1) -- virtual deleting destructor

    if (pHeld)                  // cmplwi cr6, r30, 0 ; beq
        return pHeld->Vf1();    // return (*(*v2+4))(v2) -- held vtable slot 1

    // No held object: the asm returns the deleting destructor's r3, which for
    // the scalar deleting destructor is the (now-freed) object pointer. The
    // callers discard it; surfaced here as 0.
    return 0;
}

// @ 0x82C2DBB8 (free arm): release `this` through the XAudio XMem manager
// singleton with the baked attribute word.
void CBatchAllocatedObject::operator delete(void* pMemory)
{
    gXMemMemoryManager.XMemFree(pMemory, KU_XMEM_FREE_ATTRIBUTES);
}

} // namespace XAUDIO
