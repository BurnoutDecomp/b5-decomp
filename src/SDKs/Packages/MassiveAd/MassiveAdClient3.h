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
//   CMassiveList (4 dwords, 0x10 bytes)
//     +0x00  mpHead                   (a1[0]; first node, 0 when empty)
//     +0x04  mpTail                   (a1[1]; last node)
//     +0x08  mpCurrent                (a1[2]; iterator cursor)
//     +0x0C  mnCount                  (a1[3]; live node count)
// ===========================================================================
//
// CMassiveList bodies reconstructed from the X360 ARTIST.XEX:
//     MassiveAdClient3::CMassiveList::Append       @ 0x82BCEFE8
//     MassiveAdClient3::CMassiveList::GetCurrData   @ 0x82BCF098
//     MassiveAdClient3::CMassiveList::GoToNext      @ 0x82BCF070
//     MassiveAdClient3::CMassiveList::GoToStart     @ 0x82BCF060
//     MassiveAdClient3::CMassiveList::Remove        @ 0x82BCF0D8
//     MassiveAdClient3::CMassiveList::RemoveAll     @ 0x82BCF1A8
//     MassiveAdClient3::CMassiveList::~CMassiveList @ 0x82BCF1F0

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

// ---------------------------------------------------------------------------
// MassiveAd string-format helper (sub_82C10930).
//
// Every MassiveAd identifier-building path (GUID, MAC address, server-time
// string) routes through this single snprintf-style formatter: it writes at
// most `nCount` bytes (NUL-terminated) into `pcBuffer` from a printf format and
// varargs, and returns the formatted length. On the X360 it is a vendor wrapper
// around the platform vsnprintf; declared here, defined by the MassiveAd
// platform string layer (another TU). The leaf consumers below build small
// fixed-size strings, so a vararg signature is the faithful shape.
//   FLAG: the X360 passes %I64d (a 64-bit conversion) in a PPC register pair;
//   the vararg packing below is reproduced from the Hex-Rays decode -- the
//   buffer/count/format operands are confirmed against the call-site registers,
//   but the exact 64-bit lane order of the timestamp argument is unverifiable
//   from a blind compile and is flagged rather than asserted.
int MassiveFormatString(char* pcBuffer, unsigned int nCount, const char* pcFormat, ...);

// ---------------------------------------------------------------------------
// MassiveAd platform clock hooks (FLAGGED platform APIs).
//
// On the X360 these are the Win32-style GetTickCount() and GetSystemTime():
//   MassiveGetTickCount()        -> milliseconds since boot (the DWORD tick)
//   MassiveGetSystemTimeBlock(p) -> fills the 16-byte SYSTEMTIME-shaped block p
//                                   (the wall clock; only its trailing two 16-bit
//                                    words are consumed by CreateMassiveGuid)
// Declared here and supplied by the MassiveAd platform layer (another TU). FLAG:
// these wrap OS time APIs, so they are external/platform, not project-owned.
// ---------------------------------------------------------------------------
unsigned int MassiveGetTickCount();
void         MassiveGetSystemTimeBlock(unsigned char* pacSystemTime);

// The default name handed to a CMassiveBaseObject constructed without (or with an
// empty) name. The X360 reads it through the .data pointer off_82F91A94[0]; the
// string content is "Invalid BaseObject Name". Defined in MassiveAdClient3.cpp.
extern const char* gpcDefaultBaseObjectName;

// ---------------------------------------------------------------------------
// MassiveAd debug-log hook (FLAGGED vendor logging API).
//
// The CMassiveCriticalSection Enter/Exit paths emit a printf-style trace through
// a vendor logging entry point that Hex-Rays renders as the placeholder symbol
// `STUB(level, name, format, ...)`. The call-site registers fix the leading
// operands exactly: r3 = a log level/category (the X360 passes 7), r4 = the
// object's base name (CMassiveBaseObject::mpcName, read from +0x0C), r5 = the
// format string, then the variadic substitution arguments (the section name and
// the "by whom" caller name). It is the classic MassiveAd verbose-logging sink.
// Declared here and supplied by the MassiveAd logging layer (another TU). FLAG:
// this is an external/vendor variadic logging API, not project-owned code, so it
// keeps a vendor-shaped signature rather than the project mp/lf scheme.
//   MassiveLog(nLevel, pcName, pcFormat, ...) -> emit a formatted trace line
// ---------------------------------------------------------------------------
void MassiveLog(int nLevel, const char* pcName, const char* pcFormat, ...);

