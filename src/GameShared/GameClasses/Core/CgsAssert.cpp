#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsDev::Assert front-end. The X360 build enters gAssertMutex (BeginAssert), forwards to
// CgsDev::Assert::Manager::HandleAssert (FireAssert: log + break/ignore per policy), and leaves
// the mutex (EndAssert). The assert Manager is its own TU; these minimal bodies let the boot/
// loading path link with asserts inert (a fired assert is recorded as a no-op return).
namespace CgsDev
{
    namespace Assert
    {
        int   BeginAssert()                              { return 0; }
        int   FireAssert(const char*, const char*, int)  { return 0; }
        void* EndAssert()                                { return 0; }
    }
}
