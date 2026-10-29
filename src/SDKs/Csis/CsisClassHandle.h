#pragma once
#include "types.hpp"

namespace Csis
{
// Names corroborated by DecFIGS csis.h; values used by ARTIST binding paths.
enum Result
{
    RESULT_OK = 0, RESULT_ERR_MEM = -1, RESULT_ERR_ALLOCATE = -2,
    RESULT_ERR_HANDLEEXPIRED = -3, RESULT_ERR_UNUSED = -4,
    RESULT_ERR_NOTFOUND = -5, RESULT_ERR_UNINITIALIZED = -6,
    RESULT_ERR_ILLEGALOP = -7, E_OK = RESULT_OK
};

// ARTIST SetHandle loads the string at +0 and signed CRCs at +4/+6.
struct InterfaceId
{
    const char* mpName;
    s16 muSystemId;
    s16 muInterfaceId;
};
union Parameter { int iVal; float fVal; };
}
namespace CsisDef { struct FunctionDesc; }
namespace Csis
{
template <class THandle, class TId, class TFunctionDesc>
Result SetHandle(THandle*, const TId*, int, int, int);

class ClassHandle
{
public:
    ClassHandle(); // ARTIST 82B0F150 writes only mKey at +4.
    Result SetFast(const InterfaceId*);
    union { void* mpDescriptor; uintptr_t miPayload; };
    s32 miIndex;
};
CGS_STATIC_ASSERT(sizeof(InterfaceId) == 8, "ARTIST interface selector");
CGS_STATIC_ASSERT(offsetof(InterfaceId, muSystemId) == 4, "ARTIST system CRC");
CGS_STATIC_ASSERT(offsetof(InterfaceId, muInterfaceId) == 6, "ARTIST interface CRC");
CGS_STATIC_ASSERT(sizeof(ClassHandle) == 8, "ARTIST class handle");
CGS_STATIC_ASSERT(offsetof(ClassHandle, miIndex) == 4, "ARTIST handle key");
}
