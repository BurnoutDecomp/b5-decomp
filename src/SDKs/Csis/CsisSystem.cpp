#include "SDKs/Csis/CsisSystem.h"
#include "SDKs/Csis/CsisFunctionHandle.h"
#include "SDKs/Csis/CsisGlobalVariableHandle.h"
#include "coreallocator/icoreallocator_interface.h"
#include <xtl.h>
#include <cstring>

namespace
{
    HANDLE ghCsisMutex = 0;                         // 8324E8FC
    unsigned char gbCsisAllocatorSet = 0;            // 8324E900
    unsigned char gbCsisInited = 0;                  // 8324E901
    EA::Allocator::ICoreAllocator* gpCsisAllocator = 0; // 8324E904
    short gwCsisSerial = 0;                         // 8324E908
    Csis::SystemContentNode* gpCsisListHead = 0;     // 8324E90C
    short NextSerial(short serial)
    {
        short next = static_cast<short>(serial + 1);
        if (next < 0) { next = 1; gwCsisSerial = 1; }
        return next;
    }
}
namespace Csis
{
int System::SetAllocator(EA::Allocator::ICoreAllocator* allocator)
{
    if (gbCsisInited) return RESULT_ERR_ILLEGALOP;
    gpCsisAllocator = allocator;
    gbCsisAllocatorSet = 1;
    return RESULT_OK;
}
// ARTIST CreateInstanceFast uses ICoreAllocator::Alloc's three-argument slot +8.
void* System::Allocate(size_t size, const char* name, u32 flags)
{
    return gpCsisAllocator->Alloc(size, name, flags);
}
void System::Free(void* block)
{
    if (gpCsisAllocator) gpCsisAllocator->Free(block, 0);
}
int System::Init()
{
    ghCsisMutex = ::CreateMutexA(0, FALSE, 0);
    gpCsisListHead = 0;
    gbCsisInited = 1;
    return 0;
}
int System::IsInited() { return gbCsisInited; }
int System::Lock() { ::WaitForSingleObject(ghCsisMutex, 0xFFFFFFFFu); return 0; }
int System::Unlock() { ::ReleaseMutex(ghCsisMutex); return 0; }

// ARTIST 82B0F2A8: native header +28hex followed by 12/12/16-byte records.
int System::Subscribe(SystemContent* content)
{
    content->pFunctionDesc = reinterpret_cast<CsisDef::FunctionDesc*>(content + 1);
    content->pClassDesc = content->pFunctionDesc + content->numFunctions;
    content->pGlobalVariableDesc = reinterpret_cast<CsisDef::GlobalVariableDesc*>(
        content->pClassDesc + content->numClasses);
    short serial = gwCsisSerial;
    for (u32 i = 0; i < content->numFunctions; ++i)
    {
        CsisDef::FunctionDesc& desc = content->pFunctionDesc[i];
        desc.pStringId = reinterpret_cast<const char*>(reinterpret_cast<uintptr_t>(content) + reinterpret_cast<uintptr_t>(desc.pStringId));
        serial = NextSerial(serial);
        desc.u.crcAndKey.key = serial;
        if (i + 1 == content->numFunctions) gwCsisSerial = serial;
    }
    for (u32 i = 0; i < content->numClasses; ++i)
    {
        CsisDef::ClassDesc& desc = content->pClassDesc[i];
        desc.pStringId = reinterpret_cast<const char*>(reinterpret_cast<uintptr_t>(content) + reinterpret_cast<uintptr_t>(desc.pStringId));
        serial = NextSerial(serial);
        desc.u.crcAndKey.key = serial;
        if (i + 1 == content->numClasses) gwCsisSerial = serial;
    }
    for (u32 i = 0; i < content->numGlobalVariables; ++i)
    {
        CsisDef::GlobalVariableDesc& desc = content->pGlobalVariableDesc[i];
        desc.pStringId = reinterpret_cast<const char*>(reinterpret_cast<uintptr_t>(content) + reinterpret_cast<uintptr_t>(desc.pStringId));
        serial = NextSerial(serial);
        desc.u.crcAndKey.key = serial;
        if (i + 1 == content->numGlobalVariables) gwCsisSerial = serial;
    }
    content->linkNode.next = gpCsisListHead;
    content->linkNode.prev = 0;
    if (gpCsisListHead) gpCsisListHead->prev = &content->linkNode;
    gpCsisListHead = &content->linkNode;
    return 0;
}
int System::Unsubscribe(SystemContent* content)
{
    for (u32 i = 0; i < content->numFunctions; ++i) content->pFunctionDesc[i].u.key = -1;
    for (u32 i = 0; i < content->numClasses; ++i) content->pClassDesc[i].u.key = -1;
    for (u32 i = 0; i < content->numGlobalVariables; ++i) content->pGlobalVariableDesc[i].u.key = -1;
    SystemContentNode* node = &content->linkNode;
    if (node == gpCsisListHead) gpCsisListHead = node->next;
    if (node->prev) node->prev->next = node->next;
    if (node->next) node->next->prev = node->prev;
    return 0;
}

// ARTIST 82B0F7A0/F890/F9D0: two passes, first restricted to the system CRC.
// The caller supplies original SystemDesc member offsets (20/10,24/12,28/14).
// Recover the corresponding typed members instead of byte-offset field access.
template <class TDesc> struct DescriptorArray;
template <> struct DescriptorArray<CsisDef::FunctionDesc>
{
    static CsisDef::FunctionDesc* Get(SystemContent* content, int offset)
    { return offset == 24 ? content->pClassDesc : content->pFunctionDesc; }
    static u16 Count(SystemContent* content, int offset)
    { return offset == 12 ? content->numClasses : content->numFunctions; }
};
template <> struct DescriptorArray<CsisDef::GlobalVariableDesc>
{
    static CsisDef::GlobalVariableDesc* Get(SystemContent* content, int)
    { return content->pGlobalVariableDesc; }
    static u16 Count(SystemContent* content, int)
    { return content->numGlobalVariables; }
};
template <class THandle, class TId, class TDesc>
Result SetHandle(THandle* handle, const TId* id, int, int arrayOffset, int countOffset)
{
    for (int pass = 0; pass < 2; ++pass)
    {
        for (SystemContentNode* node = gpCsisListHead; node; node = node->next)
        {
            SystemContent* content = reinterpret_cast<SystemContent*>(
                reinterpret_cast<char*>(node) - offsetof(SystemContent, linkNode));
            TDesc* descriptors = DescriptorArray<TDesc>::Get(content, arrayOffset);
            u16 count = DescriptorArray<TDesc>::Count(content, countOffset);
            if (content->crc == id->muSystemId || pass)
            {
                for (u32 i = 0; i < count; ++i)
                {
                    TDesc& desc = descriptors[i];
                    if (desc.u.crcAndKey.crc == id->muInterfaceId &&
                        std::strcmp(id->mpName, desc.pStringId) == 0)
                    {
                        handle->mpDescriptor = &desc;
                        handle->miIndex = desc.u.key;
                        return RESULT_OK;
                    }
                }
            }
        }
    }
    return RESULT_ERR_NOTFOUND;
}
template Result SetHandle<ClassHandle, const InterfaceId, CsisDef::FunctionDesc>(ClassHandle*, const InterfaceId*, int, int, int);
template Result SetHandle<FunctionHandle, const InterfaceId, CsisDef::FunctionDesc>(FunctionHandle*, const InterfaceId*, int, int, int);
template Result SetHandle<GlobalVariableHandle, const InterfaceId, CsisDef::GlobalVariableDesc>(GlobalVariableHandle*, const InterfaceId*, int, int, int);
}
