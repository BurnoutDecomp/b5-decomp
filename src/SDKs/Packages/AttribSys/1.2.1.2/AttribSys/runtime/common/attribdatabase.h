#pragma once

// SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribdatabase.h
//
// Minimal control-struct + helper declarations for the reconstructed AttribSys database
// container helpers (the eastl::list / eastl::vector methods the X360 build emits under
// truncated symbols). Reconstructed from BURNOUT_X360_ARTIST.XEX + DecFIGS DWARF
// (EASTL/list.h, EASTL/vector.h, attribdatabase.cpp).
//
// These helpers back:
//   eastl::list<const Attrib::Class*, AttribSysPackageAllocator>::DoClear    @ 0x828054B0
//   eastl::list<const Attrib::Class*, AttribSysPackageAllocator>::DoFreeNode @ 0x82803FC8
//   eastl::vector<const Attrib::TypeDesc*, AttribSysPackageAllocator>::reserve @ 0x8280C258

#include "types.hpp"

namespace Attrib
{
// eastl::list<T*> node: next4 + prev4 + value4 (12 bytes). mpNext @ +0 is attested by the
// DoClear ring-walk advance (lwz r30,0(r30)); the +=12 free accounting fixes the size.
struct AttribListNode
{
    AttribListNode* mpNext;   // +0x00
    AttribListNode* mpPrev;   // +0x04
    void*           mpValue;  // +0x08
};

// eastl::ListBase sentinel {mpNext, mpPrev}: DoClear receives &mNode as `this`, walks
// mNode.mpNext back to &mNode. Modelled as the sentinel only (the DWARF ListBase also
// embeds an AttribSysPackageAllocator mAllocator, but the frees target the STATIC EASTL
// allocator, so the base tail beyond the 8-byte sentinel is untouched by these helpers).
struct AttribListBase
{
    AttribListNode mNode;   // +0x00 (&mNode == this)
};

// eastl::vector control block: mpBegin @ +0, mpEnd @ +4, mpCapacityEnd @ +8 (three 4-byte
// pointers), attested by the reserve lwz/stw at 0(r31)/4(r31)/8(r31).
struct AttribVectorBase
{
    void** mpBegin;        // +0x00
    void** mpEnd;          // +0x04
    void** mpCapacityEnd;  // +0x08
};

// One list<T*> node's worth of bytes accounted against miFreeTotal per free (attested by
// miFreeTotal += 12 in DoClear @ 0x82805558 and DoFreeNode @ 0x82804054).
const s32 KI_ATTRIB_LIST_NODE_SIZE = 12;

// --- container helpers bodied in attribdatabase.cpp -------------------------------------
AttribListBase* AttribListClearNodes(AttribListBase* lpList);         // DoClear    @ 0x828054B0
void            AttribListFreeNode(void* lpUnusedList, void* lpNode); // DoFreeNode @ 0x82803FC8
AttribVectorBase* AttribVectorReserve(AttribVectorBase* lpVector,     // reserve    @ 0x8280C258
                                      unsigned int luCapacity);

// --- cross-TU vector helpers (bodied in their own AttribSys vector TUs; declared-only) ---
void*             AttribVectorAllocate(AttribVectorBase* lpVector, unsigned int luCount); // DoAllocate @ 0x8280C288
AttribVectorBase* AttribVectorFree(AttribVectorBase* lpVector, s32 liOldCapacityElems);   // DoFree     @ 0x82805348
} // namespace Attrib
