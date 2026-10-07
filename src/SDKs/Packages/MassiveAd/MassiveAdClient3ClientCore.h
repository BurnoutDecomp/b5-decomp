#pragma once

// ===========================================================================
// MassiveAdClient3::CMassiveClientCore -- the MassiveAd client root (vendor middleware).
//
// The process-wide client object the title drives: Initialize builds it (plus the
// system / request-manager / network-manager singletons), Tick runs its session state
// machine and the current zone, EnterZone / ExitZone / FlushImpressions manage the ad
// zone, and Shutdown tears everything down (blocking, or on a CMassiveThread).
//
// The client core IS a CRequestBuilder: it owns the LocateService / OpenSession /
// CloseSession requests and receives their completions through HandleResponse /
// HandleError. The inherited CMassiveBaseObject valid dword (+0x10) doubles as the client
// state:
//    0 error              2 running (zone entered)   3 suspended
//    4 shutting down      5 initialised              6 server asked for shutdown
//    7 locate in flight   8 locate wanted            9 open-session in flight
//   10 open-session wanted 11 / 12 MP create / join waiting on the session close
//   13 session open, no zone entered yet            14 zone entered (-> 2)
// (the zone manager also folds 13/15 -> 14 and writes 15; see CMassiveZoneManager).
//
// Layout (console offsets; members are accessed by name, host sizes differ):
//   +0x00  CRequestBuilder base (vftable, base object, request list, submit status)
//   +0x28  mBenchmark            bandwidth samples of the zone session
//   +0x48  mCriticalSection      "CMassiveClientCore"; every public entry TryEnter's it
//   +0x80  mTime                 client clock (server time + local tick bias)
//   +0xA8  mInternalFlags        CFlag: server configuration options (IsFlagSetInternal)
//   +0xC0  mFlags                CFlag: the title's init flags (IsFlagSet)
//   +0xD8  mnImpressionFlushInterval  ms between automatic flushes (0 = disabled)
//   +0xE0  mnTimeLastFlush
//   +0xE8  mnTimeTickLast
//   +0xF0  mnTimeTickStart
//   +0xF8  mpcSkuName / +0xFC mpcSkuVersion   (the init block's SKU strings)
//   +0x100 mnField100            zeroed by the constructor
//   +0x104 mZoneNameList         zone names the server published (char* payloads)
//   +0x114 mZoneManagerList      CMassiveZoneManager payloads
//   +0x124 mpCurrentZone
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestBuilder.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Benchmark.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Objects.h"   // CMassiveTime, CFlag

// The shutdown tick loop (a free function: it is also the shutdown thread's entry point).
unsigned long ShutdownTick(void* pTimeoutMs);

namespace MassiveAdClient3
{

class CMassiveZoneManager;

// The title's heap hooks, installed by CMassiveClientCore::SetCustomMemoryFunctions.
typedef void* (*TMassiveMallocFn)(unsigned int nSize);
typedef void  (*TMassiveFreeFn)(void* pBlock);

// Initialisation block passed by pointer to CMassiveClientCore::Initialize (the title
// builds it on the stack, zero-filled, then sets the named fields).
struct SMassiveClientInit
{
    const char*    mpcSkuName;                 // +0x00  IsInitStructValid: non-empty, <= 256 chars
    const char*    mpcSkuVersion;              // +0x04  same checks
    const void*    mpPublicKey;                // +0x08  CRequestObject::SetPublicKey
    unsigned short mnFlags;                    // +0x0C  OR-ed into the core's mFlags
    unsigned short mnPad0E;                    // +0x0E
    int            mnField10;                  // +0x10  zero-filled by the title; no client-core reader
    int            mnField14;                  // +0x14  zero-filled by the title; no client-core reader
    unsigned int   mnImpressionFlushInterval;  // +0x18  ms; 0 disables the automatic flush
    const char*    mpcThirdPartyID;            // +0x1C  CRequestObject::SetThirdPartyID
    const char*    mpcThirdPartyService;       // +0x20  CRequestObject::SetThirdPartyService
};

class CMassiveClientCore : public CRequestBuilder
{
    // CMassiveZoneManager::Tick reads the embedded benchmark counters directly.
    friend class CMassiveZoneManager;
    friend unsigned long ::ShutdownTick(void* pTimeoutMs);

public:
    // Chains CRequestBuilder("CMassiveClientCore"), constructs the members, registers
    // itself as the singleton, then validates pInit and copies its configuration: state 5
    // on success, 0 on a bad init block.
    CMassiveClientCore(const SMassiveClientInit* pInit);

    // Removes (and deletes) every zone manager and frees every zone name; the members and
    // the base destruct after.
    virtual ~CMassiveClientCore();