// ---------------------------------------------------------------------------
// MassiveAd critical-section platform hooks (FLAGGED platform APIs).
//
// On the X360 CMassiveCriticalSection wraps the Win32/NT critical-section kernel
// APIs directly (RtlInitializeCriticalSection / RtlEnterCriticalSection /
// RtlLeaveCriticalSection over a 28-byte RTL_CRITICAL_SECTION embedded in the
// object at +0x14). The on-disk RTL_CRITICAL_SECTION width is platform-specific
// (28 bytes on the 32-bit X360, wider on the 64-bit host), so the embedded
// section is modelled by NAME as an opaque host-sized block and the hooks are
// declared platform-side rather than asserting the X360 byte size on the host.
// FLAG: these wrap the OS critical-section primitive -- external/platform, not
// project-owned. Declared here, supplied by the MassiveAd platform layer.
//   MassiveCriticalSectionStorage -- opaque storage for one OS critical section
//   MassiveInitializeCriticalSection(p) -> RtlInitializeCriticalSection(p)
//   MassiveEnterCriticalSection(p)      -> RtlEnterCriticalSection(p)
//   MassiveLeaveCriticalSection(p)      -> RtlLeaveCriticalSection(p)
// ---------------------------------------------------------------------------
struct MassiveCriticalSectionStorage
{
    // Opaque OS critical-section storage. The X360 RTL_CRITICAL_SECTION is 28
    // bytes; a host-sized reservation (64 bytes covers the wider 64-bit Win32
    // CRITICAL_SECTION) keeps the wrapper self-contained without depending on
    // <windows.h> here. Touched only by name via the platform hooks below.
    unsigned char abOpaque[64];
};

void MassiveInitializeCriticalSection(MassiveCriticalSectionStorage* pSection);
void MassiveEnterCriticalSection(MassiveCriticalSectionStorage* pSection);
void MassiveLeaveCriticalSection(MassiveCriticalSectionStorage* pSection);

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

protected:
    // Additive accessor (FLAG: not its own X360 function -- derived MassiveAd
    // objects read the base name field directly, e.g. CMassiveCriticalSection's
    // Enter/Exit load mpcName from +0x0C for their trace lines; this exposes that
    // same field to subclasses by NAME without widening the layout).
    const char* GetName() const { return mpcName; }

private:
    int         mnLastError;     // +0x04
    int         mnReportedError; // +0x08
    char*       mpcName;         // +0x0C
    int         mbIsValid;       // +0x10
};

// ---------------------------------------------------------------------------
// CMassiveCriticalSection -- a CMassiveBaseObject that owns one OS critical
// section plus an optional descriptive name, for serialising MassiveAd client
// access. Reconstructed from the X360 ARTIST.XEX (no leak / DecFIGS):
//     MassiveAdClient3::CMassiveCriticalSection::CMassiveCriticalSection   @ 0x82BD2700
//     MassiveAdClient3::CMassiveCriticalSection::~CMassiveCriticalSection  @ 0x82BD2788
//     MassiveAdClient3::CMassiveCriticalSection::Enter                     @ 0x82BD27E8
//     MassiveAdClient3::CMassiveCriticalSection::Exit                      @ 0x82BD28C0
//     MassiveAdClient3::CMassiveCriticalSection::`vector deleting destructor' @ 0x82BD2918
//
// The X360 ctor chains the CMassiveBaseObject base (name "MassiveCriticalSection"),
// installs this class's own vftable (off_821856B8 -- the virtual dtor below
// models it), RtlInitializeCriticalSection's the embedded section, and -- when a
// non-empty section name is supplied -- copies it into a MassiveMalloc'd buffer.
//
// Layout (members BY NAME; absolute offsets are X360-relative and not asserted on
// the host because the embedded critical section's width is platform-specific):
//   +0x00  vftable pointer        (off_821856B8; modelled via the virtual dtor)
//   +0x04  base CMassiveBaseObject body (mnLastError/mnReportedError/mpcName/mbIsValid)
//   +0x14  mCriticalSection       (a1+5; RtlInitializeCriticalSection target)
//   +0x30  mpcSectionName         (a1[12]; MassiveMalloc'd section-name copy, or 0)
// ---------------------------------------------------------------------------
class CMassiveCriticalSection : public CMassiveBaseObject
{
public:
    // @ 0x82BD2700. Constructs the base ("MassiveCriticalSection"), initialises
    // the embedded OS critical section, and -- when pcSectionName is non-null and
    // non-empty -- copies it into a MassiveMalloc'd buffer (left null on a failed
    // or skipped allocation).
    CMassiveCriticalSection(const char* pcSectionName);

