#pragma once

// ===========================================================================
// MassiveAd Client3 -- small client helper objects (vendor middleware).
//
// Five leaf MassiveAdClient3 client objects, all derived from the polymorphic
// CMassiveBaseObject homed in MassiveAdClient3.h:
//     CLog               -- the client's debug-log singleton object
//     CMassiveTime       -- a monotonic "client time" with a pause offset
//     CMassiveOrder      -- a (number, type) ad-order record
//     CAddressIndexPair  -- an (index byte, owned address-string) pair
//     CMassiveThread     -- a thin wrapper around one OS thread HANDLE
//
// There is NO reference source and NO DecFIGS dwarfdump for this subsystem;
// SHAPE and BODIES are both reconstructed from the X360 ARTIST.XEX pseudocode +
// disassembly. Per-function X360 addresses:
//     MassiveAdClient3::CLog::Initialize                            @ 0x82BD4A68
//     MassiveAdClient3::CLog::`scalar deleting destructor'          @ 0x82BD4A10
//     MassiveAdClient3::CMassiveTime::CMassiveTime                  @ 0x82BCC708
//     MassiveAdClient3::CMassiveTime::GetTime                       @ 0x82BD3868
//     MassiveAdClient3::CMassiveTime::`scalar deleting destructor'  @ 0x82BCD3F0
//     MassiveAdClient3::CMassiveOrder::CMassiveOrder                @ 0x82BDEFA8
//     MassiveAdClient3::CMassiveOrder::`vector deleting destructor' @ 0x82BDEFF8
//     MassiveAdClient3::CAddressIndexPair::CAddressIndexPair        @ 0x82BD4AF8
//     MassiveAdClient3::CAddressIndexPair::~CAddressIndexPair       @ 0x82BD4B88
//     MassiveAdClient3::CAddressIndexPair::`scalar deleting destructor' @ 0x82BD4BE8
//     MassiveAdClient3::CMassiveThread::CMassiveThread              @ 0x82BD4C38
//     MassiveAdClient3::CMassiveThread::~CMassiveThread             @ 0x82BD4C88
//     MassiveAdClient3::CMassiveThread::`scalar deleting destructor' @ 0x82BD4D48
//
// This is vendor/SDK code reconstructed in its canonical vendor home (a sibling
// of MassiveAdClient3.h under SDKs/Packages/MassiveAd). Per the naming
// convention the vendor SDK identifiers (the MassiveAdClient3 namespace and the
// CMassive* / CLog / CAddressIndexPair class names) are PRESERVED VERBATIM --
// they are an external middleware API, not project-owned code.
//
// Every one of these classes is a CMassiveBaseObject subclass that, like
// CMassiveCriticalSection in the sibling header, installs its own X360 vftable
// (off_82186014 / off_82183C8C / off_82187CAC / off_82186020 / off_82186038)
// over the base slot. That vftable rewrite is the compiler-emitted artifact of
// declaring a virtual destructor on each class, so it is modelled by giving each
// class a virtual ~dtor rather than writing the vftable store by hand.
//
// Layouts are taken from the X360 store offsets (base CMassiveBaseObject is the
// vftable + four dwords = 0x14 bytes; absolute offsets below are X360-relative
// and reproduced by member NAME, not asserted on the 64-bit host):
//   CLog              -- no own members (allocated as 0x14 bytes via
//                        CMassiveListNode::operator new(0x14))
//   CMassiveTime      +0x18  mnStartTime  (__int64; std @0x18)
//                     +0x20  mnPauseBase  (__int64; std @0x20)
//   CMassiveOrder     +0x14  mnNumber     (a2)
//                     +0x18  mnType       (a3)
//                     +0x1C  mnField1C    (0)
//                     +0x20  mnField20    (0)
//   CAddressIndexPair +0x14  mbIndex      (char a3)
//                     +0x18  mpcAddress   (owned copy of a2, or 0)
//   CMassiveThread    +0x14  mhThread     (OS thread HANDLE, 0 until started)
// ===========================================================================

#include <cstddef>

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// MassiveAd thread-handle platform hook (FLAGGED platform API).
//
// CMassiveThread's destructor closes its OS thread HANDLE through the Win32/X360
// CloseHandle kernel API. The handle is an opaque OS pointer (void*); the close
// is routed through a MassiveAd platform hook here so this header does not pull
// in <windows.h>. FLAG: this wraps the OS handle-close primitive -- external/
// platform, not project-owned. Declared here, supplied by the MassiveAd platform
// layer (another TU).
//   MassiveThreadHandle              -- one opaque OS thread HANDLE
//   MassiveCloseThreadHandle(handle) -> CloseHandle(handle)
// ---------------------------------------------------------------------------
typedef void* MassiveThreadHandle;
void MassiveCloseThreadHandle(MassiveThreadHandle hThread);

// One OS thread entry point: the routine CMassiveThread::Create spins up (the X360
// passes MassiveAdClient3::DNS to it from CNetworkManager::ResolveAddresses).
typedef unsigned long (*MassiveThreadProc)(void* pParam);

