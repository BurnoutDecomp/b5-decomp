#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePingRegions.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (Begin/Fire/End)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfacePingRegions::GetPingValue              @ 0x82877968
//   CgsNetwork::ServerInterfacePingRegions::`scalar deleting destructor' @ 0x827DE2C8
//
// GetPingValue asserts 0 <= liRegion < miCurrentRegion (both vacuous CGS_ASSERT guards), then
// returns the recorded ping. The asm address math `*(this + (liRegion + 4) * 4)` is
// this+0x10+liRegion*4 -- i.e. maRegionPings[liRegion] (the +4 words is the 16-byte
// ServerInterfaceComponent base, the *4 is the word stride).
//
// The destructor is the trivial MSVC scalar-deleting-destructor pattern shared by the
// component leaves: it restores the component vtable off_820CDBF8 at this+0 and, when the
// low free-flag bit is set, calls operator delete. ServerInterfacePingRegions owns no heap
// members freed in this thunk, so the C++ destructor body is empty; the compiler synthesises
// the vtable-store + conditional free.

namespace CgsNetwork
{

ServerInterfacePingRegions::ServerInterfacePingRegions()
    : miField_D8(0)
    , miField_DC(0)
    , mpPingManager(0)
    , miCurrentRegion(0)
    , miField_E8(-1)
    , miField_EC(2)
    , miField_F0(1)
{
    // Per the X360 init (Construct @ 0x8287C8A8): every region ping starts at -1.
    // The full field initialisation (error-data ptr / status word, etc.) is performed by the
    // ServerInterfacePingRegions::Construct virtual, reconstructed in its own TU.
    for (s32 liRegion = 0; liRegion < KI_MAX_PING_REGIONS; ++liRegion)
    {
        maRegionPings[liRegion] = -1;
    }
}

ServerInterfacePingRegions::~ServerInterfacePingRegions()
{
}

s32 ServerInterfacePingRegions::GetPingValue(s32 liRegion) const
{
    CGS_ASSERT(liRegion >= 0, "liRegion >= 0");
    CGS_ASSERT(liRegion < miCurrentRegion, "liRegion < miCurrentRegion");

    return maRegionPings[liRegion];   // this+0x10+liRegion*4
}

}
