#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/core/debug/DebugCriticalSection.h"

namespace CgsDev
{
namespace Assert
{
    namespace
    {
        char gacMessageBuffer[KI_MESSAGEBUFFERSIZE];
    }
    char* gpcMessageBuffer = gacMessageBuffer;
    s32 giMessageBufferSize = KI_MESSAGEBUFFERSIZE;
    rw::core::debug::detail::DebugCriticalSection gAssertMutex = { 0 };

    // ARTIST82820758: this is the manager's actual initialization flag. Set it
    // before creating the mutex, then seed the native font and process any cache.
    void Construct()
    {
        if (!gAssertManager.mbInitialised)
        {
            gAssertManager.mbInitialised = true;
            gAssertMutex.Create();
            gAssertManager.mVectorFont.Construct();
            Vector2 lSize(14.0f, 14.0f);
            gAssertManager.mVectorFont.SetSize(lSize);
            Manager::RenderThreadAsserts();
        }
    }

    void BeginAssert()
    {
        gAssertMutex.Enter();
    }

    void EndAssert()
    {
        gAssertMutex.Leave();
    }

    // ARTIST82820810: preserve the first capture and normalize null strings to
    // the exact empty string before the real manager handles/cache-displays it.
    void FireAssert(const char* lpcExpression, const char* lpcFile, int liLine)
    {
        if (!gAssertManager.mbGotAssert)
            gAssertManager.mCurrentAssert.mStack.Prepare();
        gAssertManager.HandleAssert(lpcExpression ? lpcExpression : "",
            lpcFile ? lpcFile : "", liLine);
    }

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