// ---------------------------------------------------------------------------
// CLog -- the MassiveAd client's debug-log object (a lazy process singleton).
//
// The X360 keeps the log object in one .data slot (dword_8327F390) plus a
// "log initialised" flag in a second slot (dword_8327F394). Initialize()
// allocates the object via CMassiveListNode::operator new(0x14) (it is a
// 0x14-byte CMassiveBaseObject with no own members), chains the base ctor with
// name "CLog", installs this class's vftable, and stores it into the slot; the
// flag slot is always set to 1. A pre-existing instance short-circuits to 0.
// The object adds no fields of its own.
// ---------------------------------------------------------------------------
class CLog : public CMassiveBaseObject
{
public:
    // @ 0x82BD4A68. Lazily creates the singleton CLog into dword_8327F390 and
    // sets the "initialised" flag dword_8327F394 to 1. Returns 0 (without
    // touching the flag) when an instance already existed, otherwise 1.
    static int Initialize();

    // @ 0x82BD4A10 (scalar deleting destructor). Rewrites the vftable
    // (compiler-emitted for a virtual dtor) and chains the base dtor; the
    // deleting-destructor thunk then frees the object through the base operator
    // delete when its low bit is set.
    virtual ~CLog();

private:
    // The X360 ctor takes no name argument from the caller -- it hard-codes
    // "CLog"; expose that as the public Initialize entry point above. A private
    // ctor keeps the only construction path through Initialize/operator new.
    CLog();
};

// ---------------------------------------------------------------------------
// CMassiveTime -- a monotonic client clock with a pause offset.
//
// The X360 ctor zeroes two 64-bit fields (mnStartTime @ +0x18, mnPauseBase @
// +0x20) over the base. GetTime() returns
//   mnStartTime - mnPauseBase + CMassiveSystem::GetSystemTime()
// i.e. the live system tick shifted by the (start - pause) bias. Both bias
// fields are 64-bit (the X360 uses std/ld), so the host members are int64.
// ---------------------------------------------------------------------------
class CMassiveTime : public CMassiveBaseObject
{
public:
    // @ 0x82BCC708. Chains the base ctor ("CMassiveTime"), installs this class's
    // vftable, and zeroes both 64-bit bias fields.
    CMassiveTime();

    // @ 0x82BCD3F0 (scalar deleting destructor). No own teardown beyond the base
    // dtor; the deleting-destructor thunk frees via the base operator delete.
    virtual ~CMassiveTime();

    // @ 0x82BD3868. Returns mnStartTime - mnPauseBase + the live system tick
    // (CMassiveSystem::GetSystemTime()). The X360 computes the (start - pause)
    // bias in 64-bit then adds the 32-bit system tick.
    long long GetTime();

private:
    long long mnStartTime;  // +0x18 (std/ld; bias start)
    long long mnPauseBase;  // +0x20 (std/ld; bias pause base)
};

// ---------------------------------------------------------------------------
// CMassiveOrder -- a MassiveAd (number, type) ad-order record.
//
// Built by CRequestEnterZone::ReadOrderBlock from two parsed dwords. The X360
// ctor stores a2 -> mnNumber (+0x14), a3 -> mnType (+0x18), then zeroes two
// trailing dwords (+0x1C, +0x20). Their semantics are not exercised by any
// recovered reader, so they are modelled by name (mnField1C / mnField20) as the
// zero-initialised slots the X360 writes.
// ---------------------------------------------------------------------------
class CMassiveOrder : public CMassiveBaseObject
{
    // CMassiveZoneManager::SetAssetExpiredByOrderID @ 0x82BD3710 reads mnNumber
    // directly on the X360 (`*(asset->mpOrder + 0x14) == nOrderId`, a bare lwz
    // through the asset's attached order); the zone manager owns the mOrderList
    // the orders live on. Friendship keeps that a named-member read rather than
    // an offset hack (same pattern as CMassiveAsset::mnAssetId).
    friend class CMassiveZoneManager;

    // CRequestEnterZone::ReadOrderBlock @ 0x82BDBEE0 constructs the order from
    // the wire block (tag 19 -> mnNumber, tag 3 -> mnType) and reads mnNumber
    // back for its "Attached Asset(%d) to Order(%d)" log line.
    friend class CRequestEnterZone;

public:
    // @ 0x82BDEFA8. Chains the base ctor ("CMassiveOrder"), installs this
    // class's vftable, stores (nNumber, nType), and zeroes the two trailing
    // fields.
    CMassiveOrder(int nNumber, int nType);

    // @ 0x82BDEFF8 (vector deleting destructor). Rewrites the vftable and chains
    // the base dtor; the deleting thunk frees via the base operator delete.
    virtual ~CMassiveOrder();

private:
    int mnNumber;   // +0x14 (a2)
    int mnType;     // +0x18 (a3)
    int mnField1C;  // +0x1C (zeroed)
    int mnField20;  // +0x20 (zeroed)
};

