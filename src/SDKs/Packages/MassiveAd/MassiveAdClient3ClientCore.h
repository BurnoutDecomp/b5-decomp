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
// ===========================================================================

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

class CMassiveClientCore
{
public:
    // Direct bl target: returns the live client-core singleton. Body in the
    // CMassiveClientCore TU.
    static CMassiveClientCore* Instance();

    // Direct (non-virtual) bl target: the client clock, returned 64-bit (the
    // X360 stores the result with std). Body in the CMassiveClientCore TU.
    long long GetTime();

    // Direct (non-virtual) bl target: the client's currently-active ad zone, or
    // null when no zone is entered. Called on the Instance() result by
    // CMassiveAdObjectSubscriber's constructor @ 0x82BCEA18 / 0x82BCEA58. Body in
    // the CMassiveClientCore TU.
    CMassiveZoneManager* GetCurrentZone();

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
};

} // namespace MassiveAdClient3
