#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePrepareParams.h"

#include <cstring>   // std::memset

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfacePrepareParams::Construct  @ 0x82580AF0
//   (called by BrnNetwork::BrnNetworkManager::Prepare)
//
// Construct zero-initialises the whole param block: XMemSet(this, 0, 48) clears the
// component-pointer array (DWARF `ServerInterfaceComponent *[12] mapComponents`; on the
// console 12 * 4 == exactly the 48 bytes the XMemSet covers), then SIX word stores (stw)
// clear +0x30..+0x44 and one BYTE store (stb) clears +0x48. The X360 XMemSet is a plain
// memset; the trailing stores are reproduced by name here (the asm writes 0 to each in
// order; +0x48 is a single byte).
//
// The memset is written over sizeof(mapComponents), NOT the console literal 48 -- on the
// LLP64 host that array is 96 bytes, and the measured behaviour being reproduced is
// "zero every component slot", not "zero 48 bytes".

namespace CgsNetwork
{

void ServerInterfacePrepareParams::Construct()
{
    std::memset(mapComponents, 0, sizeof(mapComponents));   // XMemSet(this, 0, 48)
    muField_30 = 0;
    muField_34 = 0;
    muField_38 = 0;
    muField_3C = 0;
    muField_40 = 0;
    muField_44 = 0;
    mu8Field_48 = 0;   // stb (byte)
}

}
