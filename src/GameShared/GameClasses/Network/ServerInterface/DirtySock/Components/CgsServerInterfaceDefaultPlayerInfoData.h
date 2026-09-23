#ifndef CGS_SERVER_INTERFACE_DEFAULT_PLAYER_INFO_DATA_H
#define CGS_SERVER_INTERFACE_DEFAULT_PLAYER_INFO_DATA_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfoData.h"

// ===========================================================================
// CgsNetwork::DefaultPlayerInfoData
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceDefaultPlayerInfoData.{h,cpp}
//
// The default player-info record: the player-info structure base with the default
// Prepare (the base reset, reported as a bool). Its scalar deleting destructor
// leaves the root ServerInterfaceStructureInterface vtable installed,
// the intermediate base adding nothing to tear down.
// ===========================================================================

namespace CgsNetwork
{
    struct DefaultPlayerInfoData : public ServerInterfacePlayerInfoDataBase
    {
        DefaultPlayerInfoData();

        // X360 scalar deleting destructor @ 0x82890218 (vptr-install + conditional
        // operator delete is the compiler-synthesised thunk half; the source body is
        // the empty out-of-line virtual destructor).
        virtual ~DefaultPlayerInfoData();

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

#endif // CGS_SERVER_INTERFACE_DEFAULT_PLAYER_INFO_DATA_H
