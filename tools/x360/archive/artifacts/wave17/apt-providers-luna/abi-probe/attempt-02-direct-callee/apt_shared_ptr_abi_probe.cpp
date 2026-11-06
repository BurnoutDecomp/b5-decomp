#include "SDKs/EATech/include/Apt/AptSharedPtr.h"
#include <xtl.h>

// The checked-in AptFilePtr type is intentionally compiled here as it currently
// exists, to show its XDK calling convention before comparing the native handle.
typedef void (__cdecl *CheckedInByValueSlot)(const char*, AptFilePtr);
typedef void (__cdecl *CheckedInByReferenceSlot)(const char*, AptFilePtr&);

extern "C" __declspec(noinline) void CheckedInCallByValue(
    const char* pName, AptFilePtr* pSource, CheckedInByValueSlot pCallback)
{
    pCallback(pName, *pSource);
}

extern "C" __declspec(noinline) void CheckedInCallByReference(
    const char* pName, AptFilePtr* pSource, CheckedInByReferenceSlot pCallback)
{
    pCallback(pName, *pSource);
}

// Exact minimal behavioral equivalent of the native one-pointer AptSharedPtr:
// copy construction owns an added reference; destruction releases it.
struct NativeAptFile { volatile LONG mnRefCount; };
extern "C" int NativeAptSharedPtrIncRef(NativeAptFile* pFile);
extern "C" int NativeAptSharedPtrDecRef(NativeAptFile* pFile);
extern "C" void NativeAptSharedPtrDelete(NativeAptFile* pFile);
extern "C" void NativeAptCompleteLoad(NativeAptFile** ppFile);

struct NativeAptFilePtr
{
    NativeAptFile* pData;
    NativeAptFilePtr() : pData(0) {}
    explicit NativeAptFilePtr(NativeAptFile* pFile) : pData(pFile) {}
    NativeAptFilePtr(const NativeAptFilePtr& other) : pData(other.pData)
    {
        if (pData) NativeAptSharedPtrIncRef(pData);
    }
    ~NativeAptFilePtr()
    {
        if (pData && NativeAptSharedPtrDecRef(pData) == 0)
            NativeAptSharedPtrDelete(pData);
    }
};

typedef void (__cdecl *NativeByValueSlot)(const char*, NativeAptFilePtr);
typedef void (__cdecl *NativeByReferenceSlot)(const char*, NativeAptFilePtr&);

extern "C" __declspec(noinline) void NativeCallByValue(
    const char* pName, NativeAptFilePtr* pSource, NativeByValueSlot pCallback)
{
    pCallback(pName, *pSource);
}

extern "C" __declspec(noinline) void NativeCallByReferenceDirect(
    const char* pName, NativeAptFilePtr* pSource, NativeByReferenceSlot pCallback)
{
    pCallback(pName, *pSource);
}

extern "C" __declspec(noinline) void NativeCallByReferenceWithCopy(
    const char* pName, NativeAptFilePtr* pSource, NativeByReferenceSlot pCallback)
{
    NativeAptFilePtr copied(*pSource);
    pCallback(pName, copied);
}

static void NativeCallbackConsumeBody(NativeAptFilePtr& handle)
{
    NativeAptFilePtr completionHandle(handle);
    NativeAptCompleteLoad(&completionHandle.pData);
    NativeAptFile* consumed = handle.pData;
    handle.pData = 0;
    if (consumed && NativeAptSharedPtrDecRef(consumed) == 0)
        NativeAptSharedPtrDelete(consumed);
}

extern "C" __declspec(noinline) void NativeCallbackByValue(
    const char* /*pName*/, NativeAptFilePtr handle)
{
    NativeCallbackConsumeBody(handle);
}

extern "C" __declspec(noinline) void NativeCallbackByReference(
    const char* /*pName*/, NativeAptFilePtr& handle)
{
    NativeCallbackConsumeBody(handle);
}

// Same ownership sequence as ARTIST LoadAnimation, kept directly in each
// callback body so /O2 can decide whether a by-value parameter dtor is redundant.
extern "C" __declspec(noinline) void NativeCallbackByValueDirectBody(
    const char* /*pName*/, NativeAptFilePtr handle)
{
    NativeAptFilePtr completionHandle(handle);
    NativeAptCompleteLoad(&completionHandle.pData);
    NativeAptFile* consumed = handle.pData;
    handle.pData = 0;
    if (consumed && NativeAptSharedPtrDecRef(consumed) == 0)
        NativeAptSharedPtrDelete(consumed);
}

extern "C" __declspec(noinline) void NativeCallbackByReferenceDirectBody(
    const char* /*pName*/, NativeAptFilePtr& handle)
{
    NativeAptFilePtr completionHandle(handle);
    NativeAptCompleteLoad(&completionHandle.pData);
    NativeAptFile* consumed = handle.pData;
    handle.pData = 0;
    if (consumed && NativeAptSharedPtrDecRef(consumed) == 0)
        NativeAptSharedPtrDelete(consumed);
}
