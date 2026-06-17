#include "GameShared/GameClasses/Development/MapFile/Reader/CgsMapFileReader.h"
#include "GameShared/GameClasses/Development/StackUnpick/CgsStackUnpick.h"   // StackUnpickBase
#include "GameShared/GameClasses/Core/CgsAssert.h"                           // CGS_ASSERT

namespace CgsDev
{
namespace MapFile
{
    Reader::Reader()
        : mpCallstack(nullptr)
    {
    }

    // CgsMapFileReader.cpp:48/51 - the base Prepare latches the call-stack the derived reader resolves
    // (asserting it is non-null). Derived readers call this, then open + read the map.
    void Reader::Prepare(const char* /*lpcMapFileName*/, StackUnpickBase* lpCallstack)
    {
        CGS_ASSERT(lpCallstack, "lpCallstack");
        mpCallstack = lpCallstack;
    }

    // CgsMapFileReader.cpp:66 - the base has no incremental work + resolves no names; derived overrides.
    void Reader::Update()
    {
    }

    const char* Reader::GetStackEntryName(s32 /*liIndex*/)
    {
        return nullptr;
    }
}
}