// ---------------------------------------------------------------------------
// CAddressIndexPair -- an (index byte, owned address string) pair.
//
// The X360 ctor stores the index byte a3 -> mbIndex (+0x14), nulls mpcAddress
// (+0x18), then -- iff the address string a2 is non-null and non-empty --
// MassiveMalloc(strlen+1)'s a copy into mpcAddress and strncpy's it (left null
// on a failed/skipped allocation). The destructor frees mpcAddress through the
// MassiveAd free hook, nulls it, and clears the index byte before chaining the
// base dtor.
// ---------------------------------------------------------------------------
class CAddressIndexPair : public CMassiveBaseObject
{
public:
    // @ 0x82BD4AF8. Chains the base ctor ("CAddressIndexPair"), installs this
    // class's vftable, stores the index byte, and copies the address string (if
    // valid) into a MassiveMalloc'd buffer.
    CAddressIndexPair(const char* pcAddress, char bIndex);

    // @ 0x82BD4B88. Frees mpcAddress through the MassiveAd heap hook, nulls it,
    // clears mbIndex, then chains the base dtor. The X360 dtor is virtual (it
    // rewrites the vftable first).
    virtual ~CAddressIndexPair();

    // The owned address string (read from +0x18 for the trace line in
    // CRequestLocateService::Parse, which logs each pair's address after it has
    // copied it in). Additive accessor (FLAG: not its own X360 function) so the
    // consumer reads the field BY NAME rather than reaching into the private copy.
    const char* GetAddress() const { return mpcAddress; }

private:
    char  mbIndex;     // +0x14 (a3)
    char* mpcAddress;  // +0x18 (owned copy of a2, or 0)
};

// ---------------------------------------------------------------------------
// CMassiveThread -- a thin wrapper around one OS thread HANDLE.
//
// The X360 ctor chains the base ctor (name "MassiveThread"), installs this
// class's vftable, and zeroes the handle slot (+0x14). The destructor closes
// the handle via CloseHandle (the MassiveCloseThreadHandle platform hook) and
// chains the base dtor. The recovered ctor takes no handle argument -- the
// handle is assigned later by the thread-start path (not in this TU), so it
// starts null.
// ---------------------------------------------------------------------------
class CMassiveThread : public CMassiveBaseObject
{
public:
    // @ 0x82BD4C38. Chains the base ctor ("MassiveThread"), installs this
    // class's vftable, and nulls the thread handle.
    CMassiveThread();

    // @ 0x82BD4C88. Closes the thread handle (CloseHandle / the platform hook)
    // and chains the base dtor. The X360 dtor is virtual (it rewrites the
    // vftable first).
    virtual ~CMassiveThread();

    // Starts the OS thread on pfnProc(pParam), storing its handle into mhThread.
    // Separate ledger TU (its body wraps the platform thread-create primitive);
    // declared here -- CMassiveThread is its home -- because CNetworkManager::
    // ResolveAddresses spins the DNS thread up through it (a direct bl in that TU's
    // asm).
    void Create(MassiveThreadProc pfnProc, void* pParam);

private:
    MassiveThreadHandle mhThread;  // +0x14 (OS thread HANDLE, 0 until started)
};

// ---------------------------------------------------------------------------
// CFlag -- a bare named CMassiveBaseObject (a MassiveAd boolean/flag object).
//
// Reconstructed from the X360 ARTIST.XEX (no leak / DecFIGS):
//     MassiveAdClient3::CFlag::`vector deleting destructor' @ 0x82BCC758
//
// The vector deleting destructor (the only recovered CFlag function) installs
// CFlag's own vftable (off_82183CA0), chains ~CMassiveBaseObject, and -- when
// the low bit of the delete flag is set -- frees the object through
// CMassiveBaseObject::operator delete:
//     *a1 = &off_82183CA0;                            // install CFlag vftable
//     CMassiveBaseObject::~CMassiveBaseObject(a1);    // chain base dtor
//     if ( (a2 & 1) != 0 )
//         CMassiveBaseObject::operator delete(a1);    // conditional free
//     return a1;
// The vftable rewrite + base-dtor chain are the compiler-emitted virtual-dtor
// body; the conditional free is the deleting-destructor thunk synthesised around
// it. CFlag tears down nothing of its own -- it adds no fields (like CLog), so
// it is just the polymorphic base with its own vftable.
// ---------------------------------------------------------------------------
class CFlag : public CMassiveBaseObject
{
public:
    // The X360 has no standalone CFlag ctor symbol in this TU; the recovered
    // function is the vector deleting destructor, which the virtual dtor models.
    // A name-taking ctor chains the base (the MassiveAd objects are always
    // constructed with a name); no name is asserted from the asm of this TU.
    CFlag(const char* pcName);

    // @ 0x82BCC758 (vector deleting destructor). Rewrites the vftable
    // (off_82183CA0) and chains the base dtor; the deleting-destructor thunk
    // frees via the base operator delete when its low bit is set. No own teardown.
    virtual ~CFlag();
};

} // namespace MassiveAdClient3
