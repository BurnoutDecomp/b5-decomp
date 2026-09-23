#include "BrnNetworkPlayerInfoData.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::PlayerInfoData::`scalar deleting destructor'  @ 0x8255EA38
//
// Stores the class vptr (off_8207C88C) at this+0 then conditionally calls
// operator delete -- the thunk emitted for a virtual destructor. Reconstructed
// as the out-of-line virtual dtor.

namespace BrnNetwork
{
    PlayerInfoData::~PlayerInfoData()
    {
    }

    // Chain to the platform prepare; true on success.
    bool PlayerInfoData::Prepare()
    {
        return CgsNetwork::ServerInterfacePlayerInfoDataX360::Prepare();
    }

    // The record replicates nothing of its own: an empty pattern and no data (the console slots
    // are the shared empty-string / zero bodies).
    const char* PlayerInfoData::GetPattern() const
    {
        return "";
    }

    u32 PlayerInfoData::GetDataSize() const
    {
        return 0;
    }

    void* PlayerInfoData::GetData()
    {
        return nullptr;
    }

    const void* PlayerInfoData::GetData() const
    {
        return nullptr;
    }
}
