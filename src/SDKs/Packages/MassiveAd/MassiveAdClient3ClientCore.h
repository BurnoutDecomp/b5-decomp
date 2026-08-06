#pragma once

// ===========================================================================
// MassiveAdClient3::CMassiveClientCore -- minimal owning home (vendor
// middleware).
//
// The full CMassiveClientCore body (CMassiveClientCore ctor, RequestSessionOpen/
// Close, RequestLocateService, SetIsPaused, ...) is a SEPARATE ledger TU. This
// header only pins the surface the committed CRequestObject TU
// (MassiveAdClient3Request.h/.cpp) is attested to touch, so the class can be
// grown here later:
//
//   - Instance(): direct bl from CRequestObject::FinishBaseBlock @ 0x82BD04E8
//   - GetTime(core): direct bl on the Instance() result; its 64-bit return
//     (PPC std of r3) is the request timestamp appended to the wire buffer
//
// (BrnMassive.h forward-declares this same class and reaches the core through
// game-side wrapper declarations; this vendor-side home is the class's owning
// header.)
//
// ZONE-MANAGER SLICE (grown for CMassiveZoneManager::CreateImpUpdateReque
// @ 0x82BD2E00 / HandleResponse @ 0x82BD3410 / Tick @ 0x82BD3130): the class is
// now modelled with its ATTESTED base and first own member --
//   - The X360 ctor @ 0x82BCD440 chains
//     `CRequestBuilder::CRequestBuilder(this, "CMassiveClientCore")` and installs
//     vftable off_821840B4, then overrides the builder's completion callbacks
//     (HandleResponse @ 0x82BCE260 / HandleError @ 0x82BCD310, both in the
//     ledger). So the client core IS a CRequestBuilder; the "client state" dword
//     the zone layer reads at core+0x10 is the inherited CMassiveBaseObject
//     valid/state dword (5 = initialised by the ctor; 4 = shutting down, gated
//     by CreateImpUpdateReque; 13/15 -> 14 folded by the zone manager's
//     HandleResponse; 15 stored by its Tick on a submitted enter-zone request).
//   - The ctor's next construction is `CMassiveBenchmark(this + 0x28)` -- the
//     first own member past the 0x28-byte X360 CRequestBuilder base. Tick reads
//     the benchmark's three counters (core+0x3C/+0x40/+0x44 = benchmark
//     +0x14/+0x18/+0x1C) as the bandwidth-usage triple for CRequestEnterZone/
//     CRequestExitZone::CreateRequest, and AddBenchmarkData accumulates into it.
//   - Members past +0x48 (critical section @ +0x48, time @ +0x80, the CFlag pair
//     @ +0xA8/+0xC0, the zone list @ +0x114, the current-zone slot @ +0x124, ...)
//     are NOT modelled yet -- nothing committed touches them by name; the full
//     CMassiveClientCore TU recovers them. Growth stays additive.
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestBuilder.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Benchmark.h"

namespace MassiveAdClient3
{

// The client's ad-zone manager (its own ledger TU); GetCurrentZone returns the
// active one. Pointer-only use here -- the owning home is
// MassiveAdClient3ZoneManager.h.
class CMassiveZoneManager;

// Custom heap-hook function-pointer types installed via
// CMassiveClientCore::SetCustomMemoryFunctions -- the game routes MassiveAd
// allocations through its own heap (see BrnMassive::System360HWMassive::
// BurnoutMassiveMalloc / BurnoutMassiveFree).
typedef void* (*TMassiveMallocFn)(unsigned int nSize);
typedef void  (*TMassiveFreeFn)(void* pBlock);

// Initialisation parameters passed BY POINTER to CMassiveClientCore::Initialize.
// Reconstructed from the store offsets at the game's init call site
// (BrnMassive::System360HWMassive::Prepare @ 0x823AB258, which builds this block on
// the stack): a leading application-name / version pair, four zero-initialised
// fields, an integer field set to 10000, and a trailing pair of string fields. Only
// the application-name/version pair is name-attested; the remaining members are named
// by offset (the vendor SDK's real field names are not recovered here).
struct SMassiveClientInit
{
    const char* mpcApplicationName;  // +0x00  e.g. "burnout_5_x360_na"
    const char* mpcVersion;          // +0x04  e.g. "1.0"
    int         mnUnknown08;         // +0x08  zero-initialised
    short       mnUnknown0C;         // +0x0C  zero-initialised (16-bit store)
    short       mnUnknown0E;         // +0x0E  pad within the zeroed dword
    int         mnUnknown10;         // +0x10  zero-initialised
    int         mnUnknown14;         // +0x14  zero-initialised
    int         mnUnknown18;         // +0x18  set to 10000 at the call site
    const char* mpcUnknown1C;        // +0x1C  e.g. "Shawn"
    const char* mpcUnknown20;        // +0x20  e.g. "None"
};

class CMassiveClientCore : public CRequestBuilder
{
    // CMassiveZoneManager::Tick @ 0x82BD3130 reads this core's embedded
    // benchmark counters directly on the X360 (`lwz r6/r7/r10, 0x3C/0x40/0x44`
    // off the Instance() result); friendship keeps that a named-member read.
    // (The zone manager's reads/writes of the inherited base state dword at
    // +0x10 are granted by the matching friendship on CMassiveBaseObject.)
    friend class CMassiveZoneManager;

public:
    // @ 0x82BCD440. Chains CRequestBuilder("CMassiveClientCore"), installs this
    // class's vftable (off_821840B4), constructs the embedded benchmark /
    // critical-section / time / flag members, registers itself as the singleton
    // (off_8327F294 = this), then validates pInit (IsInitStructValid) and seeds
    // the config fields -- state 5 on success, 0 on a bad init struct. Body in
    // the CMassiveClientCore TU.
    CMassiveClientCore(const SMassiveClientInit* pInit);

