// Embed check for CgsAemsFactory.{h,cpp}. REWORKED (AEMS-cascade slice 2): the
// class sits on its real AemsRWSampleFactory base now (spec ctor only -- no
// default construction without an Environment), and CsisPrint is the STATIC
// one-argument callback the ctor's CSIS tail takes the address of (void return;
// the old probe's echo-return expectation was stale). Instance-free checks only.
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstring>

using namespace CgsSound::Playback;

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

// The protected member surface stays shape-checked through a thin subclass that
// only takes member addresses (no instance is constructed).
struct AemsFactoryShapeProbe : public AemsFactory
{
    static void ShapeCheck()
    {
        // CsisPrint is the static one-arg callback (the ctor materializes its
        // raw address); taking it as a plain function pointer proves the ABI.
        void (*lpfnPrint)(const char*) = &AemsFactory::CsisPrint;
        lpfnPrint("hello");
        lpfnPrint(nullptr);   // maps to "<NULLSTRING>" inside

        // FindPatchMonitor keeps its member shape.
        PatchMonitor* (AemsFactory::*lpfnFind)(const char*) =
            &AemsFactoryShapeProbe::FindPatchMonitor;
        (void)lpfnFind;
    }
};

static_assert(sizeof(PatchMonitor) >= sizeof(void*) * 3 + sizeof(s32),
              "PatchMonitor holds name/func/data pointers + perfmon id");

int main()
{
    AemsFactoryShapeProbe::ShapeCheck();
    return 0;
}
