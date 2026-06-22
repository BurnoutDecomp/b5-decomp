// Translation-unit embed check for the CgsNetwork packet-message GetName group 2:
//   CgsNetwork::SignalMessage / TestConnectionMessage / PingMessage / PingReplyMessage.
// Pulls in every reconstructed home so the gate compiles the headers together and
// exercises each TU's ledger GetName func(s).
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsSignalMessage.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsTestConnectionMessage.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsPingMessage.h"

namespace CgsNetwork
{
    // NOTE: absolute offsets/sizes are NOT asserted here -- the Message base begins
    // with mpVTable, which is 4 bytes on the X360 (32-bit) but 8 on the host (64-bit),
    // so any byte-offset that crosses that pointer differs between platforms. Only
    // platform-stable RELATIVE facts are asserted.

    // SignalMessage and TestConnectionMessage add no data of their own: each is the
    // same size as its committed base.
    static_assert(sizeof(SignalMessage) == sizeof(MessageWithPlayerIDs),
                  "SignalMessage adds no own data");
    static_assert(sizeof(TestConnectionMessage) == sizeof(ReliableMessage),
                  "TestConnectionMessage adds no own data");

    // PingMessage / PingReplyMessage each add exactly one f32 after the Message base.
    static_assert(sizeof(PingMessage) > sizeof(Message),
                  "PingMessage carries mfPingTime");
    static_assert(sizeof(PingReplyMessage) > sizeof(Message),
                  "PingReplyMessage carries mfPingTime");

    void CgsNetworkPacketMsgs2_EmbedCheck()
    {
        SignalMessage         lSignal;
        TestConnectionMessage lTestConn;
        PingMessage           lPing;
        PingReplyMessage      lPingReply;

        const char* lpSignalName    = lSignal.GetName();    // 0x827DDF40
        const char* lpTestConnName  = lTestConn.GetName();  // 0x827DE358
        const char* lpPingName       = lPing.GetName();      // 0x827DE0C8
        const char* lpPingReplyName  = lPingReply.GetName(); // 0x827DE0D8

        (void)lpSignalName;
        (void)lpTestConnName;
        (void)lpPingName;
        (void)lpPingReplyName;
    }
} // namespace CgsNetwork