    // @ 0x82BD2788. Rewrites the vftable (compiler-emitted for a virtual dtor),
    // frees the section-name buffer through the MassiveAd heap hook and nulls it,
    // then chains the base dtor.
    virtual ~CMassiveCriticalSection();

    // @ 0x82BD27E8. Acquires the OS critical section (blocking) and logs a
    // "Try(blocking) succeeded on <section> by <who>" trace. pcWho is the caller
    // tag passed straight through to the log.
    void Enter(const char* pcWho);

    // @ 0x82BD28C0. Releases the OS critical section and logs an
    // "Exit succeeded on <section> by <who>" trace.
    void Exit(const char* pcWho);

    // @ 0x82BD2918. Vector deleting destructor (the X360 vftable entry): runs the
    // dtor, then frees the object through CMassiveBaseObject::operator delete when
    // the low bit of bDelete is set. Returns this.
    void* VectorDeletingDestructor(char bDelete);

private:
    MassiveCriticalSectionStorage mCriticalSection; // +0x14 (RTL_CRITICAL_SECTION)
    char*                         mpcSectionName;    // +0x30 (section-name copy, or 0)
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
    // CMassiveList owns the intrusive links and walks them by name on the X360
    // (it reads/writes node +0x00 / +0x04 / +0x08 directly), so it is the one
    // class permitted to touch these private link members.
    friend class CMassiveList;

    CMassiveListNode* mpNext;  // +0x00
    CMassiveListNode* mpPrev;  // +0x04
    void*             mpOwner; // +0x08
};

// ---------------------------------------------------------------------------
// CMassiveList -- intrusive doubly-linked list over CMassiveListNode.
//
// The X360 keeps head/tail, a single-cursor iterator (GoToStart / GoToNext /
// GetCurrData), and a live count. Append links a caller-owned node at the tail
// and advances the cursor onto it; Remove unlinks a node (optionally destroying
// it through the MassiveAd heap hook) and patches head/tail/cursor as needed.
// ---------------------------------------------------------------------------
class CMassiveList
{
public:
    // @ 0x82BCF1F0. Tail-calls RemoveAll: destroys and frees every remaining
    // node. The X360 dtor is non-virtual (a plain branch into RemoveAll).
    ~CMassiveList();

    // @ 0x82BCEFE8. Links pNode at the tail (cursor follows it) and bumps the
    // count. A null node is a no-op returning 0; otherwise returns 1.
    int Append(CMassiveListNode* pNode);

    // @ 0x82BCF0D8. Unlinks pNode and patches head/tail/cursor; when bDelete is
    // set it also runs the node dtor and frees it through the heap hook. A null
    // node returns 0; otherwise returns 1.
    int Remove(CMassiveListNode* pNode, int bDelete);

    // @ 0x82BCF1A8. Removes (and deletes) every node, leaving the list empty.
    void RemoveAll();

    // @ 0x82BCF060. Resets the cursor to the head node.
    CMassiveListNode* GoToStart();

    // @ 0x82BCF070. Advances the cursor to the next node and returns it (0 when
    // the cursor was already past the end).
    CMassiveListNode* GoToNext();

    // @ 0x82BCF098. Returns the payload (mpOwner) of the node under the cursor,
    // or 0 when the cursor is null.
    void* GetCurrData();

    // Additive accessor (FLAG: not its own X360 function). The MassiveAd record-
    // teardown loop (CMassiveRecord::RemoveInteractionRecords @ 0x82BDD568) reads
    // the cursor node directly (`while ( *(list + 8) )`, i.e. mpCurrent != 0) to
    // decide whether to keep walking. This exposes that cursor BY NAME so the
    // owning record does not reach into the private link member.
    CMassiveListNode* GetCurrent() const { return mpCurrent; }

private:
    CMassiveListNode* mpHead;    // +0x00
    CMassiveListNode* mpTail;    // +0x04
    CMassiveListNode* mpCurrent; // +0x08
    int               mnCount;   // +0x0C
};

