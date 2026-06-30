#pragma once

// =====================================================================
//  AptLinkerThingy.h  --  EA APT (ActionScript Player Technology) middleware
//
//  CORRECTED LAYOUT (supersedes the {mpLinkPrev,mpFile,muState,mpLinkNext}
//  guess). The list links are NOT in the thingy; the linker list is a separate
//  singly-linked list of 8-byte AptSingleListNode records, each holding a
//  ref-counted AptLinkerThingy* + a next pointer. The thingy is itself a
//  ref-counted heap object (count at +0x00).
//
//  HARD ASM EVIDENCE (BURNOUT_X360_ARTIST.XEX):
//   * push_front AptSingleLis @0x82AF4F88: Allocate(pool,8); node[0]=thingy
//     (AddRef'd); node[1]=oldhead.            => NODE is 8 bytes {thingy*,next}.
//   * node dtor AptSingleL @0x82AF5020: Release node[0]; at 0 -> thingy dtor;
//     Deallocate(node,8).                     => node[0] is a COUNTED thingy ref.
//   * pop_front sub_82AF50B8: next=*(head+4). => node next at +0x04.
//   * thingy dtor @0x82AE5128 (16-byte obj): r3=*(this+4); *(this+4)=0;
//     AptSharedPtr<AptFile>::Dispose(r3); Deallocate(this,0x10).
//                                             => thingy+0x04 == owned AptFilePtr.
//   * walks (CancelLoad/ConvertToZombie/Load/Update): *(*node+8) keyed on the
//     AptValue arg.                           => thingy+0x08 == AptValue* mpValue.
//   * Update: lbz/stb *(thingy+0xC) byte flag set to 1 once linked.
//                                             => thingy+0x0C == bool mbLinked.
//   * thingy ctor (Load @0x82B06908, r4=&AptFilePtr r5=AptValue*) fills +4/+8.
//
//  Members accessed BY NAME (semantic parity; console offsets documentation only).
// =====================================================================

#include "types.hpp"

#include "SDKs/EATech/include/Apt/AptSharedPtr.h"   // AptFilePtr (AptSharedPtr<AptFile>)
#include "SDKs/EATech/Apt/DogmaAllocator.h"          // DOGMA_PoolManager, gpAptSharedPtrPool

struct AptFile;
class  AptValue;

// 16-byte ref-counted per-linked-file record.
struct AptLinkerThingy
{
    int32_t    mnRefCount;   // [0x00] intrusive ref count (FIRST member)
    AptFilePtr mpFile;       // [0x04] owned AptFile shared reference (disposed by dtor)
    AptValue*  mpValue;      // [0x08] the linked AptValue (list key; null == inactive)
    bool       mbLinked;     // [0x0C] "already linked into the scene" flag (+3 pad)

    // X360 @0x82AE5128 -- MSVC scalar deleting destructor: dispose+zero mpFile,
    // then (flags&1) free the 16-byte block. Returns `this`.
    void* ScalarDeletingDestructor(char flags);
};

// 8-byte external pool node forming the linker's thingy list (head is
// AptLinker::mpThingyListHead). Owns a COUNTED reference to its thingy.
struct AptSingleListNode
{
    AptLinkerThingy*   mpThingy;   // [0x00] counted held thingy
    AptSingleListNode* mpNext;     // [0x04] next node
};

// MakeLinkerThingy -- the AptLinkerThingy::AptLinkerThingy ctor @0x82ADBF58 wrapped
// as the AptLinker::Load factory: pool-allocate a 16-byte node, then MOVE the held
// file reference from rFile into mpFile (incref into the thingy, dispose the source)
// and key it on pValue. Body in AptLinkerThingy.cpp (the X360 ctor is folded -- no
// own export symbol -- and was disassembled from the decrypted ARTIST.XEX).
AptLinkerThingy* MakeLinkerThingy(AptFilePtr& rFile, AptValue* pValue);