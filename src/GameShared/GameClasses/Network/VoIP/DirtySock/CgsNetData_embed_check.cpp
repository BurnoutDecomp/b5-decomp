// Embed check for the CgsNet-data leaf group (X360):
//   - CgsNetwork::DefaultPlayerInfoData::~DefaultPlayerInfoData    @ 0x82890218 (scalar deleting dtor)
//   - CgsNetwork::DefaultPlayerParameters::~DefaultPlayerParameters @ 0x82890260 (vector deleting dtor)
//   - CgsNetwork::VoIPClient::operator=                            @ 0x82893618
//
// Includes all three homes together to catch cross-header ODR / include-order
// problems and exercises each bodied function. Compile-only.
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceDefaultPlayerInfoData.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceDefaultPlayerParameters.h"
#include "GameShared/GameClasses/Network/VoIP/DirtySock/CgsVoIPManagerDirtySock.h"

namespace
{
    using namespace CgsNetwork;

    // Both Default* types derive from the vptr-only ServerInterfaceStructureInterface
    // base; they add no data members, so each must be exactly the base size (a single
    // vtable pointer). Sizes are compared between types -- no absolute X360 offset is
    // asserted across the 32-bit/64-bit boundary.
    static_assert(sizeof(DefaultPlayerInfoData) == sizeof(ServerInterfaceStructureInterface),
                  "DefaultPlayerInfoData adds no members beyond the StructureInterface vptr");
    static_assert(sizeof(DefaultPlayerParameters) == sizeof(ServerInterfaceStructureInterface),
                  "DefaultPlayerParameters adds no members beyond the StructureInterface vptr");

    // The Default* types are abstract (they inherit the interface's pure virtuals and
    // override nothing), so they cannot be instantiated directly. Exercise their
    // deleting-destructor codegen through concrete leaf subclasses that satisfy the
    // pure virtuals with trivial stubs (the stubs are test scaffolding, NOT the
    // reconstructed bodies -- no GetPattern/GetData body is attributed to the Default
    // types in the X360 image).
    struct ConcreteInfo : public DefaultPlayerInfoData
    {
        const char* GetPattern() const { return ""; }
        s32         GetPatternLength() const { return 0; }
        u32         GetDataSize() const { return 0; }
        void*       GetData() { return 0; }
        const void* GetData() const { return 0; }
    };

    struct ConcreteParams : public DefaultPlayerParameters
    {
        const char* GetPattern() const { return ""; }
        s32         GetPatternLength() const { return 0; }
        u32         GetDataSize() const { return 0; }
        void*       GetData() { return 0; }
        const void* GetData() const { return 0; }
    };

    // Drive the virtual deleting-destructor path (the @0x82890218 / @0x82890260 thunk
    // bodies) through a base interface pointer + heap delete.
    void ExerciseDefaultDtors()
    {
        ServerInterfaceStructureInterface* lpInfo   = new ConcreteInfo();
        ServerInterfaceStructureInterface* lpParams = new ConcreteParams();
        delete lpInfo;
        delete lpParams;
    }

    // Exercise VoIPClient::operator= (@0x82893618): memberwise copy of the scalar
    // prefix + the two HeadsetStatusMessage members.
    VoIPClient ExerciseVoIPClientAssign()
    {
        VoIPClient lSrc;
        lSrc.mPlayerID                       = 7;
        lSrc.miConnectionID                  = 3;
        lSrc.mbIsBlocked                     = true;
        lSrc.mHeadsetSendMsg.mu8HeadsetStatus = 2;
        lSrc.mHeadsetRecMsg.mu8HeadsetStatus  = 1;

        VoIPClient lDst;
        lDst = lSrc;          // -> VoIPClient::operator=
        return lDst;
    }

    void UseAll()
    {
        ExerciseDefaultDtors();
        VoIPClient lResult = ExerciseVoIPClientAssign();
        (void)lResult.mPlayerID;
    }
}
