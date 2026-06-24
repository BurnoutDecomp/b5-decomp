#include "SDKs/Packages/MassiveAd/MassiveAdClient3Objects.h"

#include <cstddef>  // std::size_t
#include <cstring>  // strlen, strncpy
#include <new>      // placement new (CLog::Initialize construction over operator new)

// ===========================================================================
// MassiveAdClient3 -- CLog / CMassiveTime / CMassiveOrder / CAddressIndexPair /
// CMassiveThread definition home.
//
// SHAPE and BODIES are both reconstructed from BURNOUT_X360_ARTIST.XEX (there is
// no leak source / DecFIGS for this vendor middleware). Stores are reproduced
// member-for-member against the X360 disassembly; see MassiveAdClient3Objects.h
// for the per-offset layout map. Each class installs its own X360 vftable over
// the CMassiveBaseObject slot -- modelled by the virtual destructors declared in
// the header, so no vftable store is written by hand.
// ===========================================================================

namespace MassiveAdClient3
{

// ===========================================================================
// CLog -- the MassiveAd client debug-log singleton.
// ===========================================================================

// dword_8327F390 -- the single .data slot holding the lazily-created CLog.
// dword_8327F394 -- the "log initialised" flag slot. Both are defined here so
// each .data slot has exactly one home; Initialize touches them by name.
static CLog* gpLogInstance = 0;
static int   gnLogInitialised = 0;

// ---------------------------------------------------------------------------
// CLog::CLog
//
// The X360 has no standalone CLog ctor symbol: Initialize inlines the base ctor
// chain (CMassiveBaseObject(this, "CLog")) and the vftable install directly.
// Modelled here as a private ctor that chains the base with the hard-coded name
// "CLog"; the virtual dtor declaration installs this class's vftable
// (off_82186014) automatically.
// ---------------------------------------------------------------------------
CLog::CLog()
    : CMassiveBaseObject("CLog")
{
}

// ---------------------------------------------------------------------------
// CLog::~CLog @ 0x82BD4A10 (scalar deleting destructor body)
//
// The X360 scalar deleting destructor rewrites the vftable (off_82186014),
// chains ~CMassiveBaseObject, and -- when its low bit is set -- frees the object
// through CMassiveBaseObject::operator delete. The vftable rewrite + base-dtor
// chain are the compiler-emitted virtual-dtor body; the conditional free is the
// deleting-destructor thunk the compiler emits around it. Nothing of CLog's own
// is torn down (it has no fields).
// ---------------------------------------------------------------------------
CLog::~CLog()
{
}

// ---------------------------------------------------------------------------
// CLog::Initialize @ 0x82BD4A68
//
// Lazy singleton: when the slot is already populated, return 0 immediately
// (leaving the flag untouched). Otherwise allocate a CLog via
// CMassiveListNode::operator new(0x14), chain the base ctor ("CLog") + install
// the vftable (the placement-new construction below), store it into the slot
// (or store null on allocation failure), set the "initialised" flag to 1, and
// return 1.
// ---------------------------------------------------------------------------
int CLog::Initialize()
{
    if (gpLogInstance)            // if (dword_8327F390) return 0;
        return 0;

    // r3 = 0x14; CMassiveListNode::operator new(0x14) -- the X360 allocates the
    // CLog object through the list-node heap hook. Placement-new runs the base
    // ctor chain + vftable install that the X360 inlines here.
    void* lpMemory = CMassiveListNode::operator new(static_cast<std::size_t>(0x14));
    if (lpMemory)                                 // if (v1) { ...construct...; slot = v2; }
        gpLogInstance = new (lpMemory) CLog();
    else
        gpLogInstance = 0;                        // else dword_8327F390 = 0;

    gnLogInitialised = 1;                         // dword_8327F394 = 1;
    return 1;
}

// ===========================================================================
// CMassiveTime -- a monotonic client clock with a pause offset.
// ===========================================================================

// ---------------------------------------------------------------------------
// CMassiveTime::CMassiveTime @ 0x82BCC708
//
// X360 store order: chain CMassiveBaseObject(this, "CMassiveTime"), then
// std 0,0x18 (mnStartTime = 0), install the vftable (off_82183C8C), std 0,0x20
// (mnPauseBase = 0). Both bias fields are 64-bit zero.
// ---------------------------------------------------------------------------
CMassiveTime::CMassiveTime()
    : CMassiveBaseObject("CMassiveTime")
    , mnStartTime(0)   // std r10(=0), 0x18(r31)
    , mnPauseBase(0)   // std r10(=0), 0x20(r31)
{
}

// ---------------------------------------------------------------------------
// CMassiveTime::~CMassiveTime @ 0x82BCD3F0 (scalar deleting destructor body)
//
// No own teardown -- the X360 scalar deleting destructor chains
// ~CMassiveBaseObject and conditionally frees through the base operator delete.
// ---------------------------------------------------------------------------
CMassiveTime::~CMassiveTime()
{
}

// ---------------------------------------------------------------------------
// CMassiveTime::GetTime @ 0x82BD3868
//
// asm:  CMassiveSystem::Instance(); v2 = CMassiveSystem::GetSystemTime();
//       r11 = ld 0x18 (mnStartTime); r10 = ld 0x20 (mnPauseBase);
//       r11 = r11 - r10; r3 = r11 + v2.
// The X360 calls Instance() then GetSystemTime() (a static returning the live
// tick); the bias (mnStartTime - mnPauseBase) is computed in 64-bit and the
// 32-bit system tick is added on.
// ---------------------------------------------------------------------------
long long CMassiveTime::GetTime()
{
    CMassiveSystem::Instance();                              // bl ...Instance (result unused)
    unsigned int lnSystemTime = CMassiveSystem::GetSystemTime(); // v2 = GetSystemTime()
    return mnStartTime - mnPauseBase + lnSystemTime;         // (start - pause) + tick
}

// ===========================================================================
// CMassiveOrder -- a (number, type) ad-order record.
// ===========================================================================

// ---------------------------------------------------------------------------
// CMassiveOrder::CMassiveOrder @ 0x82BDEFA8
//
// X360 store order: chain CMassiveBaseObject(this, "CMassiveOrder"), stw a2,0x14
// (mnNumber), stw a3,0x18 (mnType), install the vftable (off_82187CAC),
// stw 0,0x1C and stw 0,0x20 (the two trailing fields).
// ---------------------------------------------------------------------------
CMassiveOrder::CMassiveOrder(int nNumber, int nType)
    : CMassiveBaseObject("CMassiveOrder")
    , mnNumber(nNumber)   // stw r30(=a2), 0x14(r31)
    , mnType(nType)       // stw r29(=a3), 0x18(r31)
    , mnField1C(0)        // stw r10(=0), 0x1C(r31)
    , mnField20(0)        // stw r10(=0), 0x20(r31)
{
}

// ---------------------------------------------------------------------------
// CMassiveOrder::~CMassiveOrder @ 0x82BDEFF8 (vector deleting destructor body)
//
// No own teardown -- the X360 vector deleting destructor rewrites the vftable
// (off_82187CAC), chains ~CMassiveBaseObject, and conditionally frees through
// the base operator delete.
// ---------------------------------------------------------------------------
CMassiveOrder::~CMassiveOrder()
{
}

// ===========================================================================
// CAddressIndexPair -- an (index byte, owned address string) pair.
// ===========================================================================

// ---------------------------------------------------------------------------
// CAddressIndexPair::CAddressIndexPair @ 0x82BD4AF8
//
// X360 store order: chain CMassiveBaseObject(this, "CAddressIndexPair"),
// stb a3,0x14 (mbIndex), stw 0,0x18 (mpcAddress = 0), install the vftable
// (off_82186020). Then, iff a2 (pcAddress) is non-null AND strlen(a2) != 0,
// MassiveMalloc(strlen+1) -> mpcAddress (stored even on failure) and, on a
// non-null buffer, strncpy(buffer, a2, strlen+1).
//   asm: r30 = a2 (address) captured before the base ctor; v6 = strlen(a2);
//        v7 = v6 + 1 (the strncpy count); v8 = MassiveMalloc(v7); a1+0x18 = v8.
// ---------------------------------------------------------------------------
CAddressIndexPair::CAddressIndexPair(const char* pcAddress, char bIndex)
    : CMassiveBaseObject("CAddressIndexPair")
    , mbIndex(bIndex)    // stb r29(=a3), 0x14(r31)
    , mpcAddress(0)      // stw r10(=0), 0x18(r31)
{
    if (pcAddress)                                     // if (a2)
    {
        std::size_t luLength = std::strlen(pcAddress); // v6 = strlen(a2)
        if (luLength != 0)                             // if (v6)
        {
            std::size_t luCount = luLength + 1;        // v7 = v6 + 1
            char* lpcBuffer = static_cast<char*>(MassiveMalloc(luCount)); // v8 = MassiveMalloc(v7)
            mpcAddress = lpcBuffer;                    // a1+0x18 = v8
            if (lpcBuffer)                             // if (v8)
                std::strncpy(lpcBuffer, pcAddress, luCount); // strncpy(v8, a2, v7)
        }
    }
}

// ---------------------------------------------------------------------------
// CAddressIndexPair::~CAddressIndexPair @ 0x82BD4B88
//
// X360: v2 = mpcAddress (+0x18); rewrite the vftable (off_82186020); if (v2)
// free-hook(v2); then stb 0,0x14 (mbIndex = 0); chain ~CMassiveBaseObject.
// MassiveFree already null-checks, so the guarded free is a straight forward.
// ---------------------------------------------------------------------------
CAddressIndexPair::~CAddressIndexPair()
{
    if (mpcAddress)            // v2 = a1+0x18; cmplwi r3,0; beq ...
    {
        MassiveFree(mpcAddress); // off_82F91C18(v2)  (free hook)
    }                          // (asm does NOT zero +0x18 here -- no dead store)
    mbIndex = 0;              // stb r11(=0), 0x14(r31)
    // ~CMassiveBaseObject() runs next (compiler-chained).
}

// ===========================================================================
// CMassiveThread -- a thin wrapper around one OS thread HANDLE.
// ===========================================================================

// ---------------------------------------------------------------------------
// CMassiveThread::CMassiveThread @ 0x82BD4C38
//
// X360 store order: chain CMassiveBaseObject(this, "MassiveThread"), stw 0,0x14
// (mhThread = 0), install the vftable (off_82186038).
// ---------------------------------------------------------------------------
CMassiveThread::CMassiveThread()
    : CMassiveBaseObject("MassiveThread")
    , mhThread(0)   // stw r10(=0), 0x14(r31)
{
}

// ---------------------------------------------------------------------------
// CMassiveThread::~CMassiveThread @ 0x82BD4C88
//
// X360: v2 = mhThread (+0x14); rewrite the vftable (off_82186038);
// CloseHandle(v2); chain ~CMassiveBaseObject. The close runs unconditionally
// (the X360 passes the raw handle, including 0, to CloseHandle); routed here
// through the MassiveCloseThreadHandle platform hook.
// ---------------------------------------------------------------------------
CMassiveThread::~CMassiveThread()
{
    MassiveCloseThreadHandle(mhThread);  // r3 = ld 0x14; bl CloseHandle
    // ~CMassiveBaseObject() runs next (compiler-chained).
}

// ===========================================================================
// CFlag -- a bare named CMassiveBaseObject.
// ===========================================================================

// ---------------------------------------------------------------------------
// CFlag::CFlag
//
// The X360 has no standalone CFlag ctor symbol in this TU; the only recovered
// CFlag function is the vector deleting destructor (0x82BCC758). The ctor is
// modelled as a base-chaining ctor (the MassiveAd leaf objects are always built
// with a name); the virtual dtor declaration installs CFlag's vftable
// (off_82183CA0). No fields of its own.
// ---------------------------------------------------------------------------
CFlag::CFlag(const char* pcName)
    : CMassiveBaseObject(pcName)
{
}

// ---------------------------------------------------------------------------
// CFlag::~CFlag @ 0x82BCC758 (vector deleting destructor body)
//
// X360 store order: stw off_82183CA0, 0(r31) (install CFlag vftable), then
// bl ~CMassiveBaseObject (chain base dtor), then -- when (a2 & 1) -- bl
// CMassiveBaseObject::operator delete (conditional free); return this. The
// vftable rewrite + base-dtor chain are the compiler-emitted virtual-dtor body;
// the conditional free is the deleting-destructor thunk MSVC synthesises around
// it. Nothing of CFlag's own is torn down (it has no fields).
// ---------------------------------------------------------------------------
CFlag::~CFlag()
{
}

} // namespace MassiveAdClient3
