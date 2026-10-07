// MassiveAdClient3::CMassiveClientCore -- the MassiveAd client root (vendor middleware), plus the
// SDK's heap-hook pointers, its compiled-out log sink and its bounded string formatter.

#include "SDKs/Packages/MassiveAd/MassiveAdClient3ClientCore.h"

#include <new>      // placement new over the heap-hook allocation
#include <cstdarg>  // va_list
#include <cstdint>  // std::uintptr_t
#include <cstdio>   // vsnprintf, vsprintf_s
#include <cstdlib>  // std::malloc, std::free
#include <cstring>  // std::memset, std::strlen, std::strncpy

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Objects.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Request.h"               // gnMassiveSessionID / gnMassivePlayerID
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestManager.h"        // spRequestManager
#include "SDKs/Packages/MassiveAd/MassiveAdClient3NetworkManager.h"        // spNetworkManager
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ZoneManager.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestCloseSession.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestLocateService.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestOpenSession.h"

// Free vendor helpers (no namespace): case-insensitive compare (0 = equal) and the
// LocateService port table install (consumes and frees the list).
extern int  CompareStrings(const char* pcA, const char* pcB);
extern void MassiveSetServerPorts(MassiveAdClient3::CMassiveList* pPortPairs);

// Kernel sleep (the shutdown loop yields 10 ms per tick).
extern "C" __declspec(dllimport) void __stdcall Sleep(unsigned long dwMilliseconds);

namespace MassiveAdClient3
{

namespace
{
    // FLAG PC-platform leaf: the console initialises the two hook pointers to the CRT malloc /
    // free themselves. Their size_t parameter is wider than the hook's 32-bit one on the 64-bit
    // host, so the defaults are these two adapters over the same CRT calls.
    void* CrtMassiveMalloc(unsigned int nSize) { return std::malloc(nSize); }
    void  CrtMassiveFree(void* pBlock)          { std::free(pBlock); }

