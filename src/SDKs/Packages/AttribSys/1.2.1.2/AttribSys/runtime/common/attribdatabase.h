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
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h"

namespace Attrib
{
// ARTIST 0x8280C65C and 0x8280C6A0 construct each list as an 8-byte sentinel
// followed by its 24-byte allocator. Only list entries carry a value.
struct AttribListLink
{
    AttribListLink* mpNext;
    AttribListLink* mpPrev;
};

struct AttribListNode : AttribListLink
{
    void* mpValue;
};

struct AttribListBase
{
    AttribListLink mNode;
    CgsAttribSys::AttribSysPackageAllocator mAllocator;
};

CGS_STATIC_ASSERT(sizeof(AttribListNode) == 12, "ARTIST list node size");
CGS_STATIC_ASSERT(sizeof(AttribListBase) == 32, "ARTIST list base size");

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
void            AttribListClearNodes(AttribListBase* lpList);         // DoClear    @ 0x828054B0
void            AttribListFreeNode(void* lpUnusedList, void* lpNode); // DoFreeNode @ 0x82803FC8

// The DoInsertValue / allocate-node-with-value counterpart the delete-queue push sites call
// (eastl::list<const Class*/Collection*, AttribSysPackageAllocator>): allocate a 12-byte node
// from lpList's AttribSys package allocator, store *lppValue into its mpValue slot, and return
// it (the caller links it into the ring). X360 sub_82809228 (Class list) / sub_828091A8
// (Collection list) -- one behaviour, two pointer-type instantiations; declared once here as the
// shared cross-TU seam (bodied in the list-container TUs), matching AttribListFreeNode above.
template <typename T>
AttribListNode* AttribListAllocateNode(AttribListBase* lpList, const T* const* lppValue);
AttribVectorBase* AttribVectorReserve(AttribVectorBase* lpVector,     // reserve    @ 0x8280C258
                                      unsigned int luCapacity);

// --- cross-TU vector helpers (bodied in their own AttribSys vector TUs; declared-only) ---
void*             AttribVectorAllocate(AttribVectorBase* lpVector, unsigned int luCount); // DoAllocate @ 0x8280C288
AttribVectorBase* AttribVectorFree(AttribVectorBase* lpVector, s32 liOldCapacityElems);   // DoFree     @ 0x82805348
} // namespace Attrib
