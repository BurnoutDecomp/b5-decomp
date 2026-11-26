#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

// The assert entry points (Construct, FireAssert, BeginAssert, EndAssert) and the shared message
// buffer and mutex are defined in their DWARF home, Development/AssertSystem/CgsAssertManager.cpp
// (DecFIGS CgsAssertManager.cpp:79-191; ARTIST 0x82817548 / 0x82817558 / 0x82820758 / 0x82820810
// lie in CgsDevelopmentUnity's range). This file keeps what DWARF homes in CgsAssert.cpp.

namespace CgsDev
{
namespace Assert
{
    void PrintStringed(const char* lpcFile, const char* lpcFunction, const int liLine, const char* lpcMessage)
    {
        (void)lpcFunction;
        StrStreamBase& lrStream = *Log::gpAssertPrint;
        lrStream << "CGSASSERT : ";
        lrStream << (lpcFile ? lpcFile : "<NULLSTRING>");
        lrStream << ":";
        lrStream << static_cast<s32>(liLine) << " " << (lpcMessage ? lpcMessage : "<NULLSTRING>") << "\n";
    }
}
}
