#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"

#include <cstring>  // strlen, strncpy

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

} // namespace MassiveAdClient3
