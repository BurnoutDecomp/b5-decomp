#pragma once

// =====================================================================
//  AptLinker.h  --  EA APT middleware: the per-target FILE LINKER.
//
//  AptLinker is the 24-byte sub-object AptTarget owns at +0x20 (pool-allocated
//  inline in AptTarget::AptTarget @0x82B00160). It drives loadMovie/unloadMovie:
//  it owns (a) a singly-linked list of AptLinkerThingy records (one per file
//  being linked) and (b) a vector of pending AptFile shared pointers, then each
//  frame resolves loaded files into the scene graph (Update), converting old
//  nodes to "zombies".
//
//  LAYOUT (24 bytes), proven by the inline construction in AptTarget::AptTarget
//  (v7 = Allocate(pool,24); v7[0]=0; v7[1]=0; v7[2]=0; v7[3]=v7+4; v7[4]=0;
//  v7[5]=0) cross-checked vs Notify/Load/Update/the dtor:
//
//   +0x00  mpThingyListHead   AptSingleListNode*  (singly-linked thingy list head;
//                             walked as `for(i=*this; i; i=i->mpNext)`; the dtor
//                             drains it via pop_front sub_82AF50B8)
//   +0x04  mPendingFiles      a vector<AptFilePtr> (EA BasicString-as-vector):
//          +0x04  mnSize      logical element count
//          +0x08  mnCapacity  allocated slots
//          +0x0C  mpData      element storage (== &mInlineStorage[0] when small)
//          +0x10  mInlineStorage[2]   inline SBO buffer (2 x AptFilePtr; the ctor
//                             zeroes both and points mpData at it)
//
//  ASM EVIDENCE for the vector: Notify @0x82B00938 reads v2=this+4; data=v2[2]
//  (+0x0C); end=data+ *v2 (size at +0x04); scans [data,end) of AptFilePtr; the
//  push_back sub_82B00498 calls the demangled
//  `BasicString<StringAsVectorEncoding<AptSharedPtr<AptFile>>>::Insert`; the
//  swap helper sub_82AFF018 swaps {size,cap,data} at +0/+4/+8 with SBO fixups;
//  the dtor @0x82B00D50 calls the BasicString dtor on this+4 then drains the
//  list. The ctor's v7[3]=v7+4 (mpData -> +0x10) + two zero slots pins the
//  2-element inline capacity. Modelled (per the AptFileSavedInputStateVector
//  precedent) as a clean typed vector with NAMED members -- semantic parity, not
//  the console's literal SBO offsets (the pervasive x64 PC-port FLAG).
//
//  EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// =====================================================================

#include "types.hpp"

#include "SDKs/EATech/include/Apt/AptSharedPtr.h"        // AptFilePtr
#include "SDKs/EATech/include/Apt/AptLinkerThingy.h"      // AptLinkerThingy, AptSingleListNode
#include "SDKs/EATech/include/Apt/AptString/EAString.h"   // EAStringC (Load's name arg)

class  AptValue;
struct AptCIH;
struct AptFile;

struct AptLinker
{
    // --------------------------------------------------------------
    // mPendingFiles -- the EA BasicString<StringAsVectorEncoding<AptFilePtr>>
    // reconstructed as a typed vector of owned AptFilePtr. Inline SBO buffer of
    // 2 elements (the ctor's initial capacity). Members by NAME.
    // --------------------------------------------------------------
    struct PendingFileVector
    {
        int32_t    mnSize;              // [+0x04] logical count
        int32_t    mnCapacity;          // [+0x08] allocated slots
        AptFilePtr* mpData;             // [+0x0C] storage (-> mInlineStorage when small)
        AptFilePtr mInlineStorage[2];   // [+0x10] inline SBO buffer (ctor zeroes both)

        AptFilePtr*       begin()       { return mpData; }
        AptFilePtr*       end()         { return mpData + mnSize; }
        const AptFilePtr* begin() const { return mpData; }
        const AptFilePtr* end()   const { return mpData + mnSize; }
        int32_t           size()  const { return mnSize; }

        // push_back sub_82B00498 -> BasicString<...>::Insert at end (grow if full).
        void PushBack(const AptFilePtr& rFile);
        // swap with another (the X360 swap-assign sub_82AFF018; SBO-aware).
        void Swap(PendingFileVector& rOther);
    };

    AptSingleListNode* mpThingyListHead;   // [+0x00] linker thingy list head
    PendingFileVector  mPendingFiles;      // [+0x04 .. +0x17]

    // ctor: inline in AptTarget::AptTarget @0x82B00160 (head=0; empty SBO vector).
    AptLinker();

    // X360 @0x82B00D50 -- MSVC `scalar deleting destructor`. Destroys mPendingFiles
    // (BasicString dtor on this+4), drains the thingy list (pop_front until empty),
    // then (flags&1) frees the 24-byte block. Returns `this`.
    void* ScalarDeletingDestructor(char flags);

    // @0x82B06660 -- resolve a loadMovie: getVariable(name) for the target CIH,
    // (re)load the AptFile through the loader, and -- when it is ready -- enqueue a
    // notify + an AptLinkerThingy linking that file to the value.
    void Load(int nLevel, EAStringC* pFileName);   // a2 == level/target id, a3 == name

    // @0x82B00938 -- record that an AptFile shared ptr should be (un)linked: if the
    // file is already in mPendingFiles drop the incoming ref, else push_back it.
    void Notify(AptFilePtr* pFile);

    // @0x82B0CC68 -- per-frame: tick the loader, walk the thingy list resolving any
    // file that finished loading (linking it into the scene or converting the old
    // node to a zombie), then clear mPendingFiles + run the queued actions.
    void Update();

    // @0x82B00A18 -- convert pValue's node to a "zombie": if it is mid-transition
    // (flags 0x60000000==0x20000000) find its thingy, allocate a replacement AptCIH,
    // splice it into the parent/display list, fix up references, and drop the thingy.
    // Returns the replacement AptCIH (or pValue when no transition is pending).
    AptCIH* ConvertToZombie(AptValue* pValue);

    // @0x82AFAE18 -- cancel a pending load for pValue: find the thingy keyed on
    // pValue and pop it from the list (releasing its file ref). No-op if not found.
    void CancelLoad(AptValue* pValue);
};