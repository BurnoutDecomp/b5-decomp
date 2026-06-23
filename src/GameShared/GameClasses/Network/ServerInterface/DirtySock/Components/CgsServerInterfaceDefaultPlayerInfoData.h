#ifndef CGS_SERVER_INTERFACE_DEFAULT_PLAYER_INFO_DATA_H
#define CGS_SERVER_INTERFACE_DEFAULT_PLAYER_INFO_DATA_H

#include "types.hpp"
#include "../CgsServerInterfaceStructureInterface.h"

// ===========================================================================
// CgsNetwork::DefaultPlayerInfoData
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceDefaultPlayerInfoData.{h,cpp}
//
// A "default" (null-object) implementation of the player-info server-interface
// structure. Forward-declared in the DirtySock DWARF
//   (references/DecFIGS/dwarfdump/.../CgsServerInterfaceStructureInterface.h:173,
//    `struct CgsNetwork::DefaultPlayerInfoData;`)
// with no body and no own data members emitted; the only X360 codegen attributed
// to it is its scalar deleting destructor @ 0x82890218.
//
// X360 EVIDENCE (scalar deleting destructor @ 0x82890218):
//   *this = &off_8207C88C;            ; install the StructureInterface vtable
//   if ( a2 & 1 ) operator delete(this);
//   return this;
// The vptr it installs is off_8207C88C -- the SAME vtable as the abstract
// ServerInterfaceStructureInterface base (CgsServerInterfaceStructureInterface.cpp
// `vector deleting destructor' @ 0x82541120 installs the identical pointer). The
// destructor therefore reduces entirely to the base sub-object: this class adds
// NO vtable slots of its own and NO data members beyond the inherited vptr, which
// is exactly the codegen for a thin default that does not override the interface's
// pure virtuals with new state.
//
// Because no GetPattern / GetData / size bodies are attributed to this type in the
// X360 image or the DWARF, NONE are fabricated here: the class is left ABSTRACT
// (it inherits the interface's pure virtuals). Only the out-of-line destructor is
// defined, which anchors emission of the inherited vtable pointer to this TU --
// mirroring the established deleting-destructor home convention in this tree
// (CgsServerInterfaceComponentDtor.cpp).
// ===========================================================================

namespace CgsNetwork
{
    struct DefaultPlayerInfoData : public ServerInterfaceStructureInterface
    {
        DefaultPlayerInfoData();

        // X360 scalar deleting destructor @ 0x82890218 (vptr-install + conditional
        // operator delete is the compiler-synthesised thunk half; the source body is
        // the empty out-of-line virtual destructor).
        virtual ~DefaultPlayerInfoData();
    };
} // namespace CgsNetwork

#endif // CGS_SERVER_INTERFACE_DEFAULT_PLAYER_INFO_DATA_H
