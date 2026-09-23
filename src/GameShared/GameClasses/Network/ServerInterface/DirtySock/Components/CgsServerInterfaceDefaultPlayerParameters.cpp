#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceDefaultPlayerParameters.h"

#include <new>

// Platform memset (declared exactly as the other network TUs declare it).
extern "C"
{
    void* XMemSet(void* lpDest, s32 liValue, u32 luCount);
}

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::DefaultPlayerParameters::`vector deleting destructor' @ 0x82890260
//
// ---------------------------------------------------------------------------
// `vector deleting destructor'  @ 0x82890260
//
//   stw  off_8207C88C, 0(r31)      ; install the StructureInterface vtable at this+0
//   clrlwi r10, r4, 31             ; r10 = a2 & 1   (the "delete me too" flag)
//   if ( (a2 & 1) != 0 )
//   {
//       operator delete(this);     ; bl operator_delete
//       return this;
//   }
//   return this;
//
// off_8207C88C is the ServerInterfaceStructureInterface vtable (identical to the
// pointer installed by that base's own deleting destructor @ 0x82541120). This
// default adds no vtable slots and no data members, so the destructor body is
// empty; MSVC regenerates the vptr-install + conditional `operator delete` thunk
// half from the trivial virtual ~DefaultPlayerParameters(). Defining it out-of-line
// here anchors the inherited vtable emission to this TU.
// ---------------------------------------------------------------------------

namespace CgsNetwork
{
    DefaultPlayerParameters::DefaultPlayerParameters()
    {
    }

    DefaultPlayerParameters::~DefaultPlayerParameters()
    {
    }

    // Clear every parameter, open firewall.
    bool DefaultPlayerParameters::Prepare()
    {
        XMemSet(macMachineAddress, 0, sizeof(macMachineAddress));
        mpcName           = 0;
        miIdent           = 0;
        miPresence        = 0;
        muExternalAddress = 0;
        muInternalAddress = 0;
        muFlags           = 0;
        meFirewallSetting = E_FIREWALL_OPEN;
        return true;
    }

    namespace
    {
        // The default record's pattern length (the console constant 20).
        const s32 KI_DEFAULT_PATTERN_LENGTH = 20;
    }

    const char* DefaultPlayerParameters::GetPattern() const
    {
        return "";
    }

    s32 DefaultPlayerParameters::GetPatternLength() const
    {
        return KI_DEFAULT_PATTERN_LENGTH;
    }

    u32 DefaultPlayerParameters::GetDataSize() const
    {
        return 0;
    }

    void* DefaultPlayerParameters::GetData()
    {
        return 0;
    }

    const void* DefaultPlayerParameters::GetData() const
    {
        return 0;
    }
}
