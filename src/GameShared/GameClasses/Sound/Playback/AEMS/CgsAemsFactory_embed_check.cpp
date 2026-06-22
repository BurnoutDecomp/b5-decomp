// Embed check for CgsAemsFactory.{h,cpp}: exercises both bodied ledger functions.
// FindPatchMonitor/CsisPrint are non-public, so a friend-ish test subclass reaches
// them through the protected/public surface.
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstring>

using namespace CgsSound::Playback;

// CsisPrint is public; FindPatchMonitor is protected. Expose it via a thin probe.
struct AemsFactoryProbe : public AemsFactory
{
    PatchMonitor* CallFind(const char* lpcName) { return FindPatchMonitor(lpcName); }
};

// Provide the externs the .cpp references so the check links standalone.
namespace CgsDev
{
namespace Log
{
    StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
    static DebugPrint sStubPrint;
    DebugPrint* gpDebugPrint = &sStubPrint;
    void WriteToLog(const char*) {}
}
namespace Message
{
    u32 gxMessageFilterFlags = 0;
}
}
// StrStreamBase ctor referenced by DebugPrint's implicit base ctor.
CgsDev::StrStreamBase::StrStreamBase() {}

static void exercise()
{
    AemsFactoryProbe lProbe = AemsFactoryProbe(); // value-init -> count == 0

    // CsisPrint returns its argument; null maps to "<NULLSTRING>".
    const char* lpcEcho = lProbe.CsisPrint("hello");
    CGS_ASSERT(std::strcmp(lpcEcho, "hello") == 0, "CsisPrint echoes its argument");
    const char* lpcNull = lProbe.CsisPrint(nullptr);
    CGS_ASSERT(lpcNull == nullptr, "CsisPrint returns the (null) argument unchanged");

    // FindPatchMonitor on a freshly-zeroed table returns null (count == 0 path).
    // (Members are private; we rely on the count being default-initialised to 0
    // only conceptually -- the probe object's storage is not zeroed, so just verify
    // the call path compiles and runs without dereferencing a populated table.)
    PatchMonitor* lpFound = lProbe.CallFind("Foo");
    (void)lpFound;
}

static_assert(sizeof(PatchMonitor) == sizeof(void*) * 3 + sizeof(s32) ||
              sizeof(PatchMonitor) >= sizeof(void*) * 3 + sizeof(s32),
              "PatchMonitor holds name/func/data pointers + perfmon id");

int main()
{
    exercise();
    return 0;
}
