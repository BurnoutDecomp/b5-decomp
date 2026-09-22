#include "types.hpp"

#include "GameSource/Network/BrnNetworkManager.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"  // CgsNetwork::PackOrUnpackInt field primitive

// ============================================================================================
// BrnNetwork::BrnNetworkManager -- the online hub.
//
// All 39 console functions of the class are declared in BrnNetworkManager.h with their
// reconstructed signatures, and the class layout is homed there by name. Only PackOrUnpack is
// bodied here so far; the remaining bodies land in partfiles beside this one.
// ============================================================================================

namespace BrnNetwork
{
    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::PackOrUnpack
    //
    // Static field (de)serialise helper for a NetworkPlayerID carried in a reliable message:
    // copy the field into a local, route it through the shared quantised-int primitive over
    // the full signed 32-bit range, write the (possibly updated) value back, and return the
    // per-field pack/unpack status (0 == success).
    // ----------------------------------------------------------------------------------------
    BrnNetworkManager::PackOrUnpackResult BrnNetworkManager::PackOrUnpack(
            CgsNetwork::Message* lpMessage,
            NetworkPlayerID*     lpNetworkPlayerID )
    {
        s32 liValue = *lpNetworkPlayerID;
        const BrnNetworkManager::PackOrUnpackResult lxResult =
            CgsNetwork::PackOrUnpackInt( lpMessage, &liValue, 0x80000000, 0x7FFFFFFF );
        *lpNetworkPlayerID = liValue;
        return lxResult;
    }
}
