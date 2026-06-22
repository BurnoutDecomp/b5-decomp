#pragma once

// ===========================================================================
// MassiveAd Client3 -- in-game advertising client SDK (vendor middleware).
//
// There is NO Feb-2007 leak source and NO DecFIGS dwarfdump for this subsystem;
// SHAPE and BODIES are both reconstructed from the X360 ARTIST.XEX pseudocode +
// disassembly:
//     MassiveAdClient3::CMassiveBaseObject::CMassiveBaseObject          @ 0x82BCEE90
//     MassiveAdClient3::CMassiveBaseObject::~CMassiveBaseObject         @ 0x82BCEDC0
//     MassiveAdClient3::CMassiveBaseObject::`scalar deleting destructor'@ 0x82BCEF88
//     MassiveAdClient3::CMassiveBaseObject::operator delete            @ 0x82BCF0B8
//     MassiveAdClient3::CMassiveBaseObject::IsValidString              @ 0x82BCEE18
//     MassiveAdClient3::CMassiveBaseObject::SetLastError               @ 0x82BCEE58
//     MassiveAdClient3::CMassiveListNode::CMassiveListNode             @ 0x82BD3808
//     MassiveAdClient3::CMassiveListNode::~CMassiveListNode            @ 0x82BD3820
//     MassiveAdClient3::CMassiveListNode::operator new                 @ 0x82BD3830
//
// This is vendor/SDK code reconstructed in a canonical vendor home (the on-disk
// path mirrors the other reconstructed middleware packages under SDKs/Packages/).
// Per the naming convention, the vendor SDK identifiers (the MassiveAdClient3
// namespace, the CMassive* class names, IsValidString / SetLastError, and the
// MassiveMalloc / MassiveFree heap hooks) are PRESERVED VERBATIM -- they are an
// external middleware API, not project-owned code, so they keep their original
// casing instead of the project mp/lf/KI_ scheme.
//
// CMassiveBaseObject is the refcount-less polymorphic base of every MassiveAd
// client object: it owns a heap-allocated copy of the object's name string and a
// two-slot "last error" record. CMassiveListNode is the intrusive doubly-linked
// list node (next / prev / owner-data) used by the MassiveAd CMassiveList
// container. Both classes live in the same MassiveAdClient3 subsystem and so
// share this one owning header.
//
// Layouts are taken from the X360 store offsets:
//   CMassiveBaseObject (vtable + 5 dwords, 0x18 bytes)
//     +0x00  vftable pointer          (off_82184854; modelled via virtual dtor)
//     +0x04  mnLastError              (a1[1]; SetLastError writes the code here)
//     +0x08  mnReportedError          (a1[2]; updated only when it differs)
//     +0x0C  mpcName                  (a1[3]; MassiveMalloc'd name copy)
//     +0x10  mbIsValid                (a1[4]; 1 on success, 0 on alloc failure)
//   CMassiveListNode (3 dwords, 0x0C bytes)
//     +0x00  mpNext                   (result[0])
//     +0x04  mpPrev                   (result[1])
//     +0x08  mpOwner                  (result[2]; the listed payload pointer)
// ===========================================================================

#include <cstddef>

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// MassiveAd heap hooks.
//
// The X360 binary reaches the underlying heap through two indirect calls
// (function pointers in .data): MassiveMalloc (the `MassiveMalloc' @-relative
// pointer at off_..., rendered by Hex-Rays as a `malloc' call through a pointer)
// and off_82F91C18 (the matching `free'). They are the classic install-time
// allocator hooks. Declared here and defined by the MassiveAd platform heap
// layer (another TU); a forward declaration is all these objects need to
// compile and link.
//   MassiveMalloc(size) -> allocate `size` bytes
//   MassiveFree(ptr)    -> free a block            (off_82F91C18)
// ---------------------------------------------------------------------------
void* MassiveMalloc(std::size_t nSize);
void  MassiveFree(void* pBlock);

// The default name handed to a CMassiveBaseObject constructed without (or with an
// empty) name. The X360 reads it through the .data pointer off_82F91A94[0]; the
// string content is "Invalid BaseObject Name". Defined in MassiveAdClient3.cpp.
extern const char* gpcDefaultBaseObjectName;

// ---------------------------------------------------------------------------
// CMassiveBaseObject -- polymorphic base for MassiveAd client objects.
// ---------------------------------------------------------------------------
class CMassiveBaseObject
{
public:
    // @ 0x82BCEE90. Copies the supplied name (or the default name when the name
    // is null/empty) into a MassiveMalloc'd buffer. On allocation failure it
    // records error -99 and marks the object invalid.
    CMassiveBaseObject(const char* pcName);

    // @ 0x82BCEDC0. Frees the name buffer; the X360 dtor is virtual (it rewrites
    // the vftable pointer back to off_82184854 first).
    virtual ~CMassiveBaseObject();

    // @ 0x82BCEE18. Static helper: a name is valid iff it is non-null and not the
    // empty string. Returns the X360 boolean (0 / 1).
    static int IsValidString(const char* pcStr);

    // @ 0x82BCEE58. Records the last-error code. Variadic in the X360 (it is a
    // printf-style setter), but this leaf path only stores the code: mnLastError
    // is always overwritten, while mnReportedError is updated only when it would
    // change. Returns the code that was set.
    int SetLastError(int nErrorCode, const char* pcFormat, ...);

    // @ 0x82BCF0B8. Routes object teardown through the MassiveAd heap hook.
    static void operator delete(void* pMemory);

private:
    int         mnLastError;     // +0x04
    int         mnReportedError; // +0x08
    char*       mpcName;         // +0x0C
    int         mbIsValid;       // +0x10
};

// ---------------------------------------------------------------------------
// CMassiveListNode -- intrusive doubly-linked list node for CMassiveList.
// ---------------------------------------------------------------------------
class CMassiveListNode
{
public:
    // @ 0x82BD3808. Stores the owner/payload pointer and nulls both links.
    CMassiveListNode(void* pOwner);

    // @ 0x82BD3820. Clears the owner pointer (links are unlinked by CMassiveList
    // before the node is destroyed).
    ~CMassiveListNode();

    // @ 0x82BD3830. Allocates a node through the MassiveAd heap hook.
    static void* operator new(std::size_t nSize);

private:
    CMassiveListNode* mpNext;  // +0x00
    CMassiveListNode* mpPrev;  // +0x04
    void*             mpOwner; // +0x08
};

} // namespace MassiveAdClient3
