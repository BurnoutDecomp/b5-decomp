#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"

#include <cstdint>  // uintptr_t
#include <cstring>  // strlen, strncpy, memset
#include <ctime>    // tm, _localtime64, __time64_t

// ===========================================================================
// MassiveAdClient3 -- CMassiveBaseObject + CMassiveListNode.
//
// SHAPE and BODIES are both reconstructed from BURNOUT_X360_ARTIST.XEX (there is
// no leak source / DecFIGS for this vendor middleware). Stores are reproduced
// member-for-member against the X360 disassembly; see MassiveAdClient3.h for the
// per-offset layout map.
// ===========================================================================

namespace MassiveAdClient3
{

// off_82F91A94[0] -- the default-name string the X360 ctor copies when no valid
// name is supplied. Defined here so the one .data slot has a single home.
const char* gpcDefaultBaseObjectName = "Invalid BaseObject Name";

// ---------------------------------------------------------------------------
// CMassiveBaseObject::CMassiveBaseObject @ 0x82BCEE90
//
// X360 store order: vftable, mnLastError=0, mnReportedError=0, mbIsValid=1, then
// the name copy. The name argument is used iff IsValidString(pcName) holds;
// otherwise the default name is copied. On a failed MassiveMalloc the object
// records error -99 (via SetLastError) and clears mbIsValid.
// ---------------------------------------------------------------------------
CMassiveBaseObject::CMassiveBaseObject(const char* pcName)
{
    mnLastError     = 0;   // a1[1] = 0
    mnReportedError = 0;   // a1[2] = 0
    mpcName         = 0;   // (set below; the vtable slot a1[0] is the implicit vftable)
    mbIsValid       = 1;   // a1[4] = 1

    // v5 = pcName && strlen(pcName) != 0  (inlined IsValidString).
    bool lbHasName = false;
    if (pcName && std::strlen(pcName) != 0)
        lbHasName = true;

    if (lbHasName)
    {
        std::size_t luLength = std::strlen(pcName) + 1;        // v10
        char* lpcBuffer = static_cast<char*>(MassiveMalloc(luLength));
        mpcName = lpcBuffer;                                   // a1[3] = v7
        if (!lpcBuffer)
        {
            SetLastError(-99, reinterpret_cast<const char*>(0)); // &unk_820046A7
            mbIsValid = 0;                                      // a1[4] = 0
            return;
        }
        std::strncpy(lpcBuffer, pcName, luLength);
    }
    else
    {
        std::size_t luLength = std::strlen(gpcDefaultBaseObjectName) + 1; // v6
        char* lpcBuffer = static_cast<char*>(MassiveMalloc(luLength));
        mpcName = lpcBuffer;                                   // a1[3] = v7
        if (!lpcBuffer)
        {
            SetLastError(-99, reinterpret_cast<const char*>(0)); // &unk_820046A7
            mbIsValid = 0;                                      // a1[4] = 0
            return;
        }
        std::strncpy(lpcBuffer, gpcDefaultBaseObjectName, luLength);
    }
}

// ---------------------------------------------------------------------------
// CMassiveBaseObject::~CMassiveBaseObject @ 0x82BCEDC0
//
// Rewrites the vftable slot (handled by the compiler for a virtual dtor), then
// frees the name buffer and nulls it.
// ---------------------------------------------------------------------------
CMassiveBaseObject::~CMassiveBaseObject()
{
    if (mpcName)         // result = a1[3]; if (result)
    {
        MassiveFree(mpcName);
        mpcName = 0;     // a1[3] = 0
    }
}

// ---------------------------------------------------------------------------
// CMassiveBaseObject::IsValidString @ 0x82BCEE18
//
// A name is valid iff non-null and non-empty. Returns the X360 boolean.
// ---------------------------------------------------------------------------
int CMassiveBaseObject::IsValidString(const char* pcStr)
{
    if (!pcStr)
        return 0;
    if (std::strlen(pcStr) == 0)   // v2 = strlen; if (!v2) return 0
        return 0;
    return 1;
}

// ---------------------------------------------------------------------------
// CMassiveBaseObject::SetLastError @ 0x82BCEE58
//
// Stores the code into mnLastError unconditionally; mnReportedError is updated
// only when it differs from the new code. Returns the code. The X360 declares
// this variadic (printf-style), but this path consumes neither the format nor
// the varargs -- they are flagged below.
// ---------------------------------------------------------------------------
int CMassiveBaseObject::SetLastError(int nErrorCode, const char* /*pcFormat*/, ...)
{
    int lnReported = mnReportedError;   // v5 = *(a1 + 8)
    mnLastError = nErrorCode;           // *(a1 + 4) = a2
    if (lnReported != nErrorCode)       // if (v5 != a2)
        mnReportedError = nErrorCode;   //   *(a1 + 8) = a2
    return nErrorCode;
}

// ---------------------------------------------------------------------------
// CMassiveBaseObject::operator delete @ 0x82BCF0B8
//
// Routes teardown through the MassiveAd free hook (off_82F91C18). The X360 tail-
// calls free(p) when p is non-null; MassiveFree already null-checks, so this is a
// straight forward.
// ---------------------------------------------------------------------------
void CMassiveBaseObject::operator delete(void* pMemory)
{
    if (pMemory)
        MassiveFree(pMemory);
}

// ---------------------------------------------------------------------------
// CMassiveListNode::CMassiveListNode @ 0x82BD3808
//
// X360 store order: mpOwner first (+8 = a2), then mpNext (+0 = 0) and
// mpPrev (+4 = 0).
// ---------------------------------------------------------------------------
CMassiveListNode::CMassiveListNode(void* pOwner)
{
    mpOwner = pOwner;   // result[2] = a2
    mpNext  = 0;        // *result   = 0
    mpPrev  = 0;        // result[1] = 0
}

// ---------------------------------------------------------------------------
// CMassiveListNode::~CMassiveListNode @ 0x82BD3820
//
// Clears the owner pointer; the list links are unlinked by CMassiveList::Remove
// before the node is destroyed.
// ---------------------------------------------------------------------------
CMassiveListNode::~CMassiveListNode()
{
    mpOwner = 0;   // *(result + 8) = 0
}

// ---------------------------------------------------------------------------
// CMassiveListNode::operator new @ 0x82BD3830
//
// Thunk straight into the MassiveAd heap hook.
// ---------------------------------------------------------------------------
void* CMassiveListNode::operator new(std::size_t nSize)
{
    return MassiveMalloc(nSize);
}

// ---------------------------------------------------------------------------
// CMassiveList::Append @ 0x82BCEFE8
//
// X360: a null node returns 0. When the list is non-empty (head != 0) the new
// node is stitched after the current tail: tail->mpNext = node (**(a1+4)=a2)
// and node->mpPrev = old tail (*(a2+4)=*(a1+4)). When empty, the node becomes
// the head (*a1 = a2). The tail and the iterator cursor are then both pointed at
// the new node and the count is incremented. Returns 1.
// ---------------------------------------------------------------------------
int CMassiveList::Append(CMassiveListNode* pNode)
{
    if (!pNode)                       // if (!a2) return 0;
        return 0;

    if (mpHead)                       // if (*a1)
    {
        mpTail->mpNext = pNode;       // **(a1 + 4) = a2  (tail->mpNext = node)
        pNode->mpPrev  = mpTail;      // *(a2 + 4) = *(a1 + 4)  (node->mpPrev = tail)
    }
    else
    {
        mpHead = pNode;               // *a1 = a2
    }

    int lnCount = mnCount;            // v3 = *(a1 + 12)
    mpTail    = pNode;                // *(a1 + 4) = a2
    mpCurrent = pNode;                // *(a1 + 8) = a2
    mnCount   = lnCount + 1;          // *(a1 + 12) = v3 + 1
    return 1;
}

// ---------------------------------------------------------------------------
// CMassiveList::Remove @ 0x82BCF0D8
//
// Unlinks pNode and patches the three list pointers, then optionally destroys
// it. A null node returns 0. Head/tail are reseated when they referenced the
// removed node (*a1 == a2 / a1[1] == a2). The neighbours are relinked across the
// gap: prev->mpNext = node->mpNext (*v6 = *a2) and node->mpNext->mpPrev =
// node->mpPrev (*(*a2 + 4) = a2[1]). If the cursor sat on the node it advances to
// node->mpNext. When bDelete is set the node is run through ~CMassiveListNode and
// freed via CMassiveBaseObject::operator delete (the MassiveAd free hook). The
// count is decremented and 1 returned.
// ---------------------------------------------------------------------------
int CMassiveList::Remove(CMassiveListNode* pNode, int bDelete)
{
    if (!pNode)                            // if (!a2) return 0;
        return 0;

    if (mpHead == pNode)                   // if (*a1 == a2)
        mpHead = pNode->mpNext;            //   *a1 = *a2

    if (mpTail == pNode)                   // if (a1[1] == a2)
        mpTail = pNode->mpPrev;            //   a1[1] = a2[1]

    CMassiveListNode* lpPrev = pNode->mpPrev;  // v6 = a2[1]
    if (lpPrev)                            // if (v6)
        lpPrev->mpNext = pNode->mpNext;    //   *v6 = *a2

    if (pNode->mpNext)                     // if (*a2)
        pNode->mpNext->mpPrev = pNode->mpPrev; // *(*a2 + 4) = a2[1]

    if (pNode == mpCurrent)                // if (a2 == a1[2])
        mpCurrent = pNode->mpNext;         //   a1[2] = *a2

    if (bDelete)                           // if (a3)
    {
        pNode->~CMassiveListNode();        // CMassiveListNode::~CMassiveListNode(a2)
        CMassiveBaseObject::operator delete(pNode); // ...::operator delete(a2)
    }

    --mnCount;                             // --a1[3]
    return 1;
}

// ---------------------------------------------------------------------------
// CMassiveList::RemoveAll @ 0x82BCF1A8
//
// Repeatedly removes (with deletion) the head node until the list is empty.
// ---------------------------------------------------------------------------
void CMassiveList::RemoveAll()
{
    for (; mpHead; )               // for ( i = result; *i; ... )
        Remove(mpHead, 1);         // result = Remove(i, *i, 1)
}

// ---------------------------------------------------------------------------
// CMassiveList::~CMassiveList @ 0x82BCF1F0
//
// Tail-calls RemoveAll (a plain branch in the X360).
// ---------------------------------------------------------------------------
CMassiveList::~CMassiveList()
{
    RemoveAll();
}

// ---------------------------------------------------------------------------
// CMassiveList::GoToStart @ 0x82BCF060
//
// Resets the cursor to the head and returns it.
// ---------------------------------------------------------------------------
CMassiveListNode* CMassiveList::GoToStart()
{
    CMassiveListNode* lpHead = mpHead;  // result = *a1
    mpCurrent = lpHead;                 // a1[2] = result
    return lpHead;
}

// ---------------------------------------------------------------------------
// CMassiveList::GoToNext @ 0x82BCF070
//
// Advances the cursor to mpCurrent->mpNext and returns the new cursor. A null
// cursor short-circuits to 0.
// ---------------------------------------------------------------------------
CMassiveListNode* CMassiveList::GoToNext()
{
    CMassiveListNode* lpCurr = mpCurrent;  // v2 = *(a1 + 8)
    if (!lpCurr)                           // if (!v2) return 0;
        return 0;

    CMassiveListNode* lpNext = lpCurr->mpNext;  // result = *v2
    mpCurrent = lpNext;                         // *(a1 + 8) = *v2
    return lpNext;
}

// ---------------------------------------------------------------------------
// CMassiveList::GetCurrData @ 0x82BCF098
//
// Returns the payload pointer (mpOwner) of the node under the cursor, or 0.
// ---------------------------------------------------------------------------
void* CMassiveList::GetCurrData()
{
    CMassiveListNode* lpCurr = mpCurrent;  // v1 = *(a1 + 8)
    if (lpCurr)                            // if (v1)
        return lpCurr->mpOwner;            //   return *(v1 + 8)
    return 0;
}

// ===========================================================================
// CMassiveSystem -- the MassiveAd client system root / process singleton.
// ===========================================================================

// dword_8327F380 -- the single .data slot holding the lazily-created singleton.
// Defined here so the slot has exactly one home; Initialize/Instance/Shutdown
// all touch it by name.
static CMassiveSystem* gpMassiveSystemInstance = 0;

// ---------------------------------------------------------------------------
// Platform externs (FLAGGED).
//
// XNetGetTitleXnAddr is the Xbox 360 networking API that fills an XNADDR with
// the title's network identity. Only its 6-byte ethernet MAC (XNADDR::abEnet,
// at byte offset +0x0A of the 0x60-byte struct read on the X360) is consumed
// here. Declared with a minimal local shape sufficient to reproduce the field
// reads; the real definition lives in the Xbox networking layer (not in this
// project). FLAG: this is a platform API stub -- the byte offsets are taken
// straight from the disassembly (lbz from var_36..var_31 == struct +0x0A..+0x0F).
// ---------------------------------------------------------------------------
struct MassiveXnAddr  // mirrors the leading bytes of the X360 XNADDR
{
    unsigned char abLeading[0x0A]; // ina + inaOnline (8) + wPortOnline (2)
    unsigned char abEnet[6];       // +0x0A: ethernet MAC (the only field used)
    unsigned char abPad[0x60 - 0x10];
};

extern "C" signed int XNetGetTitleXnAddr(MassiveXnAddr* pxna);

// ---------------------------------------------------------------------------
// CMassiveSystem::ClearMassiveGuid @ 0x82BD2218
//
// result = mpcGuid; if (result) { MassiveFree(result); mpcGuid = 0; } return
// result. (The X360 reads the freed pointer's old value into r3 and returns it.)
// ---------------------------------------------------------------------------
void* CMassiveSystem::ClearMassiveGuid()
{
    void* lpOld = mpcGuid;       // r3 = *(r31) = mpcGuid
    if (lpOld)                   // cmplwi r3,0; beq ...
    {
        MassiveFree(lpOld);      // off_82F91C18(r3)  (free hook)
        mpcGuid = 0;             // stw r11(=0), 0(r31)
    }
    return lpOld;
}

// ---------------------------------------------------------------------------
// CMassiveSystem::SetGuid @ 0x82BD2268
//
// Clears the current GUID, allocates a fresh 37-byte buffer into mpcGuid,
// zero-fills it, and strncpy's at most 37 bytes from pcGuid. Returns the strncpy
// destination (mpcGuid), or null when the allocation failed.
// ---------------------------------------------------------------------------
char* CMassiveSystem::SetGuid(const char* pcGuid)
{
    ClearMassiveGuid();                                     // bl ...ClearMassiveGuid
    char* lpcBuffer = static_cast<char*>(MassiveMalloc(37)); // malloc(0x25)
    mpcGuid = lpcBuffer;                                     // stw r3, 0(r31)
    if (!lpcBuffer)                                          // beq (alloc failed)
        return lpcBuffer;                                    // returns 0 in r3
    std::memset(lpcBuffer, 0, 37);                           // memset(...,0,0x25)
    return std::strncpy(mpcGuid, pcGuid, 37);                // strncpy(dst,src,0x25)
}

// ---------------------------------------------------------------------------
// CMassiveSystem::ScalarDeletingDestructor @ 0x82BD2560
//
// Clears the GUID, then frees the object itself through the heap hook iff the
// low bit of bDelete is set (and this is non-null). Returns this. The X360 dtor
// is NOT virtual -- there is no vftable rewrite, just the manual free.
// ---------------------------------------------------------------------------
void* CMassiveSystem::ScalarDeletingDestructor(char bDelete)
{
    ClearMassiveGuid();                  // bl ...ClearMassiveGuid
    if ((bDelete & 1) != 0 && this)      // clrlwi. r11,r30,31; ...; cmplwi r31,0
        MassiveFree(this);               // off_82F91C18(r31)  (free hook)
    return this;                         // r3 = r31
}

// ---------------------------------------------------------------------------
// CMassiveSystem::Initialize @ 0x82BD25C0
//
// Lazy singleton: if the slot is empty, MassiveMalloc(4) a system object, zero
// its GUID pointer (*result = 0), and store it back; an existing instance is
// returned untouched. A failed allocation stores (and returns) null.
// ---------------------------------------------------------------------------
CMassiveSystem* CMassiveSystem::Initialize()
{
    CMassiveSystem* lpInstance = gpMassiveSystemInstance;   // lwz r3, slot
    if (!lpInstance)                                         // bne -> skip
    {
        lpInstance = static_cast<CMassiveSystem*>(MassiveMalloc(4)); // malloc(4)
        if (lpInstance)
            lpInstance->mpcGuid = 0;                         // stw 0, 0(r3)
        else
            lpInstance = 0;                                  // li r3, 0
        gpMassiveSystemInstance = lpInstance;                // stw r3, slot
    }
    return lpInstance;
}

// ---------------------------------------------------------------------------
// CMassiveSystem::Instance @ 0x82BD2208
//
// Returns the raw singleton slot.
// ---------------------------------------------------------------------------
CMassiveSystem* CMassiveSystem::Instance()
{
    return gpMassiveSystemInstance;   // lwz r3, slot; blr
}

// ---------------------------------------------------------------------------
// CMassiveSystem::Shutdown @ 0x82BD26B8
//
// If the singleton exists, run its scalar deleting destructor (bDelete = 1) and
// clear the slot. Always returns 1.
// ---------------------------------------------------------------------------
int CMassiveSystem::Shutdown()
{
    CMassiveSystem* lpInstance = gpMassiveSystemInstance;   // lwz r3, slot
    if (lpInstance)                                         // beq -> return 1
    {
        lpInstance->ScalarDeletingDestructor(1);            // r4=1; bl ...dtor
        gpMassiveSystemInstance = 0;                        // stw 0, slot
    }
    return 1;                                               // li r3, 1
}

// ---------------------------------------------------------------------------
// CMassiveSystem::GetSystemTime @ 0x82BD22E0
//
// Returns GetTickCount() (clrldi r3,r3,32 -> zero-extended DWORD). std::clock
// here would be a different clock; the faithful source is the platform tick
// counter. FLAG: GetTickCount is a Win32/X360 platform API -- declared below and
// supplied by the platform layer.
// ---------------------------------------------------------------------------
unsigned int CMassiveSystem::GetSystemTime()
{
    return MassiveGetTickCount();   // bl GetTickCount; clrldi r3,r3,32
}

// ---------------------------------------------------------------------------
// CMassiveSystem::GetServerTimeFormatted @ 0x82BD2308
//
// Builds "YYYY-MM-DD HH:MM:SS,mmm" from a Unix-millisecond timestamp:
//   seconds = nMilliseconds / 1000  -> localtime64 -> tm
//   date  via "%d-%.2d-%.2d "      (tm_year+1900, tm_mon+1, tm_mday)
//   time  via "%.2d:%.2d:%.2d,%.3d" (tm_hour, tm_min, tm_sec, ms remainder)
//   final via "%s%s" (date then time) into pcBuffer
// On a localtime64 failure the whole buffer is set to "Time Unavailable".
// The asm reads tm fields by byte offset; the host struct tm member names map as:
//   *(tm+0x00)=tm_sec  *(tm+0x04)=tm_min  *(tm+0x08)=tm_hour  *(tm+0x0C)=tm_mday
//   *(tm+0x10)=tm_mon  *(tm+0x14)=tm_year
// ---------------------------------------------------------------------------
int CMassiveSystem::GetServerTimeFormatted(int /*nUnused*/, unsigned int nMilliseconds,
                                           char* pcBuffer, unsigned int nCount)
{
    __time64_t lnSeconds = static_cast<__time64_t>(nMilliseconds) / 1000; // divdu r30,1000
    struct tm* lpTm = _localtime64(&lnSeconds);                           // localtime64

    if (lpTm)
    {
        char lacDate[32];  // var_50
        char lacTime[32];  // var_70

        MassiveFormatString(lacDate, 0x20, "%d-%.2d-%.2d ",
                            lpTm->tm_year + 1900,   // r6 = *(tm+0x14)+0x76C
                            lpTm->tm_mon + 1,       // r7 = *(tm+0x10)+1
                            lpTm->tm_mday);         // r8 = *(tm+0x0C)

        unsigned int luMsRemainder = nMilliseconds - 1000u * (nMilliseconds / 1000u); // subf r9
        MassiveFormatString(lacTime, 0x20, "%.2d:%.2d:%.2d,%.3d",
                            lpTm->tm_hour,          // r6 = *(tm+0x08)
                            lpTm->tm_min,           // r7 = *(tm+0x04)
                            lpTm->tm_sec,           // r8 = *(tm+0x00)
                            luMsRemainder);         // r9

        return MassiveFormatString(pcBuffer, nCount, "%s%s", lacDate, lacTime); // r6=date,r7=time
    }

    return MassiveFormatString(pcBuffer, nCount, "Time Unavailable");
}

// ---------------------------------------------------------------------------
// CMassiveSystem::GetHardwareAddress @ 0x82BD23D0
//
// XNetGetTitleXnAddr -> XNADDR; on success allocate an 18-byte string into
// *ppcOut, zero it, and format the 6 MAC bytes (abEnet, struct +0x0A..+0x0F) as
// "%02X:%02X:%02X:%02X:%02X:%02X". Returns the XNetGetTitleXnAddr status (the
// negative error is returned directly; on alloc failure the positive status is
// returned without writing the string).
// ---------------------------------------------------------------------------
signed int CMassiveSystem::GetHardwareAddress(int /*nUnused*/, char** ppcOut)
{
    MassiveXnAddr lXnAddr;  // var_40 (XNADDR scratch)

    signed int lnStatus = XNetGetTitleXnAddr(&lXnAddr);  // r3 = status
    if (lnStatus < 0)                                    // blt -> return status
        return lnStatus;

    char* lpcBuffer = static_cast<char*>(MassiveMalloc(0x12)); // malloc(18)
    *ppcOut = lpcBuffer;                                       // stw r3, 0(r31)
    if (!lpcBuffer)                                            // beq -> return status
        return lnStatus;

    std::memset(lpcBuffer, 0, 0x12);                          // memset(...,0,18)

    // r6..r10 + a stacked 6th arg = abEnet[0..5] (struct +0x0A..+0x0F).
    MassiveFormatString(*ppcOut, 0x11, "%02X:%02X:%02X:%02X:%02X:%02X",
                        lXnAddr.abEnet[0],   // var_36 (+0x0A)
                        lXnAddr.abEnet[1],   // var_35 (+0x0B)
                        lXnAddr.abEnet[2],   // var_34 (+0x0C)
                        lXnAddr.abEnet[3],   // var_33 (+0x0D)
                        lXnAddr.abEnet[4],   // var_32 (+0x0E)
                        lXnAddr.abEnet[5]);  // var_31 (+0x0F, stacked vararg)
    return lnStatus;
}

// ---------------------------------------------------------------------------
// CMassiveSystem::CreateMassiveGuid @ 0x82BD2628
//
// Allocates a 37-byte buffer at *ppcOut, zero-fills it, then formats a
// pseudo-GUID "%8.8x-%I64d-%d" from a fixed tag, the wall-clock SYSTEMTIME, and
// the tick counter. Returns the buffer pointer (null on allocation failure).
//
// Operand mapping confirmed against the call-site registers (asm @ 0x82BD2680..):
//   r3 = *ppcOut (buffer)         r4 = 0x25 (count)        r5 = "%8.8x-%I64d-%d"
//   r6 = a1 (the char** address itself)        -> %8.8x  (hex of the slot ptr)
//   r7 = TickCount (clrldi, zero-extended 64b) -> %I64d   (the tick counter)
//   r8 = v17 * v16 (product of the two trailing SYSTEMTIME 16-bit words) -> %d
// FLAG: %8.8x prints the host pointer value of `ppcOut`; on the 64-bit host this
// is an 8-byte pointer truncated to 32 bits by %x, matching the X360's 32-bit
// pointer width only in the low word. The two SYSTEMTIME words (v16 at the
// 12-byte block +0x0C, v17 at +0x0E) are read back as the X360 stored them; the
// product is reproduced verbatim. The observable contract is a per-call unique
// GUID-shaped string of the form "%8.8x-%I64d-%d".
// ---------------------------------------------------------------------------
int CMassiveSystem::CreateMassiveGuid(char** ppcOut)
{
    char* lpcBuffer = static_cast<char*>(MassiveMalloc(0x25)); // malloc(37)
    *ppcOut = lpcBuffer;                                       // stw r3, 0(r31)
    if (!lpcBuffer)                                            // beq -> return result(=0)
        return 0;

    std::memset(lpcBuffer, 0, 0x25);                           // memset(...,0,37)

    // The X360 passes a SYSTEMTIME-shaped block to GetSystemTime (8 WORDs / 16
    // bytes); IDA typed only the leading 12 bytes as v15 but reads v16/v17 from
    // the last two WORDs (+0x0C/+0x0E), so the full 16-byte block is modelled.
    unsigned char lacSystemTime[16];                           // v15 (GetSystemTime BYREF)
    MassiveGetSystemTimeBlock(lacSystemTime);                  // GetSystemTime(v15)
    unsigned int luTickCount = MassiveGetTickCount();          // GetTickCount() -> r3

    // v16 @ block+0x0C (var_14), v17 @ block+0x0E (var_12); both 16-bit.
    unsigned short luWord16 = static_cast<unsigned short>(     // v16  (lhz var_14)
        lacSystemTime[12] | (lacSystemTime[13] << 8));
    unsigned short luWord17 = static_cast<unsigned short>(     // v17  (lhz var_12)
        lacSystemTime[14] | (lacSystemTime[15] << 8));

    unsigned int luProduct  = static_cast<unsigned int>(luWord17 * luWord16); // mullw r8,r9,r8

    unsigned int luSlotAddr = static_cast<unsigned int>(           // r6 = a1 (low 32b of slot ptr)
        reinterpret_cast<std::uintptr_t>(ppcOut));
    return MassiveFormatString(*ppcOut, 0x25, "%8.8x-%I64d-%d",
                               luSlotAddr,                          // %8.8x
                               static_cast<long long>(luTickCount), // r7 = TickCount (%I64d)
                               static_cast<int>(luProduct));        // r8 = v17*v16 (%d)
}

} // namespace MassiveAdClient3
