// Translation-unit embed check for the CgsNetwork packet-message GetName group:
//   CgsNetwork::HeadsetStatusMessage / HostKeepAliveMessage / NewHostMessage.
// Pulls in every reconstructed home so the gate compiles the headers together and
// exercises the single ledger func of each TU (GetName).
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsHeadsetStatusMessage.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsHostKeepAliveMessage.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsNewHostMessage.h"

#include <cstddef>  // offsetof

namespace CgsNetwork
{
    // NOTE: absolute offsets/sizes are NOT asserted here -- the Message base begins
    // with mpVTable, which is 4 bytes on the X360 (32-bit) but 8 on the host (64-bit),
    // so any byte-offset that crosses that pointer differs between platforms. Only
    // platform-stable RELATIVE facts are asserted.

    // HostKeepAliveMessage adds no data of its own: same size as the base.
    static_assert(sizeof(HostKeepAliveMessage) == sizeof(Message),
                  "HostKeepAliveMessage adds no own data");

    // NewHostMessage's two player-id fields are laid out in declaration order.
    static_assert(offsetof(NewHostMessage, maReceivedClientsIDs)
                      > offsetof(NewHostMessage, mNewHostID),
                  "maReceivedClientsIDs after mNewHostID");

    void CgsNetworkPacketMsgs_EmbedCheck()
    {
        HeadsetStatusMessage lHeadset;
        HostKeepAliveMessage lKeepAlive;
        NewHostMessage       lNewHost;

        const char* lpHeadsetName   = lHeadset.GetName();   // 0x827DBC78
        const char* lpKeepAliveName = lKeepAlive.GetName(); // 0x827DBC48
        const char* lpNewHostName   = lNewHost.GetName();   // 0x827DBC58

        (void)lpHeadsetName;
        (void)lpKeepAliveName;
        (void)lpNewHostName;
    }
} // namespace CgsNetwork