// ---------------------------------------------------------------------------
// CMassiveSystem -- the MassiveAd client system root / process singleton.
//
// Reconstructed from the X360 ARTIST.XEX (no leak / DecFIGS):
//     MassiveAdClient3::CMassiveSystem::ClearMassiveGuid               @ 0x82BD2218
//     MassiveAdClient3::CMassiveSystem::CreateMassiveGuid              @ 0x82BD2628
//     MassiveAdClient3::CMassiveSystem::GetHardwareAddress             @ 0x82BD23D0
//     MassiveAdClient3::CMassiveSystem::GetServerTimeFormatted         @ 0x82BD2308
//     MassiveAdClient3::CMassiveSystem::GetSystemTime                  @ 0x82BD22E0
//     MassiveAdClient3::CMassiveSystem::Initialize                     @ 0x82BD25C0
//     MassiveAdClient3::CMassiveSystem::Instance                       @ 0x82BD2208
//     MassiveAdClient3::CMassiveSystem::SetGuid                        @ 0x82BD2268
//     MassiveAdClient3::CMassiveSystem::Shutdown                       @ 0x82BD26B8
//     MassiveAdClient3::CMassiveSystem::`scalar deleting destructor'   @ 0x82BD2560
//
// The X360 object is a single dword (MassiveMalloc(4); *p = 0): just the
// heap-owned GUID-string pointer. It is NOT polymorphic -- the scalar deleting
// destructor frees the GUID then conditionally frees the object via the heap
// hook (no vftable rewrite), so this class has no virtuals and stays 4 bytes
// (host pointer width 8B is the only floor -- FLAG: do not assert the absolute
// X360 4-byte size on the 64-bit host).
//
// Layout:
//   +0x00  mpcGuid   (the MassiveMalloc'd 37-byte GUID string; *this on X360)
//
// Lifetime is a classic lazy process singleton kept in one .data slot
// (dword_8327F380): Initialize() allocates + zeroes it on first call, Instance()
// returns it, Shutdown() tears it down and clears the slot.
// ---------------------------------------------------------------------------
class CMassiveSystem
{
public:
    // @ 0x82BD25C0. Lazily allocates the singleton (MassiveMalloc(4), GUID
    // pointer zeroed) into the shared .data slot and returns it; a prior
    // instance is returned untouched. Returns null on allocation failure.
    static CMassiveSystem* Initialize();

    // @ 0x82BD2208. Returns the current singleton pointer (the raw .data slot;
    // null before Initialize / after Shutdown).
    static CMassiveSystem* Instance();

    // @ 0x82BD26B8. Destroys the singleton (scalar deleting destructor) and
    // clears the .data slot. Always returns 1.
    static int Shutdown();

    // @ 0x82BD22E0. Returns GetTickCount() (a millisecond tick, zero-extended to
    // the X360 DWORD return).
    static unsigned int GetSystemTime();

    // @ 0x82BD2308. Formats a server timestamp (Unix milliseconds in nMilliseconds)
    // as "YYYY-MM-DD HH:MM:SS,mmm" via localtime64 into pcBuffer (capacity nCount);
    // writes "Time Unavailable" when localtime64 fails. The X360 carries a dead
    // leading argument (a1, unused by the asm) ahead of the timestamp; it is kept
    // here as nUnused to preserve the call ABI. The ms remainder is computed from
    // the same timestamp value, not a separate argument (the Hex-Rays a3 was
    // register noise). Returns the formatted length.
    static int GetServerTimeFormatted(int nUnused, unsigned int nMilliseconds,
                                      char* pcBuffer, unsigned int nCount);

    // @ 0x82BD23D0. Queries the title XNADDR (XNetGetTitleXnAddr) and formats the
    // 6-byte ethernet MAC into a freshly MassiveMalloc'd 18-byte string at *ppcOut
    // ("AA:BB:CC:DD:EE:FF"). Returns the XNetGetTitleXnAddr status (<0 on error,
    // also returned when the allocation fails).
    static signed int GetHardwareAddress(int nUnused, char** ppcOut);

    // @ 0x82BD2628. Allocates a 37-byte buffer at *ppcOut and formats a
    // pseudo-GUID from the buffer address + tick counter + a SYSTEMTIME-derived
    // word via MassiveFormatString. Returns the formatted length (0 on allocation
    // failure, mirroring the X360 which returns the null malloc result).
    static int CreateMassiveGuid(char** ppcOut);

    // @ 0x82BD2268. Replaces this->mpcGuid with a fresh 37-byte copy of pcGuid:
    // clears any existing GUID, allocates + zero-fills, then strncpy's at most 37
    // bytes. Returns the (strncpy) destination pointer, or null on alloc failure.
    char* SetGuid(const char* pcGuid);

    // @ 0x82BD2218. Frees this->mpcGuid through the MassiveAd heap hook and nulls
    // it. Returns the old pointer value (0 when nothing was freed).
    void* ClearMassiveGuid();

    // @ 0x82BD2560. Scalar deleting destructor: clears the GUID, then frees the
    // object itself through the heap hook when bDelete & 1. Returns this.
    void* ScalarDeletingDestructor(char bDelete);

private:
    char* mpcGuid;  // +0x00
};

} // namespace MassiveAdClient3