    // The well-known locate server the address table is seeded with.
    const char* const kpcLocateServerHost = "locate.madserver.net";
}

void* (*MassiveMalloc)(unsigned int nSize) = &CrtMassiveMalloc;
void  (*MassiveFree)(void* pBlock)         = &CrtMassiveFree;

// The SDK's log sink is compiled out of this build: every call lands on one empty body.
void MassiveLog(int /*nLevel*/, const char* /*pcName*/, const char* /*pcFormat*/, ...)
{
}

// The CRT sprintf_s: at most nCount bytes, NUL-terminated; returns the formatted length.
int MassiveFormatString(char* pcBuffer, unsigned int nCount, const char* pcFormat, ...)
{
    va_list lArgs;
    va_start(lArgs, pcFormat);
    const int lnLength = vsprintf_s(pcBuffer, nCount, pcFormat, lArgs);
    va_end(lArgs);
    return lnLength;
}

CMassiveClientCore* CMassiveClientCore::spInstance       = 0;
CMassiveThread*     CMassiveClientCore::spShutdownThread = 0;

CMassiveClientCore* CMassiveClientCore::Instance()
{
    return spInstance;
}

// ---------------------------------------------------------------------------
// Construction / teardown
// ---------------------------------------------------------------------------

CMassiveClientCore::CMassiveClientCore(const SMassiveClientInit* pInit)
    : CRequestBuilder("CMassiveClientCore")
    , mBenchmark()
    , mCriticalSection("CMassiveClientCore")
    , mTime()
    , mInternalFlags("CFlag")
    , mFlags("CFlag")
    , mnImpressionFlushInterval(0)
    , mnTimeLastFlush(0)
    , mnTimeTickLast(0)
    , mnTimeTickStart(0)
    , mpcSkuName(0)
    , mpcSkuVersion(0)
    , mnField100(0)
    , mZoneNameList()
    , mZoneManagerList()
    , mpCurrentZone(0)
{
    spInstance = this;
    mInternalFlags.mnFlags = 0;
    mFlags.mnFlags = 0;

    // The console hands it the init block and "game:\AdClientLog.txt"; it reads neither.
    CLog::Initialize();

    if (!IsInitStructValid(pInit))
    {
        SetValid(0);
        return;
    }

    mFlags.mnFlags |= pInit->mnFlags;
    mpcSkuName = pInit->mpcSkuName;
    mpcSkuVersion = pInit->mpcSkuVersion;
    mnImpressionFlushInterval = pInit->mnImpressionFlushInterval;
    if (pInit->mnImpressionFlushInterval == 0)
        MassiveLog(4, GetName(), "Automatic Impression Flush Disabled");

    CRequestObject::SetThirdPartyID(pInit->mpcThirdPartyID);
    CRequestObject::SetThirdPartyService(pInit->mpcThirdPartyService);
    CRequestObject::SetPublicKey(pInit->mpPublicKey);

    mTime.mnStartTime = 0;
    mTime.mnPauseBase = mTime.GetTimeLocal();
    SetValid(5);
}

CMassiveClientCore::~CMassiveClientCore()
{
    mpCurrentZone = 0;

    mZoneManagerList.GoToStart();
    while (mZoneManagerList.GetCurrent())
        ZoneManagerRemove(static_cast<CMassiveZoneManager*>(mZoneManagerList.GetCurrData()));

    mZoneNameList.GoToStart();
    while (mZoneNameList.GetCurrent())
        ZoneNameRemove(static_cast<char*>(mZoneNameList.GetCurrData()));
}

// Builds the core, then the system / request / network singletons; the network manager is
// seeded with a one-entry address list holding the locate server.
CMassiveClientCore* CMassiveClientCore::Initialize(const SMassiveClientInit* pInit)
{
    if (spInstance)
    {
        spInstance->SetLastError(-199, "");
        return spInstance;
    }

    CMassiveSystem::Initialize();
    void* lpMemory = CMassiveListNode::operator new(sizeof(CMassiveClientCore));
    spInstance = lpMemory ? ::new (lpMemory) CMassiveClientCore(pInit) : 0;

    if (!spInstance->mCriticalSection.TryEnter("CMassiveClientCore::Initialize"))
    {
        MassiveLog(5, "CMassiveClientCore", "Can not obtain lock to CMassiveClientCore::Initialize");
        return spInstance;
    }

    if (!spInstance)
    {
        MassiveLog(2, "CMassiveClientCore", "ALLOCATION Failed for CMassiveClientCore");
    }
    else if (spInstance->GetValid() == 0)
    {
        MassiveLog(2, spInstance->GetName(), "CMassiveClientCore Failed Initialization");
    }
    else
    {
        const char* lpcFailure = 0;

        void* lpPairMemory = CMassiveListNode::operator new(sizeof(CAddressIndexPair));
        CAddressIndexPair* lpLocatePair =
            lpPairMemory ? ::new (lpPairMemory) CAddressIndexPair(kpcLocateServerHost, 1) : 0;
        if (!lpLocatePair)
        {
            lpcFailure = "ALLOCATION Failed for CAddressIndexPair";
        }
        else
        {
            void* lpListMemory = CMassiveListNode::operator new(sizeof(CMassiveList));
            CMassiveList* lpAddressList = lpListMemory ? ::new (lpListMemory) CMassiveList() : 0;
            if (!lpAddressList)
            {
                lpcFailure = "ALLOCATION Failed for CAddressIndexPair List";
            }
            else
            {
                void* lpNodeMemory = CMassiveListNode::operator new(sizeof(CMassiveListNode));
                lpAddressList->Append(lpNodeMemory ? ::new (lpNodeMemory) CMassiveListNode(lpLocatePair) : 0);

                if (!CRequestManager::Initialize())
                    lpcFailure = "ALLOCATION Failed CRequestManager";
                else if (!CNetworkManager::Initialize(&spInstance->mFlags, lpAddressList))
                    lpcFailure = "ALLOCATION Failed for CNetworkManager";
            }
        }

        if (lpcFailure)
        {
            CMassiveClientCore* lpCore = spInstance;
            MassiveLog(2, lpCore->GetName(), lpcFailure);
            lpCore->SetValid(0);
        }
    }

    spInstance->mCriticalSection.Exit("CMassiveClientCore::Initialize");
    return spInstance;
}

int CMassiveClientCore::IsInitStructValid(const SMassiveClientInit* pInit)
{
    int lnError;
    if (!pInit)
    {
        lnError = SetLastError(-198, "InitStruct: NULL\n");
    }
    else if (!pInit->mpcSkuName || !IsValidString(pInit->mpcSkuName) ||
             std::strlen(pInit->mpcSkuName) > 0x100)
    {
        lnError = SetLastError(-197, "InitStruct: Invalid SKU\n");
    }
    else if (!pInit->mpcSkuVersion || !IsValidString(pInit->mpcSkuVersion) ||
             std::strlen(pInit->mpcSkuVersion) > 0x100)
    {
        lnError = SetLastError(-196, "InitStruct: Invalid SKU Version\n");
    }
    else
    {
        return 1;
    }
    return lnError == 0;
}

void CMassiveClientCore::SetCustomMemoryFunctions(TMassiveMallocFn pfnMalloc, TMassiveFreeFn pfnFree)
{
    MassiveMalloc = pfnMalloc;
    MassiveFree = pfnFree;
}

int CMassiveClientCore::ShutdownComplete()
{
    if (!spInstance)
        return 0;

    CNetworkManager::Shutdown();
    CRequestManager::Shutdown();
    const int lnResult = CMassiveSystem::Shutdown();

    if (spShutdownThread)
    {
        delete spShutdownThread;
        spShutdownThread = 0;
    }
    if (spInstance)
    {
        delete spInstance;
        spInstance = 0;
    }
    return lnResult;
}

// Starts the shutdown: unless the server ordered an immediate stop (internal option bit 0) it
// flushes and closes the session first, then either runs the shutdown tick loop here (bWait)
// or hands it to a CMassiveThread.
int CMassiveClientCore::Shutdown(int bWait, unsigned int nTimeoutMs)
{
    if (!spInstance)
        return 0;

    if (!spInstance->mCriticalSection.TryEnter("CMassiveClientCore::Shutdown"))
    {
        MassiveLog(5, "CMassiveClientCore", "Can not obtain lock to CMassiveClientCore::Shutdown");
        return 0;
    }

    if (spInstance->GetValid() == 4)
    {
        spInstance->SetLastError(-200, "Shutdown allready in progress");
        spInstance->mCriticalSection.Exit("CMassiveClientCore::Shutdown");
        return 0;
    }

    CNetworkManager::KillAndWaitForDNS(30);
    const int lbServerShutdown = spInstance->mInternalFlags.mnFlags & 1;
    MassiveLog(4, spInstance->GetName(), "Shutdown AdClient Library Begin.  Wait: %d Timeout: %d",
               bWait, nTimeoutMs);
    spInstance->SetValid(4);

    if (!lbServerShutdown)
    {
        FlushImpressions();
        spInstance->mnLastError = spInstance->RequestSessionClose();
        if (spInstance->mnLastError)
            spInstance->SetLastError(spInstance->mnLastError, "");
    }

    if (spRequestManager)
        spRequestManager->PrepareForShutdown();
    if (spNetworkManager)
        spNetworkManager->PrepareForShutdown();

    if (lbServerShutdown)
    {
        spInstance->mCriticalSection.Exit("CMassiveClientCore::Shutdown");
        return ShutdownComplete();
    }

    void* lpTimeoutParam = reinterpret_cast<void*>(static_cast<std::uintptr_t>(nTimeoutMs));
    if (bWait)
    {
        ShutdownTick(lpTimeoutParam);
    }
    else
    {
        void* lpMemory = CMassiveListNode::operator new(sizeof(CMassiveThread));
        spShutdownThread = lpMemory ? ::new (lpMemory) CMassiveThread() : 0;
        if (!spShutdownThread)
        {
            CMassiveClientCore* lpCore = spInstance;
            MassiveLog(2, lpCore->GetName(), "ALLOCATION Failed for CMassiveThread Shutdown");
            lpCore->SetLastError(-99, "");
            spInstance->mCriticalSection.Exit("CMassiveClientCore::Shutdown");
            return 0;
        }
        spShutdownThread->Create(&ShutdownTick, lpTimeoutParam);
        spInstance->mCriticalSection.Exit("CMassiveClientCore::Shutdown");
    }
    return 1;
}

// ---------------------------------------------------------------------------
// Per-frame tick and the session state machine
// ---------------------------------------------------------------------------

int CMassiveClientCore::Tick(int bForce)
{
    if (!spInstance)
        return 0;

    if (!spInstance->mCriticalSection.TryEnter("CMassiveClientCore::Tick"))
    {
        MassiveLog(5, "CMassiveClientCore", "Can not obtain lock to CMassiveClientCore::Tick");
        return 0;
    }

    spInstance->mnLastError = 0;
    if (spInstance->GetValid() == 0)
    {
        spInstance->mCriticalSection.Exit("CMassiveClientCore::Tick");
        return 0;
    }

    if (spInstance->HandleState() != 0)
    {
        // HandleState may have shut the client down underneath us.
        if (spInstance)
            spInstance->mCriticalSection.Exit("CMassiveClientCore::Tick");
        return 0;
    }

    spInstance->mnTimeTickStart = spInstance->mTime.GetTime();
    const unsigned long long luElapsed =
        static_cast<unsigned long long>(spInstance->mnTimeTickStart - spInstance->mnTimeTickLast);

    spInstance->mInternalFlags.mnFlags &= 0xFBFF;
    if (luElapsed > 1000)
    {
        spInstance->mInternalFlags.mnFlags |= 0x400;
        MassiveLog(3, spInstance->GetName(), "MAX Time between Ticks");
    }

    if (!bForce && luElapsed <= 1)
    {
        MassiveLog(3, spInstance->GetName(), "Minimum Time between Ticks not reached: %d", luElapsed);
        spInstance->mCriticalSection.Exit("CMassiveClientCore::Tick");
        return 0;
    }

    spNetworkManager->Tick();
    int lbNetworkTicked = 1;
    if (spNetworkManager->mnLastError)
    {
        lbNetworkTicked = 0;
        spInstance->SetLastError(spNetworkManager->mnLastError, "");
    }

    int lbZoneTicked = 1;
    CMassiveZoneManager* lpZone = spInstance->mpCurrentZone;
    if (lpZone)
    {
        if (lpZone->mbIsValid == 0)
        {
            spInstance->SetLastError(lpZone->mnLastError, "");
            lbZoneTicked = 0;
            spInstance->mpCurrentZone = 0;
            spInstance->SetValid(gnMassiveSessionID ? 13 : 0);
        }
        else
        {
            lbZoneTicked = lpZone->Tick();
        }
    }

    if (spInstance->mnImpressionFlushInterval != 0)
    {
        const unsigned long long luSinceFlush =
            static_cast<unsigned long long>(spInstance->mTime.GetTime() - spInstance->mnTimeLastFlush);
        if (luSinceFlush > static_cast<unsigned long long>(spInstance->mnImpressionFlushInterval) &&
            spInstance->GetValid() != 4)
        {
            const int lbFlushed = FlushImpressions();
            CMassiveClientCore* lpCore = spInstance;
            if (!lbFlushed)
                MassiveLog(7, lpCore->GetName(), "Flush Impressions Failed");
            spInstance->mnTimeLastFlush = lpCore->mTime.GetTime();
        }
    }

    spInstance->mnTimeTickLast = spInstance->mnTimeTickStart;
    spInstance->mnTimeTickStart = 0;
    spInstance->mCriticalSection.Exit("CMassiveClientCore::Tick");

    return (lbNetworkTicked && lbZoneTicked) ? 1 : 0;
}

int CMassiveClientCore::HandleState()
{
    int lnResult = 0;
    char lacMessage[64];
    std::memset(lacMessage, 0, sizeof(lacMessage));

    const char* lpcFormat;
    switch (GetValid())
    {
        case 5:
        case 8:
            lnResult = RequestLocateService();
            SetValid(7);
            if (!lnResult)
                return lnResult;
            lpcFormat = "Failed to Locate Service: %d";
            break;

        case 6:
            Shutdown(0, 0);
            return -200;

        case 10:
            lnResult = RequestSessionOpen();
            SetValid(9);
            if (!lnResult)
                return lnResult;
            lpcFormat = "Open Session Failed: %d";
            break;

        case 14:
            SetValid(2);
            return lnResult;

        default:
            return lnResult;
    }

    MassiveFormatString(lacMessage, sizeof(lacMessage), lpcFormat, lnResult);
    SetLastError(lnResult, "");
    MassiveLog(2, GetName(), lacMessage);
    SetValid(0);
    return lnResult;
}

int CMassiveClientCore::SuspendAll()
{
    if (!spInstance || !spInstance->mCriticalSection.TryEnter("CMassiveClientCore::SuspendAll"))
        return 0;

    if (spInstance->GetValid() != 2 ||
        (spInstance->mpCurrentZone && !spInstance->mpCurrentZone->Suspend()))
    {
        spInstance->mCriticalSection.Exit("CMassiveClientCore::SuspendAll");
        return 0;
    }

    int lbSuspended = 0;
    if (spRequestManager->SuspendAll() == 0)
    {
        lbSuspended = 1;
        spInstance->SetValid(3);
    }
    spInstance->mCriticalSection.Exit("CMassiveClientCore::SuspendAll");
    return lbSuspended;
}

int CMassiveClientCore::ResumeAll()
{
    if (!spInstance || !spInstance->mCriticalSection.TryEnter("CMassiveClientCore::ResumeAll"))
        return 0;

    if (spInstance->GetValid() != 3 ||
        (spInstance->mpCurrentZone && !spInstance->mpCurrentZone->Resume()))
    {
        spInstance->mCriticalSection.Exit("CMassiveClientCore::ResumeAll");
        return 0;
    }

    int lbResumed = 0;
    if (spRequestManager->ResumeAll() == 0)
    {
        lbResumed = 1;
        spInstance->SetValid(2);
    }
    spInstance->mCriticalSection.Exit("CMassiveClientCore::ResumeAll");
    return lbResumed;
}

void CMassiveClientCore::Log(int /*nLevel*/, const char* /*pcName*/, const char* pcFormat, ...)
{
    if (spInstance && spInstance->mCriticalSection.TryEnter("CMassiveClientCore::Log"))
    {
        char lacLine[512];
        va_list lArgs;
        va_start(lArgs, pcFormat);
        vsnprintf(lacLine, sizeof(lacLine), pcFormat, lArgs);
        va_end(lArgs);
        spInstance->mCriticalSection.Exit("CMassiveClientCore::Log");
    }
}

// ---------------------------------------------------------------------------
// Zones
// ---------------------------------------------------------------------------

int CMassiveClientCore::EnterZone(const char* pcZoneName)
{
    if (!spInstance)
        return 0;

    if (!spInstance->mCriticalSection.TryEnter("CMassiveClientCore::EnterZone"))
    {
        MassiveLog(5, "CMassiveClientCore", "Can not obtain lock to CMassiveClientCore::EnterZone");
        return 0;
    }

    if (!IsValidString(pcZoneName))
    {
        spInstance->SetLastError(-300, "");
        spInstance->SetValid(0);
        spInstance->mCriticalSection.Exit("CMassiveClientCore::EnterZone");
        return 0;
    }

    int lbEntered;
    if (spInstance->mZoneNameList.GetCount() != 0 && !spInstance->ZoneNameFind(pcZoneName))
    {
        spInstance->SetLastError(-300, "Cannot Find Zone in List: %s", pcZoneName);
        lbEntered = 0;
    }
    else
    {
        CMassiveZoneManager* lpZone = spInstance->ZoneManagerFind(pcZoneName);
        if (lpZone && lpZone->mbIsValid != 16)
        {
            spInstance->mpCurrentZone = lpZone;
            lpZone->Resume();
        }
        else
        {
            spInstance->ZoneManagerAdd(pcZoneName);
            if (spInstance->GetValid() == 5)
                spInstance->SetValid(8);
            if (!gnMassiveSessionID && spInstance->GetValid() == 13)
                spInstance->SetValid(10);
        }
        lbEntered = 1;
    }

    spInstance->mCriticalSection.Exit("CMassiveClientCore::EnterZone");
    return lbEntered;
}

int CMassiveClientCore::ExitZone(const char* pcZoneName)
{
    if (!spInstance)
        return 0;

    if (!spInstance->mCriticalSection.TryEnter("CMassiveClientCore::ExitZone"))
    {
        MassiveLog(5, "CMassiveClientCore", "Can not obtain lock to CMassiveClientCore::ExitZone");
        return 0;
    }

    if (!IsValidString(pcZoneName))
    {
        spInstance->SetLastError(-300, "");
        spInstance->mCriticalSection.Exit("CMassiveClientCore::ExitZone");
        return 0;
    }

    FlushImpressions();

    int lbExited = 0;
    CMassiveZoneManager* lpZone = spInstance->ZoneManagerFind(pcZoneName);
    if (lpZone && lpZone->mbIsValid != 0)
    {
        lpZone->mbIsValid = 17;
        lbExited = lpZone->Tick();
        spInstance->mBenchmark.Reset();
    }
    else
    {
        CMassiveClientCore* lpCore = spInstance;
        if (lpZone)
            MassiveLog(2, lpCore->GetName(), "Exit Zone Failed, Zone is in an Error State");
        lpCore->SetLastError(-297, "");
    }

    spInstance->mCriticalSection.Exit("CMassiveClientCore::ExitZone");
    return lbExited;
}

int CMassiveClientCore::FlushImpressions()
{
    if (!spInstance)
        return 0;

    if (!spInstance->mCriticalSection.TryEnter("CMassiveClientCore::FlushImpressions"))
    {
        MassiveLog(5, "CMassiveClientCore", "Can not obtain lock to CMassiveClientCore::FlushImpressions");
        return 0;
    }

    CMassiveClientCore* lpCore = spInstance;
    if (lpCore->mInternalFlags.mnFlags & 4)
    {
        MassiveLog(4, lpCore->GetName(), "Server Configuration: Impressions Disabled");
        lpCore->mCriticalSection.Exit("CMassiveClientCore::FlushImpressions");
        return 0;
    }

    const int lnState = lpCore->GetValid();
    if (lnState != 2 && lnState != 4)
    {
        MassiveLog(3, lpCore->GetName(), "Failed Impression Flush: Invalid Client State: %d", lnState);
        spInstance->mCriticalSection.Exit("CMassiveClientCore::FlushImpressions");
        return 0;
    }

    if (!lpCore->mpCurrentZone)
    {
        MassiveLog(2, lpCore->GetName(), "Failed Impression Flush: Invalid Zone");
        lpCore->mCriticalSection.Exit("CMassiveClientCore::FlushImpressions");
        return 0;
    }

    CMassiveZoneManager* lpZone = lpCore->mpCurrentZone;
    int lbFlushed;
    if (lpZone->mbIsValid == 2)
    {
        MassiveLog(4, lpCore->GetName(), "Flushing Impressions");
        lpZone->ReportImpressions();
        lbFlushed = 1;
    }
    else
    {
        MassiveLog(2, lpCore->GetName(), "Failed Impression Flush: Invalid Zone State: %d", lpZone->mbIsValid);
        lbFlushed = 0;
    }

    spInstance->mCriticalSection.Exit("CMassiveClientCore::FlushImpressions");
    return lbFlushed;
}

CMassiveZoneManager* CMassiveClientCore::ZoneManagerFind(const char* pcZoneName)
{
    if (!IsValidString(pcZoneName))
    {
        SetLastError(-300, "");
        return 0;
    }

    std::strlen(pcZoneName);  // result unused
    mZoneManagerList.GoToStart();
    while (mZoneManagerList.GetCurrent())
    {
        CMassiveZoneManager* lpZone = static_cast<CMassiveZoneManager*>(mZoneManagerList.GetCurrData());
        if (CompareStrings(pcZoneName, lpZone->mpcZoneName) == 0)
            return static_cast<CMassiveZoneManager*>(mZoneManagerList.GetCurrData());
        mZoneManagerList.GoToNext();
    }
    return 0;
}

int CMassiveClientCore::ZoneManagerAdd(const char* pcZoneName)
{
    if (!IsValidString(pcZoneName))
    {
        SetLastError(-300, "");
        return -300;
    }

    void* lpMemory = CMassiveListNode::operator new(sizeof(CMassiveZoneManager));
    CMassiveZoneManager* lpZone = lpMemory ? ::new (lpMemory) CMassiveZoneManager(pcZoneName) : 0;
    if (!lpZone)
    {
        MassiveLog(2, spInstance->GetName(), "ALLOCATION Failed for CMassiveZoneManager.  Zone: %s", pcZoneName);
        spInstance->SetValid(0);
        return -99;
    }

    void* lpNodeMemory = CMassiveListNode::operator new(sizeof(CMassiveListNode));
    mZoneManagerList.Append(lpNodeMemory ? ::new (lpNodeMemory) CMassiveListNode(lpZone) : 0);
    mpCurrentZone = lpZone;
    return 0;
}

int CMassiveClientCore::ZoneManagerRemove(CMassiveZoneManager* pZoneManager)
{
    if (pZoneManager)
    {
        if (pZoneManager == mpCurrentZone)
            mpCurrentZone = 0;

        mZoneManagerList.GoToStart();
        while (mZoneManagerList.GetCurrent())
        {
            if (mZoneManagerList.GetCurrData() == pZoneManager)
            {
                mZoneManagerList.Remove(mZoneManagerList.Find(mZoneManagerList.GetCurrData()), 1);
                delete pZoneManager;
                return 0;
            }
            mZoneManagerList.GoToNext();
        }
    }
    return SetLastError(-300, "");
}

char* CMassiveClientCore::ZoneNameFind(const char* pcZoneName)
{
    if (!pcZoneName)
        return 0;

    std::strlen(pcZoneName);  // result unused
    mZoneNameList.GoToStart();
    while (mZoneNameList.GetCurrent())
    {
        if (CompareStrings(pcZoneName, static_cast<char*>(mZoneNameList.GetCurrData())) == 0)
            return static_cast<char*>(mZoneNameList.GetCurrData());
        mZoneNameList.GoToNext();
    }
    return 0;
}

int CMassiveClientCore::ZoneNameAdd(const char* pcZoneName)
{
    if (ZoneNameFind(pcZoneName))
        return 0;

    const unsigned int luSize = static_cast<unsigned int>(std::strlen(pcZoneName)) + 1;
    char* lpcName = static_cast<char*>(MassiveMalloc(luSize));
    if (!lpcName)
    {
        MassiveLog(2, 0, "ALLOCATION Failed for pName");
        SetLastError(-99, "");
        SetValid(0);
        return 0;
    }
    std::strncpy(lpcName, pcZoneName, luSize);

    void* lpNodeMemory = CMassiveListNode::operator new(sizeof(CMassiveListNode));
    mZoneNameList.Append(lpNodeMemory ? ::new (lpNodeMemory) CMassiveListNode(lpcName) : 0);
    return 1;
}

int CMassiveClientCore::ZoneNameRemove(const char* pcZoneName)
{
    std::strlen(pcZoneName);  // result unused
    mZoneNameList.GoToStart();
    while (mZoneNameList.GetCurrent())
    {
        if (CompareStrings(pcZoneName, static_cast<char*>(mZoneNameList.GetCurrData())) == 0)
        {
            MassiveFree(mZoneNameList.GetCurrData());
            mZoneNameList.Remove(mZoneNameList.GetCurrent(), 1);
            return 1;
        }
        mZoneNameList.GoToNext();
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

int CMassiveClientCore::IsFlagSet(unsigned short nFlag)
{
    return (mFlags.mnFlags & nFlag) != 0;
}

int CMassiveClientCore::IsFlagSetInternal(unsigned short nFlag)
{
    return (mInternalFlags.mnFlags & nFlag) != 0;
}

long long CMassiveClientCore::GetTime()
{
    return mTime.GetTime();
}

long long CMassiveClientCore::GetTimeTickLast()
{
    return mnTimeTickLast;
}

long long CMassiveClientCore::GetTimeTickStart()
{
    return mnTimeTickStart;
}

CMassiveZoneManager* CMassiveClientCore::GetCurrentZone()
{
    return mpCurrentZone;
}

void CMassiveClientCore::AddBenchmarkData(int nDataLength, long long nBenchmarkTime)
{
    mBenchmark.AddData(nDataLength, static_cast<int>(nBenchmarkTime));
}

// ---------------------------------------------------------------------------
// Session requests and their completions
// ---------------------------------------------------------------------------

int CMassiveClientCore::RequestLocateService()
{
    void* lpMemory = CMassiveListNode::operator new(sizeof(CRequestLocateService));
    CRequestLocateService* lpRequest = lpMemory ? ::new (lpMemory) CRequestLocateService() : 0;
    if (!lpRequest)
    {
        CMassiveClientCore* lpCore = spInstance;
        MassiveLog(2, lpCore->GetName(), "ALLOCATION Failed for CRequestLocateService");
        lpCore->SetValid(0);
        return spInstance->SetLastError(-99, "");
    }

    int lnResult = lpRequest->CreateRequest(this, mpcSkuName, mpcSkuVersion);
    if (lnResult)
        return SetLastError(lnResult, "Failed To Create LocateService Request: %d", lnResult);

    lnResult = lpRequest->Submit();
    if (lnResult)
        return SetLastError(lnResult, "Failed To Submit LocateService Request: %d", lnResult);

    SetValid(8);
    return 0;
}

int CMassiveClientCore::RequestSessionOpen()
{
    if (gnMassiveSessionID)
    {
        const int lnCloseResult = RequestSessionClose();
        if (lnCloseResult)
            return SetLastError(lnCloseResult, "");
    }

    int lnSessionType = 1;
    if (CMassiveSystem::Instance()->mpcGuid)
    {
        MassiveLog(4, spInstance->GetName(), "MP Create Session: %s", CMassiveSystem::Instance()->mpcGuid);
        lnSessionType = 2;
    }

    void* lpMemory = CMassiveListNode::operator new(sizeof(CRequestOpenSession));
    CRequestOpenSession* lpRequest = lpMemory ? ::new (lpMemory) CRequestOpenSession() : 0;
    if (!lpRequest)
    {
        CMassiveClientCore* lpCore = spInstance;
        MassiveLog(2, lpCore->GetName(), "ALLOCATION Failed for CRequestOpenSession");
        lpCore->SetValid(0);
        return spInstance->SetLastError(-99, "");
    }

    int lnResult = lpRequest->CreateRequest(this, mpcSkuName, mpcSkuVersion, lnSessionType,
                                            CMassiveSystem::Instance()->mpcGuid, 1);
    if (lnResult)
        return SetLastError(lnResult, "Failed To Create OpenSession Request: %d", lnResult);

    lnResult = lpRequest->Submit();
    if (lnResult)
        return SetLastError(lnResult, "Failed To Submit OpenSession Request: %d", lnResult);

    return 0;
}

int CMassiveClientCore::RequestSessionClose()
{
    if (!gnMassiveSessionID)
        return spInstance->SetLastError(-188, "No Session to Close");

    void* lpMemory = CMassiveListNode::operator new(sizeof(CRequestCloseSession));
    CRequestCloseSession* lpRequest = lpMemory ? ::new (lpMemory) CRequestCloseSession() : 0;
    if (!lpRequest)
    {
        CMassiveClientCore* lpCore = spInstance;
        MassiveLog(2, lpCore->GetName(), "ALLOCATION Failed for CRequestCloseSession");
        lpCore->SetValid(0);
        return spInstance->SetLastError(-99, "");
    }

    int lnResult = lpRequest->CreateRequest(this);
    if (lnResult)
        return SetLastError(lnResult, "Failed To Create CloseSession Request: %d", lnResult);

    lnResult = lpRequest->Submit();
    if (lnResult)
        return SetLastError(lnResult, "Failed To Submit CloseSession Request: %d", lnResult);

    return 0;
}

// Request types: 17 LocateService, 35 OpenSession, 115 CloseSession.
int CMassiveClientCore::HandleResponse(CRequestObject* pRequest)
{
    const int lnType = pRequest->GetServerType();

    if (lnType == 17)
    {
        CRequestLocateService* lpLocate = static_cast<CRequestLocateService*>(pRequest);
        const char* lpcFailure = 0;

        void* lpMemory = CMassiveListNode::operator new(sizeof(CMassiveList));
        CMassiveList* lpAddressPairs = lpMemory ? ::new (lpMemory) CMassiveList() : 0;
        if (!lpAddressPairs)
        {
            RemoveFromRequestCollect(pRequest);
            lpcFailure = "ALLOCATION Failed for CAddressIndexPair List";
        }
        else
        {
            lpLocate->GetResponseAddressIndexPairs(lpAddressPairs);

            lpMemory = CMassiveListNode::operator new(sizeof(CMassiveList));
            CMassiveList* lpPortPairs = lpMemory ? ::new (lpMemory) CMassiveList() : 0;
            if (!lpPortPairs)
            {
                RemoveFromRequestCollect(pRequest);
                lpcFailure = "ALLOCATION Failed for CPortIndexPair List";
            }
            else
            {
                lpLocate->GetResponsePortIndexPairs(lpPortPairs);
                MassiveSetServerPorts(lpPortPairs);

                // The address pairs are released unused.
                lpAddressPairs->GoToStart();
                while (lpAddressPairs->GetCurrent())
                {
                    CAddressIndexPair* lpPair = static_cast<CAddressIndexPair*>(lpAddressPairs->GetCurrData());
                    if (lpPair)
                        delete lpPair;
                    lpAddressPairs->GoToNext();
                }
                lpAddressPairs->RemoveAll();
                lpAddressPairs->~CMassiveList();
                CMassiveBaseObject::operator delete(lpAddressPairs);
            }
        }

        if (lpcFailure)
        {
            CMassiveClientCore* lpCore = spInstance;
            MassiveLog(2, lpCore->GetName(), lpcFailure);
            lpCore->SetValid(0);
            return spInstance->SetLastError(-99, "");
        }

        if (lpLocate->mbHasHeartbeatPeriod)
            spNetworkManager->mnHeartbeatInterval = lpLocate->mbHeartbeatPeriod * 60000;

        mTime.mnStartTime = lpLocate->mnServerTime;
        mTime.mnPauseBase = mTime.GetTimeLocal();
        mnTimeLastFlush = mTime.GetTime();
        mnTimeTickLast = mnTimeLastFlush;

        if (lpLocate->mbHasAutoReportingTime)
        {
            mnImpressionFlushInterval = lpLocate->mbAutoReportingTime * 60000;
            MassiveLog(4, spInstance->GetName(), "Server Configuration: Impression Flush: %I64d",
                       mnImpressionFlushInterval);
        }

        mInternalFlags.mnFlags |= lpLocate->mnServerConfigOptions;
        if (mInternalFlags.mnFlags & 1)
        {
            MassiveLog(4, GetName(), "Server Configuration: Shutdown Client");
            SetValid(6);
            return 0;
        }
        if (mInternalFlags.mnFlags & 4)
            spInstance->mnImpressionFlushInterval = 0;
        SetValid(10);
    }
    else if (lnType == 35)
    {
        CRequestOpenSession* lpOpen = static_cast<CRequestOpenSession*>(pRequest);
        gnMassivePlayerID = lpOpen->mnMassivePlayerID;
        gnMassiveSessionID = lpOpen->mnMassiveSessionID;
        SetValid(13);
        MassiveLog(4, GetName(), "Connected to Server %s",
                   CMassiveSystem::Instance()->mpcGuid ? "Multiplayer" : "Singleplayer");
    }
    else if (lnType == 115)
    {
        gnMassiveSessionID = 0;
        if (GetValid() == 11)
            MPSessionCreate();
        else if (GetValid() == 12)
            MPSessionJoin(CMassiveSystem::Instance()->mpcGuid);
    }

    return RemoveFromRequestCollect(pRequest);
}

int CMassiveClientCore::HandleError(CRequestObject* pRequest, int /*nErrorCode*/)
{
    int lnError;
    switch (pRequest->GetServerType())
    {
        case 17:
            MassiveLog(2, GetName(), "Failed To Locate Service with Server!");
            lnError = -190;
            break;
        case 35:
            MassiveLog(2, GetName(), "Failed To Open Session with Server!");
            lnError = -189;
            break;
        case 115:
            MassiveLog(2, GetName(), "Failed To Close Session with Server!");
            lnError = -188;
            break;
        default:
            return RemoveFromRequestCollect(pRequest);
    }
    SetLastError(lnError, "");
    SetValid(0);
    return RemoveFromRequestCollect(pRequest);
}

// ---------------------------------------------------------------------------
// Multiplayer sessions
// ---------------------------------------------------------------------------

char* CMassiveClientCore::MPSessionCreate()
{
    if (!spInstance)
        return 0;

    if (!spInstance->mCriticalSection.TryEnter("CMassiveClientCore::MPSessionCreate"))
    {
        MassiveLog(5, "CMassiveClientCore", "Can not obtain lock to CMassiveClientCore::MPSessionCreate");
        return 0;
    }

    if (!CMassiveSystem::Instance()->mpcGuid)
        CMassiveSystem::CreateMassiveGuid(&CMassiveSystem::Instance()->mpcGuid);

    if (gnMassiveSessionID)
    {
        const int lnCloseResult = spInstance->RequestSessionClose();
        if (lnCloseResult)
        {
            spInstance->SetLastError(lnCloseResult, "");
            spInstance->mCriticalSection.Exit("CMassiveClientCore::MPSessionCreate");
            return 0;
        }
        spInstance->SetValid(11);
    }
    else
    {
        const int lnState = spInstance ? spInstance->GetValid() : 0;
        if (lnState == 5)
            spInstance->SetValid(8);
        else if (lnState <= 6 || lnState > 8)
            spInstance->SetValid(10);
    }

    spInstance->mCriticalSection.Exit("CMassiveClientCore::MPSessionCreate");
    if (!CMassiveSystem::Instance())
        return 0;
    return CMassiveSystem::Instance()->mpcGuid;
}

int CMassiveClientCore::MPSessionJoin(const char* pcGuid)
{
    if (!spInstance)
        return 0;

    if (!spInstance->mCriticalSection.TryEnter("CMassiveClientCore::MPSessionJoin"))
    {
        MassiveLog(5, "CMassiveClientCore", "Can not obtain lock to CMassiveClientCore::MPSessionJoin");
        return 0;
    }

    if (!IsValidString(pcGuid))
    {
        spInstance->SetLastError(-194, "");
        spInstance->mCriticalSection.Exit("CMassiveClientCore::MPSessionJoin");
        return 0;
    }

    CMassiveSystem::Instance()->SetGuid(pcGuid);

    int lbJoined;
    if (gnMassiveSessionID)
    {
        const int lnCloseResult = spInstance->RequestSessionClose();
        if (lnCloseResult)
        {
            spInstance->SetLastError(lnCloseResult, "");
            lbJoined = 0;
        }
        else
        {
            lbJoined = 1;
            spInstance->SetValid(12);
        }
    }
    else
    {
        const int lnState = spInstance ? spInstance->GetValid() : 0;
        if (lnState == 5)
            spInstance->SetValid(8);
        else if (lnState <= 6 || lnState > 8)
            spInstance->SetValid(10);
        lbJoined = 1;
    }

    spInstance->mCriticalSection.Exit("CMassiveClientCore::MPSessionJoin");
    return lbJoined;
}

} // namespace MassiveAdClient3

// Ticks the shutting-down client until its session closes, it leaves the shutdown state, the
// server orders an abort (internal option 0x800) or the timeout passes (nTimeoutMs when set,
// 10 s at most), then completes the shutdown.
unsigned long ShutdownTick(void* pTimeoutMs)
{
    using namespace MassiveAdClient3;

    const unsigned int lnTimeoutMs = static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(pTimeoutMs));
    CMassiveClientCore* lpCore = CMassiveClientCore::spInstance;
    if (!lpCore)
        return 0;

    lpCore->mCriticalSection.Enter("CMassiveClientCore::Initialize");
    const unsigned int lnStartTime = static_cast<unsigned int>(lpCore->mTime.GetTime());
    while (gnMassiveSessionID)
    {
        if (!CMassiveClientCore::Tick(0))
        {
            MassiveLog(2, lpCore->GetName(), "Shutdown Tick failed. Aborting.");
            break;
        }
        if (lpCore->GetValid() != 4 || lpCore->IsFlagSetInternal(0x800))
        {
            MassiveLog(4, lpCore->GetName(), "Shutdown Tick Aborted");
            break;
        }

        Sleep(10);

        const unsigned int lnElapsed = static_cast<unsigned int>(lpCore->mTime.GetTime()) - lnStartTime;
        if ((lnTimeoutMs && lnElapsed > lnTimeoutMs) || lnElapsed > 10000)
        {
            lpCore->SetLastError(-186, "Shutdown Time out at: %d ms", lnElapsed);
            break;
        }
    }
    lpCore->mCriticalSection.Exit("CMassiveClientCore::Initialize");
    return CMassiveClientCore::ShutdownComplete();
}