    // @ 0x82BCD5D0 (slot 0; the vector deleting destructor @ 0x82BCE560 is the
    // compiler-emitted thunk around it). Body in the CMassiveClientCore TU.
    virtual ~CMassiveClientCore();

    // @ 0x82BCE260 (CRequestBuilder slot 1 override). Advances the client-core
    // session state machine on a completed OpenSession/CloseSession/
    // LocateService/Heartbeat request. Body in the CMassiveClientCore TU.
    int HandleResponse(CRequestObject* pRequest) override;

    // @ 0x82BCD310 (CRequestBuilder slot 2 override). Records the failed
    // request's error against the client core. Body in the CMassiveClientCore
    // TU.
    int HandleError(CRequestObject* pRequest, int nErrorCode) override;

    // Direct bl target: returns the live client-core singleton. Body in the
    // CMassiveClientCore TU.
    static CMassiveClientCore* Instance();

    // Direct (non-virtual) bl target: the client clock, returned 64-bit (the
    // X360 stores the result with std). Body in the CMassiveClientCore TU.
    long long GetTime();

    // Direct (non-virtual) bl target from CMassiveAsset::HandleResponse
    // @ 0x82BDA418: on a completed asset download it records a benchmark sample
    // on the live client-core singleton. The call site fixes the argument shape --
    // a2 is the downloaded payload length (the request's mnDataLength, a 32-bit
    // lwz) and a3 is the request's +0x38 field (an i64 ld, a timestamp/duration).
    // Declared here (CMassiveClientCore is its home); body in the CMassiveClientCore
    // TU.
    void AddBenchmarkData(int nDataLength, long long nBenchmarkTime);

    // Direct (non-virtual) bl target: the client's currently-active ad zone, or
    // null when no zone is entered. Called on the Instance() result by
    // CMassiveAdObjectSubscriber's constructor @ 0x82BCEA18 / 0x82BCEA58. Body in
    // the CMassiveClientCore TU.
    CMassiveZoneManager* GetCurrentZone();

    // @ 0x82BCCCC8. Direct (non-virtual) bl target from CMassiveZoneManager::
    // HandleResponse @ 0x82BD3410 (the exit-complete tail): unlinks pZoneManager
    // from the core's zone list (clearing the current-zone slot when it matches)
    // and DELETES it through its vftable slot 0; -300 when it was not listed.
    // Body in the CMassiveClientCore TU.
    int ZoneManagerRemove(CMassiveZoneManager* pZoneManager);

    // @ 0x82BCCE68. Direct (non-virtual) bl target from CRequestOpenSession::Parse
    // @ 0x82BD4E58 (the zone-name response field, wire tag 71), called on the
    // Instance() result -- MEASURED at 0x82BD4F8C..0x82BD4F94, where r3 chains
    // straight from the singleton getter into this member call. Registers
    // pcZoneName in the core's zone-name list (X360 core+0x104) unless
    // ZoneNameFind already knows it: strlen+1 MassiveMalloc copy, then a new
    // CMassiveListNode appended via CMassiveList::Append. Returns 1 on append,
    // 0 when already present or on a failed allocation (level-2 "ALLOCATION
    // Failed for pName", SetLastError -99, zeroes the state dword at +0x10).
    // Not a clipped phantom: 0x82BCCE68 has its own full body, and the export
    // carries the sibling ZoneNameRemove under the same prefix.
    // Body in the CMassiveClientCore TU.
    int ZoneNameAdd(const char* pcZoneName);

    // Install the game-supplied heap hooks. Static; body in its own ledger TU.
    static void SetCustomMemoryFunctions(TMassiveMallocFn pfnMalloc, TMassiveFreeFn pfnFree);

    // Create the client core from the init params; returns the new instance (null on
    // failure). Static; body in its own ledger TU.
    static CMassiveClientCore* Initialize(const SMassiveClientInit* pInit);

    // Emit a client-core log line (printf-style, variadic). Static; body in its own TU.
    static void Log(int nLevel, const char* pcName, const char* pcFormat, ...);

    // Enter an ad zone by name. Static; body in its own ledger TU.
    static int EnterZone(const char* pcZone);

    // Exit an ad zone by name. Static; body in its own ledger TU. Returns the live
    // client core, which the game's Release path uses as the FlushImpressions receiver.
    static CMassiveClientCore* ExitZone(const char* pcZone);

    // Flush the queued ad impressions on this core. Body in its own ledger TU.
    int FlushImpressions();

    // Tear the client core down (nFlush flag, timeout in ms); returns non-zero on
    // success. Static; body in its own ledger TU.
    static int Shutdown(int nFlush, unsigned int nTimeoutMs);

private:
    // First own member past the CRequestBuilder base (X360 +0x28; the ctor
    // constructs `CMassiveBenchmark(this + 0x28)` immediately after the base
    // chain). Accumulates the zone session's bandwidth samples: AddBenchmarkData
    // -> AddData(nDataLength, nBenchmarkTime), and CMassiveZoneManager::Tick
    // reads the three counters (+0x3C/+0x40/+0x44 = mnTotalA/mnTotalB/mnCount)
    // as the bandwidth-usage triple for the enter/exit-zone requests.
    CMassiveBenchmark mBenchmark;  // +0x28
};

} // namespace MassiveAdClient3
