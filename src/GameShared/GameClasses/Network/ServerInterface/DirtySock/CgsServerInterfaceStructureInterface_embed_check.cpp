#include "CgsServerInterfaceStructureInterface.h"

// Embed check: instantiate a concrete realization of the abstract interface so
// the vtable, the virtual destructor (and the compiler-generated deleting
// destructor thunk modelled by it), and every pure-virtual slot are emitted and
// type-checked. Exercises both the scalar (stack) and the delete-expression
// teardown paths.

namespace
{
    struct ConcreteStructure : public CgsNetwork::ServerInterfaceStructureInterface
    {
        u8          maData[4];

        const char* GetPattern() const       { return "test"; }
        s32         GetPatternLength() const  { return 4; }
        u32         GetDataSize() const       { return sizeof(maData); }
        void*       GetData()                 { return maData; }
        const void* GetData() const           { return maData; }
    };
}

extern "C" int CgsServerInterfaceStructureInterface_embed_check()
{
    ConcreteStructure  liStack;        // scalar destructor path
    CgsNetwork::ServerInterfaceStructureInterface* lpHeap = new ConcreteStructure;

    int liResult = liStack.GetPatternLength()
                 + static_cast<int>(lpHeap->GetDataSize());

    delete lpHeap;                     // delete-expression (a2 & 1) path
    return liResult;
}