    // CRequestBuilder slot 1: a LocateService / OpenSession / CloseSession request completed.
    int HandleResponse(CRequestObject* pRequest) override;

    // CRequestBuilder slot 2: one of those requests failed (state -> 0).
    int HandleError(CRequestObject* pRequest, int nErrorCode) override;

    // ----- process-wide entry points (static; they act on the singleton) ----------

    // The live client core, or null.
    static CMassiveClientCore* Instance();

    // Creates the client core and its system / request / network singletons. Returns the
    // singleton (already-initialised: the existing one, with error -199).
    static CMassiveClientCore* Initialize(const SMassiveClientInit* pInit);

    // Begins shutdown: flushes, closes the session, then either ticks the shutdown out on
    // this thread (bWait) or on a CMassiveThread. Returns 1 when started, 0 otherwise.
    static int Shutdown(int bWait, unsigned int nTimeoutMs);

    // Final teardown: network / request / system singletons, the shutdown thread, the core.
    static int ShutdownComplete();

    // One client tick: session state machine, network, current zone, automatic impression
    // flush. bForce skips the minimum-interval check. Returns 1 when everything ticked.
    static int Tick(int bForce);

    // Pause / resume the current zone and the request queue (state 2 <-> 3).
    static int SuspendAll();
    static int ResumeAll();

    // Enter / exit an ad zone by name. 1 on success.
    static int EnterZone(const char* pcZoneName);
    static int ExitZone(const char* pcZoneName);

    // Report the current zone's impressions now. 1 when the zone reported.
    static int FlushImpressions();

    // printf-style client log line (formatted into a local buffer under the core lock).
    static void Log(int nLevel, const char* pcName, const char* pcFormat, ...);

    // Install the title's heap hooks (MassiveMalloc / MassiveFree).
    static void SetCustomMemoryFunctions(TMassiveMallocFn pfnMalloc, TMassiveFreeFn pfnFree);

    // Multiplayer sessions: create (returns the session GUID string, or null) / join by GUID.
    static char* MPSessionCreate();
    static int   MPSessionJoin(const char* pcGuid);

    // ----- members ----------------------------------------------------------------

    // Checks the SKU name / version strings (non-empty, <= 256 chars).
    int IsInitStructValid(const SMassiveClientInit* pInit);

    // Zone-manager list: find by zone name / create + make current / unlink + delete.
    CMassiveZoneManager* ZoneManagerFind(const char* pcZoneName);
    int                  ZoneManagerAdd(const char* pcZoneName);
    int                  ZoneManagerRemove(CMassiveZoneManager* pZoneManager);

    // Zone-name list (names the OpenSession response published).
    char* ZoneNameFind(const char* pcZoneName);
    int   ZoneNameAdd(const char* pcZoneName);
    int   ZoneNameRemove(const char* pcZoneName);

    // Flag queries on mFlags / mInternalFlags.
    int IsFlagSet(unsigned short nFlag);
    int IsFlagSetInternal(unsigned short nFlag);

    // Clock accessors.
    long long GetTime();
    long long GetTimeTickLast();
    long long GetTimeTickStart();

    // The current zone, or null.
    CMassiveZoneManager* GetCurrentZone();

    // A finished asset download's byte count + duration into the benchmark.
    void AddBenchmarkData(int nDataLength, long long nBenchmarkTime);

    // Session requests (create + submit); 0 or the failure code.
    int RequestSessionOpen();
    int RequestSessionClose();
    int RequestLocateService();

    // Acts on the pending session state at the top of Tick; non-zero on failure.
    int HandleState();

private:
    // The live client core (set by the constructor, cleared by ShutdownComplete).
    static CMassiveClientCore* spInstance;

    // The thread a non-waiting Shutdown ticks on (deleted by ShutdownComplete).
    static CMassiveThread* spShutdownThread;

    CMassiveBenchmark       mBenchmark;                 // +0x28
    CMassiveCriticalSection mCriticalSection;           // +0x48
    CMassiveTime            mTime;                      // +0x80
    CFlag                   mInternalFlags;             // +0xA8
    CFlag                   mFlags;                     // +0xC0
    long long               mnImpressionFlushInterval;  // +0xD8
    long long               mnTimeLastFlush;            // +0xE0
    long long               mnTimeTickLast;             // +0xE8
    long long               mnTimeTickStart;            // +0xF0
    const char*             mpcSkuName;                 // +0xF8
    const char*             mpcSkuVersion;              // +0xFC
    int                     mnField100;                 // +0x100
    CMassiveList            mZoneNameList;              // +0x104
    CMassiveList            mZoneManagerList;           // +0x114
    CMassiveZoneManager*    mpCurrentZone;              // +0x124
};

} // namespace MassiveAdClient3
