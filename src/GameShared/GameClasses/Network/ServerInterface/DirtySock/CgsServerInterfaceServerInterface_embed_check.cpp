// Embed check for the CgsNetwork-ServerInterface (DirtySock) group:
//   - ServerInterfaceDirtySock      (GetMessageBuffer + vector deleting dtor)
//   - ServerInterfaceDirtySockX360  (5 forwarding lifecycle overrides)
//   - ServerInterfaceGameParamsBase (SetName/SetPassword/SetSession/IsRankedGame/operator=)
//
// Compile-only: forces every reconstructed type, vtable slot and member access to
// be parsed and type-checked together. It exercises the bodied functions through
// thin concrete shims so the abstract bases are realisable without depending on
// the not-yet-homed sibling component/SDK TUs.

#include "CgsServerInterfaceDirtySock.h"
#include "X360/CgsServerInterfaceDirtySockX360.h"
#include "Components/CgsServerInterfaceGameParams.h"

namespace
{
    // Concrete realisation of ServerInterfaceGameParamsBase: it is non-abstract
    // here (SetRankedGame's body lives in its own X360 TU), so we only override it
    // to make the vtable self-contained for this isolated check.
    struct ConcreteGameParams : public CgsNetwork::ServerInterfaceGameParamsBase
    {
        void SetRankedGame(bool) {}

        // ServerInterfaceStructureInterface pure-virtual surface.
        const char* GetPattern() const       { return "gp"; }
        s32         GetPatternLength() const  { return 2; }
        u32         GetDataSize() const       { return 0; }
        void*       GetData()                 { return nullptr; }
        const void* GetData() const           { return nullptr; }
    };
}

extern "C" int CgsServerInterfaceServerInterface_embed_check()
{
    ConcreteGameParams liParams;
    liParams.SetName("host");
    liParams.SetPassword("pw");
    liParams.SetSession("session");

    ConcreteGameParams liCopy;
    liCopy = liParams;                       // operator= (member-wise copy)

    int liResult = liCopy.IsRankedGame() ? 1 : 0;
    liResult += liCopy.GetPatternLength();

    // Type-check the GetMessageBuffer accessor + the X360 forwarding overrides
    // through pointers (no instantiation of the abstract bases required).
    char* (CgsNetwork::ServerInterfaceDirtySock::*lpfnGetBuf)() const =
        &CgsNetwork::ServerInterfaceDirtySock::GetMessageBuffer;
    (void)lpfnGetBuf;

    void (CgsNetwork::ServerInterfaceDirtySockX360::*lpfnUpdate)() =
        &CgsNetwork::ServerInterfaceDirtySockX360::Update;
    (void)lpfnUpdate;

    return liResult;
}
