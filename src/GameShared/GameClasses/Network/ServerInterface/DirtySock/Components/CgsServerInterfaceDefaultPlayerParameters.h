#ifndef CGS_SERVER_INTERFACE_DEFAULT_PLAYER_PARAMETERS_H
#define CGS_SERVER_INTERFACE_DEFAULT_PLAYER_PARAMETERS_H

#include "types.hpp"
#include "../CgsServerInterfaceStructureInterface.h"

// ===========================================================================
// CgsNetwork::DefaultPlayerParameters
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceDefaultPlayerParameters.{h,cpp}
//
// A "default" (null-object) implementation of the player-parameters
// server-interface structure. Forward-declared in the DirtySock DWARF
//   (references/DecFIGS/dwarfdump/.../CgsServerInterfaceStructureInterface.h:171,
//    `struct CgsNetwork::DefaultPlayerParameters;`)
// with no body and no own data members emitted; the only X360 codegen attributed
// to it is its vector deleting destructor @ 0x82890260.
//
// X360 EVIDENCE (vector deleting destructor @ 0x82890260):
//   *this = &off_8207C88C;            ; install the StructureInterface vtable
//   if ( a2 & 1 ) operator delete(this);
//   return this;
// The installed vptr is off_8207C88C -- the SAME vtable as the abstract
// ServerInterfaceStructureInterface base. The destructor reduces entirely to the
// base sub-object: this class adds NO vtable slots of its own and NO data members
// beyond the inherited vptr -- the codegen of a thin default that does not give
// the interface's pure virtuals new state.
//
// No GetPattern / GetData / size bodies are attributed to this type in the X360
// image or DWARF, so NONE are fabricated: the class is left ABSTRACT (inherits the
// interface's pure virtuals). Only the out-of-line destructor is defined, anchoring
// emission of the inherited vtable pointer to this TU (matching the established
// deleting-destructor home convention, CgsServerInterfaceComponentDtor.cpp).
// ===========================================================================

namespace CgsNetwork
{
    struct DefaultPlayerParameters : public ServerInterfaceStructureInterface
    {
        DefaultPlayerParameters();

        // X360 vector deleting destructor @ 0x82890260 (vptr-install + conditional
        // operator delete is the compiler-synthesised thunk half; the source body is
        // the empty out-of-line virtual destructor).
        virtual ~DefaultPlayerParameters();
    };
} // namespace CgsNetwork

#endif // CGS_SERVER_INTERFACE_DEFAULT_PLAYER_PARAMETERS_H
