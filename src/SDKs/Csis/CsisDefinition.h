#pragma once
#include "SDKs/Csis/CsisClassHandle.h"
namespace CsisDef
{
union Parameter { s32 intVal; float floatVal; };
struct CrcAndKey { s16 crc; s16 key; };
union Key { CrcAndKey crcAndKey; s32 key; };
struct FunctionDesc
{
    void* clients;
    const char* pStringId;
    Key u;
};
typedef FunctionDesc ClassDesc;
struct GlobalVariableDesc
{
    void* clients;
    Parameter curVal;
    const char* pStringId;
    Key u;
};
}
namespace Csis
{
// Native SystemDesc geometry: ARTIST Subscribe 82B0F2A8, SetHandle 82B0F7A0.
struct SystemContentNode { SystemContentNode* next; SystemContentNode* prev; };
struct SystemContent
{
    char id[4];
    u8 ver, verCsisxMajor, verCsisxMinor, verCsisxPatch;
    u8 platform, resolved;
    u16 numFunctions, numClasses, numGlobalVariables;
    s16 crc;
    char pad[2];
    CsisDef::FunctionDesc* pFunctionDesc;
    CsisDef::ClassDesc* pClassDesc;
    CsisDef::GlobalVariableDesc* pGlobalVariableDesc;
    SystemContentNode linkNode;
};
// ARTIST 82B0F980 / 82B0FAC0. A negative key returns without reading descriptor.
template <class THandle, class TDesc>
Result ValidHandle(THandle* pHandle, TDesc*)
{
    if (pHandle->miIndex < 0) return static_cast<Result>(pHandle->miIndex);
    if (!pHandle->mpDescriptor) return RESULT_ERR_UNINITIALIZED;
    TDesc* pDesc = static_cast<TDesc*>(pHandle->mpDescriptor);
    if (pHandle->miIndex == pDesc->u.key) return RESULT_OK;
    pHandle->miIndex = RESULT_ERR_HANDLEEXPIRED;
    pHandle->mpDescriptor = 0;
    return RESULT_ERR_HANDLEEXPIRED;
}
CGS_STATIC_ASSERT(sizeof(CsisDef::Parameter) == 4, "ARTIST parameter");
CGS_STATIC_ASSERT(sizeof(CsisDef::FunctionDesc) == 12, "ARTIST function descriptor stride");
CGS_STATIC_ASSERT(offsetof(CsisDef::FunctionDesc, u) == 8, "ARTIST function key");
CGS_STATIC_ASSERT(sizeof(CsisDef::GlobalVariableDesc) == 16, "ARTIST global descriptor stride");
CGS_STATIC_ASSERT(offsetof(CsisDef::GlobalVariableDesc, u) == 12, "ARTIST global key");
CGS_STATIC_ASSERT(sizeof(SystemContent) == 40, "ARTIST system descriptor");
CGS_STATIC_ASSERT(offsetof(SystemContent, numFunctions) == 10, "ARTIST function count");
CGS_STATIC_ASSERT(offsetof(SystemContent, crc) == 16, "ARTIST system CRC");
CGS_STATIC_ASSERT(offsetof(SystemContent, pFunctionDesc) == 20, "ARTIST function array");
CGS_STATIC_ASSERT(offsetof(SystemContent, pClassDesc) == 24, "ARTIST class array");
CGS_STATIC_ASSERT(offsetof(SystemContent, pGlobalVariableDesc) == 28, "ARTIST global array");
CGS_STATIC_ASSERT(offsetof(SystemContent, linkNode) == 32, "ARTIST system link");
}
