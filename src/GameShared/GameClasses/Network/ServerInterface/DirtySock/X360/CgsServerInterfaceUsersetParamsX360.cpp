#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfaceUsersetParamsX360.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceUsersetParamsX360::Prepare              @ 0x8287C790
//   CgsNetwork::ServerInterfaceUsersetParamsX360::SerialiseToString    @ 0x82889E88
//   CgsNetwork::ServerInterfaceUsersetParamsX360::SerialiseFromUserset @ 0x82889E90
//
// The X360 leaf adds no userset-parameter state of its own; every override is a
// pure forwarder to ServerInterfaceUsersetParamsBase (they occupy the leaf's
// vtable slots but do no additional work). SerialiseToString / SerialiseFromUserset
// are single `b` tail-calls in the asm; Prepare calls the base then normalises the
// return to 0/1 -- which the `bool` return already is.

namespace CgsNetwork
{

// Prepare @ 0x8287C790 -- forward to the base; asm: bl base::Prepare, then
// clrlwi/cntlzw/extrwi/xori => !!r3 (a no-op for a bool 0/1 result).
bool ServerInterfaceUsersetParamsX360::Prepare()
{
    return ServerInterfaceUsersetParamsBase::Prepare();
}

// SerialiseToString @ 0x82889E88 -- pure tail-call forward to the base.
void ServerInterfaceUsersetParamsX360::SerialiseToString(char* lpcRecord, s32 liRecLen) const
{
    ServerInterfaceUsersetParamsBase::SerialiseToString(lpcRecord, liRecLen);
}

// SerialiseFromUserset @ 0x82889E90 -- pure tail-call forward to the base.
void ServerInterfaceUsersetParamsX360::SerialiseFromUserset(const void* lpUserset)
{
    ServerInterfaceUsersetParamsBase::SerialiseFromUserset(lpUserset);
}

}
