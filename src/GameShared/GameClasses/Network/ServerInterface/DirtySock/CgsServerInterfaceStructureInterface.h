#ifndef CGS_SERVER_INTERFACE_STRUCTURE_INTERFACE_H
#define CGS_SERVER_INTERFACE_STRUCTURE_INTERFACE_H

#include "types.hpp"

// ===========================================================================
// CgsNetwork::ServerInterfaceStructureInterface
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/
//         CgsServerInterfaceStructureInterface.{h,cpp}   (DirtySock area)
//
// A pure abstract interface for a "server-interface structure": something that
// can describe itself to the DirtySock server-interface layer by exposing a
// byte pattern (with its length) and a raw data blob (with its size, mutable
// and const accessors).
//
// LAYOUT (from dwarfdump + X360 asm @ 0x82541120): the class carries ONLY the
// vtable pointer at offset 0 -- no data members. The vector deleting destructor
// stores the single vptr (off_8207C88C) at this+0 and then conditionally frees,
// which is exactly the codegen for a vptr-only polymorphic base. The vtable
// (declared below) is, in DWARF order:
//     ~ServerInterfaceStructureInterface()   (CgsServerInterfaceStructureInterface.h:47)
//     const char* GetPattern() const         (CgsServerInterfaceStructureInterface.h:50)
//     int32_t     GetPatternLength() const   (CgsServerInterfaceStructureInterface.cpp:41)
//     uint32_t    GetDataSize() const         (CgsServerInterfaceStructureInterface.h:56)
//     void*       GetData()                    (CgsServerInterfaceStructureInterface.h:59)
//     const void* GetData() const              (CgsServerInterfaceStructureInterface.h:62)
//
// Name preserved verbatim from the DWARF type name (it really is
// "...StructureInterface"); the I-prefix naming rule is for newly-coined
// interfaces without a real home name, so it does not apply here.
// ===========================================================================

namespace CgsNetwork
{
    struct ServerInterfaceStructureInterface
    {
    public:
        ServerInterfaceStructureInterface();

        // CgsServerInterfaceStructureInterface.h:47
        virtual ~ServerInterfaceStructureInterface();

        // CgsServerInterfaceStructureInterface.h:50
        virtual const char* GetPattern() const = 0;

        // CgsServerInterfaceStructureInterface.cpp:41 -- NOT pure: the DWARF homes a
        // real default body in the .cpp, and the X360 vtable off_8207C88C carries a
        // genuine target in this slot (0x82876410, a COMDAT-folded `return 20`)
        // while every other accessor slot is __purecall. Default body in the .cpp.
        virtual s32 GetPatternLength() const;

        // CgsServerInterfaceStructureInterface.h:56
        virtual u32 GetDataSize() const = 0;

        // CgsServerInterfaceStructureInterface.h:59
        virtual void* GetData() = 0;

        // CgsServerInterfaceStructureInterface.h:62
        virtual const void* GetData() const = 0;
    };
}

#endif // CGS_SERVER_INTERFACE_STRUCTURE_INTERFACE_H
