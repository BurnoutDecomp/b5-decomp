#include "SDKs/XAudio/XAudioObjectRefCount.h"
#include "SDKs/XAudio/XAudioXMemMemoryManager.h"

// ===========================================================================
// XAUDIO::CObjectRefCount -- member bodies reconstructed from
// BURNOUT_X360_ARTIST.XEX. The PowerPC asm is authoritative for every store;
// see XAudioObjectRefCount.h for the reconstructed layout and vtable notes. No
// reference source and no DWARF exist for this TU.
// ===========================================================================

namespace XAUDIO
{

// The CObjectRefCount vtable image @0x82108FF4 (off_82108FF4 in the deleting
// destructor's `*this = off_82108FF4` re-seat). It is an external rodata symbol
// owned by the X360 image, not by this TU, so it is declared `extern` here and
// supplied by the platform/link layer; this TU only seats it.
extern const CObjectRefCount::VTable ObjectRefCountVTable; // @0x82108FF4

// The XAudio global XMem memory-manager instance @0x83222C40 (unk_83222C40 in the
// deleting destructor's XMemFree call) is the shared gXMemMemoryManager singleton,
// declared extern in XAudioXMemMemoryManager.h and defined once in
// XAudioBatchAllocatedObject.cpp -- this TU routes frees through it.

// @ 0x82C2F008 -- store-for-store:
//   mr r11,r3 ; lwz r10,4(r11) ; addi r3,r10,1 ; stw r3,4(r11) ; blr
// i.e. read mRefCount, return/store count+1.
int CObjectRefCount::AddRef()
{
    int liNew = mRefCount + 1; // lwz r10,4 ; addi r3,r10,1
    mRefCount = liNew;         // stw r3,4
    return liNew;
}

// @ 0x8295F428 -- store-for-store:
//   lwz r11,4(r3) ; addic. r11,r11,-1 ; stw r11,4(r3)
//   bne -> return r11 ; else lwz r11,0(r3) ; lwz r11,0xC(r11) ; bctrl(this)
//   li r3,0 ; return 0
// Decrement the count; on reaching 0 dispatch the destructor through vtable
// slot [3] (@+0x0C) and return 0; otherwise return the surviving count.
int CObjectRefCount::Release()
{
    int liNew = mRefCount - 1; // lwz 4(r3) ; addic. r11,r11,-1
    mRefCount = liNew;         // stw r11,4(r3)
    if (liNew != 0)            // bne
        return liNew;          // return surviving count

    mpVTable->mpDestruct(this); // lwz 0(r3) ; lwz 0xC(vtable) ; bctrl(this)
    return 0;                   // li r3,0
}

// @ 0x8295F480 -- store-for-store:
//   cmplwi r3,0 ; beqlr (null -> return null)
//   lwz r11,0(r3) ; li r4,1 ; lwz r11,0(r11) ; mtctr ; bctr
// Null-guard, then tail-call vtable[0](this, 1) -- the deleting destructor with
// the delete flag set. Returns its result (the object pointer).
void* CObjectRefCount::AbsoluteRelease(CObjectRefCount* apObject)
{
    if (apObject == nullptr) // cmplwi r3,0 ; beqlr
        return apObject;     // return null

    // lwz r11,0(r3) (vtable) ; lwz r11,0(r11) (slot[0]) ; bctr (tail-call)
    return apObject->mpVTable->mpScalarDeletingDtor(apObject, 1);
}

// @ 0x82961008 -- store-for-store:
//   lis/addi r11,off_82108FF4 ; stw r11,0(r31)   (re-seat vtable)
//   clrlwi r10,r4,31 ; cmplwi r10,0 ; beq -> skip free
//   lis r11,unk_83222C40 ; lis r5,0x6182 ; addi r3,r11,unk_83222C40 ; mr r4,r31
//   bl XAUDIO::CXMemMemoryManager::XMemFree   (this=&unk_83222C40, a1, 0x61820000)
//   mr r3,r31 ; return this
void* CObjectRefCount::ScalarDeletingDestructor(char aDeleteFlag)
{
    mpVTable = &ObjectRefCountVTable; // stw off_82108FF4,0(r31)

    if ((aDeleteFlag & 1) != 0)       // clrlwi r10,r4,31 ; cmplwi ; beq
    {
        // bl XMemFree(this=&unk_83222C40, pAddress=this, attributes=0x61820000).
        // The member XMemFree takes (pAddress, uAttributes); the manager `this`
        // is the global instance, the `lis r5,0x6182` constant is the attribute
        // word (0x61820000).
        gXMemMemoryManager.XMemFree(this, 0x61820000u);
    }

    return this; // mr r3,r31
}

} // namespace XAUDIO
