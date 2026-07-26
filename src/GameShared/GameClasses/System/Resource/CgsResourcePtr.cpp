#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceBasePool.h"  // CgsResource::Entry (alias-ring head)
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT

// CgsResource::BaseResourcePtr -- the out-of-line members the X360 ARTIST build emitted for the
// base smart-pointer that did NOT land in the CgsBaseResourcePtr.cpp home (ctor/dtor/IsEqual/
// AddToNewList live there). This TU owns the alias-ring propagation + handle-reseat paths.
//
// LAYOUT (X360-authoritative -- see the FLAG in CgsResourcePtr.h):
//   +0x00 mpResourceMemory   +0x04/+0x08 mHandle (2 ptrs)
//   +0x0C mpNext             +0x10 mpPrev        +0x14 mpThis   +0x18 muThreadId
// A BaseResourcePtr and its aliases form a doubly-linked ring threaded through mpNext/mpPrev.

namespace CgsResource
{
    // Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828D9418
    //   (CgsResource::BaseResourcePtr::Propogate).  [ledger function for this TU]
    //
    // Broadcast THIS node's resource identity to every other node in its alias ring. Reproduced
    // store-for-store from the X360, accessing members BY NAME at their mapped offsets:
    //   guard: assert muThreadId (+0x18) != 0  -- "Resource pointer has not been initialized"
    //          (the asm streams the message via gpcMessageBuffer/StrStream; CGS_ASSERT forwards
    //           the plain string -- semantic parity, baked file/line dropped per convention)
    //   for ( i = this->mpNext; i != this; i = i->mpNext )
    //   {
    //       i->mpThis     = this->mpThis;        // +0x14  (asm: stw r10=[r28+0x14] -> [i+0x14])
    //       i->muThreadId = this->muThreadId;    // +0x18  (asm: stw r10=[r28+0x18] -> [i+0x18])
    //       owner = this->mpThis;                                       // +0x14 (re-read)
    //       i->mpResourceMemory        = owner->mpResourceMemory;       // +0x00
    //       i->mHandle.mpResourceMemory = owner->mHandle.mpResourceMemory; // +0x04
    //       i->mHandle.mpSourceEntry    = owner->mHandle.mpSourceEntry;    // +0x08
    //   }
    // The three identity dwords are copied FROM the ring's owner node (*this->mpThis); mpThis and
    // muThreadId are copied from THIS. On the owner this == this->mpThis, so the identity source
    // is always the owner regardless of which alias triggers propagation.
    void BaseResourcePtr::Propogate()
    {
        CGS_ASSERT(muThreadId != 0,
            "ERROR: Resource pointer has not been initialized\n");

        for (BaseResourcePtr* lpNode = mpNext; lpNode != this; lpNode = lpNode->mpNext)
        {
            lpNode->mpThis     = mpThis;        // +0x14
            lpNode->muThreadId = muThreadId;    // +0x18

            BaseResourcePtr* lpOwner = mpThis;  // +0x14: the authoritative node
            lpNode->mpResourceMemory         = lpOwner->mpResourceMemory;         // +0x00
            lpNode->mHandle.mpResourceMemory = lpOwner->mHandle.mpResourceMemory; // +0x04
            lpNode->mHandle.mpSourceEntry    = lpOwner->mHandle.mpSourceEntry;    // +0x08
        }
    }

    // Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828D93A0
    //   (CgsResource::BaseResourcePtr::CreateFromHandle).
    //
    // Reseat this ptr onto the resource named by lpSrcHandle, store-for-store from the X360:
    //   mpThis     = lpSrcHandle->mpResourceMemory;   // +0x14 <- a2[0]  (asm: stw r10=[r4] ->+0x14)
    //   muThreadId = lpSrcHandle->mpSourceEntry;      // +0x18 <- a2[1]  (asm: stw r10=[r4+4] ->+0x18)
    //   owner = mpThis;                               //        re-read +0x14
    //   mpResourceMemory         = owner[0];          // +0x00 <- *(mpThis+0)
    //   mHandle.mpResourceMemory = owner[1];          // +0x04 <- *(mpThis+4)
    //   mHandle.mpSourceEntry    = owner[2];          // +0x08 <- *(mpThis+8)
    //   -- unlink this from its current ring (the inline RemoveFromCurrentList) --
    //   if (mpNext) mpNext->mpPrev = mpPrev;          // [v2+0x10] = result[4]
    //   if (mpPrev) mpPrev->mpNext = mpNext;          // [v4+0x0C] = result[3]
    //   mpNext = this; mpPrev = this;                 // self-ring
    //   if (muThreadId) AddToNewList(this, (Entry*)muThreadId at +0x28);
    //
    // NOTE (X360-attested slot reuse): in this function the +0x14/+0x18 slots are loaded with the
    // handle's two pointers -- the source SmallResource (mpResourceMemory) and the source Entry
    // (mpSourceEntry) -- then mpThis (+0x14) is dereferenced to copy the SmallResource's first
    // three dwords (its identity) into this. The final relink adds this to the source Entry's
    // alias ring, whose head sits at Entry +0x28 (== &Entry::mResource; the X360 overlays the
    // SmallResource memory slot with the BaseResourcePtr ring head -- see the X360 RECONCILE note
    // in CgsResourceBasePool.h). muThreadId therefore holds the Entry pointer here (the X360
    // re-uses the field as the relink condition + source).
    void BaseResourcePtr::CreateFromHandle(const ResourceHandle* lpSrcHandle)
    {
        // +0x14 / +0x18 <- the handle's two pointers (a2[0], a2[1]). mpThis is loaded with the
        // source SmallResource pointer (which the X360 treats as a BaseResourcePtr*: its first
        // three dwords are the identity region mpResourceMemory + the two mHandle pointers, the
        // same overlay documented in CgsResourceBasePool.h). muThreadId carries the source Entry
        // pointer (the X360 re-uses the field as the relink condition + source).
        mpThis     = reinterpret_cast<BaseResourcePtr*>(lpSrcHandle->mpResourceMemory);
        muThreadId = reinterpret_cast<EAThread::ThreadId>(lpSrcHandle->mpSourceEntry);

        // Copy the owner node's identity region (offsets +0x00/+0x04/+0x08) into this, BY NAME.
        // PC RECONCILE (world-module mount 2026-07-26, null-handle bind): X360 call sites bind
        // NULL handles through here -- WorldGraphicsStreamer::Construct @0x827CA388 assigns the
        // all-zero default handle (dword_8300FAA0 = {0,0}) into every instance-list slot, and the
        // streamer's GetInstanceList comment attests the bound ptr "reads back as the empty/
        // default pointer". A literal read through the null owner is an AV on Win-x64 (it crashed
        // the first mounted boot inside this copy), so the attested end state -- the EMPTY
        // identity -- is expressed as the explicit null case.
        BaseResourcePtr* lpOwner = mpThis;  // owner == *(+0x14)
        if (lpOwner != 0)
        {
            mpResourceMemory         = lpOwner->mpResourceMemory;
            mHandle.mpResourceMemory = lpOwner->mHandle.mpResourceMemory;
            mHandle.mpSourceEntry    = lpOwner->mHandle.mpSourceEntry;
        }
        else
        {
            mpResourceMemory         = 0;
            mHandle.mpResourceMemory = 0;
            mHandle.mpSourceEntry    = 0;
        }

        // Inline RemoveFromCurrentList: splice this out of its current ring (null-safe).
        if (mpNext != 0) mpNext->mpPrev = mpPrev;   // [mpNext+0x10] = mpPrev
        if (mpPrev != 0) mpPrev->mpNext = mpNext;   // [mpPrev+0x0C] = mpNext
        mpNext = this;                              // self-ring
        mpPrev = this;

        // Relink into the source Entry's alias ring (head at Entry +0x28 == &mResource). Guard on
        // the Entry pointer (held in muThreadId per the slot-reuse note above).
        Entry* lpEntry = lpSrcHandle->mpSourceEntry;
        if (lpEntry != 0)
        {
            // Entry +0x28 is the alias-ring head; the SmallResource memory slot doubles as the
            // ring's BaseResourcePtr (X360-attested -- AddToNewList expects a BaseResourcePtr*).
            BaseResourcePtr* lpRingHead =
                reinterpret_cast<BaseResourcePtr*>(&lpEntry->mResource);
            AddToNewList(lpRingHead);
        }
    }
}
