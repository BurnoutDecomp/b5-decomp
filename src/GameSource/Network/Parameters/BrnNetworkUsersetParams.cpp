#include "BrnNetworkUsersetParams.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::UsersetParams::`vector deleting destructor'  @ 0x8255EAF0
//
// Stores the class vptr (off_8207C88C) at this+0 then conditionally calls
// operator delete -- the thunk emitted for a virtual destructor. Reconstructed
// as the out-of-line virtual dtor.

namespace BrnNetwork
{
    UsersetParams::~UsersetParams()
    {
    }

    bool UsersetParams::Prepare()
    {
        return CgsNetwork::ServerInterfaceUsersetParamsX360::Prepare();
    }

    const char* UsersetParams::GetPattern() const
    {
        return "";
    }

    u32 UsersetParams::GetDataSize() const
    {
        return 0;
    }

    void* UsersetParams::GetData()
    {
        return nullptr;
    }

    const void* UsersetParams::GetData() const
    {
        return nullptr;
    }
}
