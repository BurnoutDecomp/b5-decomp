#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceDefaultPlayerInfoData.h"

#include <new>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::DefaultPlayerInfoData::`scalar deleting destructor' @ 0x82890218
//
// ---------------------------------------------------------------------------
// `scalar deleting destructor'  @ 0x82890218
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
// off_8207C88C is the ServerInterfaceStructureInterface vtable (the same pointer
// installed by that base's own deleting destructor @ 0x82541120). This default
// adds no vtable slots and no data members, so the destructor body is empty; MSVC
// regenerates the vptr-install + conditional `operator delete` thunk half from the
// trivial virtual ~DefaultPlayerInfoData(). Defining it out-of-line here anchors
// the inherited vtable emission to this TU, matching where the X360 build placed
// the thunk.
// ---------------------------------------------------------------------------

namespace CgsNetwork
{
    DefaultPlayerInfoData::DefaultPlayerInfoData()
    {
    }

    DefaultPlayerInfoData::~DefaultPlayerInfoData()
    {
    }
}
