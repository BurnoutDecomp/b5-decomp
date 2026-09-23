#ifndef CGS_SERVER_INTERFACE_DEFAULT_PLAYER_PARAMETERS_H
#define CGS_SERVER_INTERFACE_DEFAULT_PLAYER_PARAMETERS_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerParams.h"

// ===========================================================================
// CgsNetwork::DefaultPlayerParameters
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceDefaultPlayerParameters.{h,cpp}
//
// The default player-parameters record: the player-parameters structure base with the
// default Prepare (clear every parameter, open firewall). Its vector deleting destructor
// leaves the root ServerInterfaceStructureInterface vtable installed, the
// intermediate base adding nothing to tear down.
// ===========================================================================

namespace CgsNetwork
{
    struct DefaultPlayerParameters : public ServerInterfacePlayerParamsBase
    {
        DefaultPlayerParameters();

        // X360 vector deleting destructor @ 0x82890260 (vptr-install + conditional
        // operator delete is the compiler-synthesised thunk half; the source body is
        // the empty out-of-line virtual destructor).
        virtual ~DefaultPlayerParameters();

        // Vtable slot 6: reset the record to its defaults before a new game.
        virtual bool Prepare();

        // The structure-interface slots: an empty pattern of length 20, and no data block
        // (each is a shared folded body on the console).
        virtual const char* GetPattern() const;
        virtual s32         GetPatternLength() const;
        virtual u32         GetDataSize() const;
        virtual void*       GetData();
        virtual const void* GetData() const;
    };
} // namespace CgsNetwork

#endif // CGS_SERVER_INTERFACE_DEFAULT_PLAYER_PARAMETERS_H
