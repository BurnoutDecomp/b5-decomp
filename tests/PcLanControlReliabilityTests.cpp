// Exercise the production control receiver with a transport fixture. No network
// or game process is needed to advance the negotiated retry clock.
#include "../vendor/dirtysdk/src/pc/lan/pclan_core.cpp"

static bool gbOnline = true;
static uint32_t guTimeoutMs = 15000;
static unsigned guAcknowledged = 0;
static unsigned guDispatched = 0;
static uint32_t guLastAckSequence = 0;

const char* CgsPcNetIdentityName() { return "ControlTest"; }
u64 CgsPcNetIdentityXuid() { return 101; }
u32 CgsPcNetIdentityLobbyIdent() { return 101; }
bool CgsPcNetLanEnabled() { return true; }
namespace CgsDev { namespace Log { void WriteToLog(const char*) {} } }

namespace B5Online
{
bool Enabled() { return gbOnline; }
bool Configure(uintptr_t, uint32_t, PcLanPeerT*) { return true; }
void Shutdown() {}
void Update() {}
uint16_t MaxPlayers() { return 8; }
uint32_t TimeoutMs() { return guTimeoutMs; }
int32_t Send(uint32_t, const uint8_t* lpPacket, int32_t)
{
    if (lpPacket[5] == PCLAN_ACK)
    {
        ++guAcknowledged;
        guLastAckSequence = lpPacket[16] | (uint32_t(lpPacket[17]) << 8) |
            (uint32_t(lpPacket[18]) << 16) | (uint32_t(lpPacket[19]) << 24);
    }
    return 0;
}
int32_t Receive(const PcLanPeerT*, const uint8_t*, int32_t, uint8_t*, int32_t, PcLanPeerT*, uint64_t*, bool*) { return 0; }
}

static void OnControl(void*, const PcLanPeerT*, const PcLanHeaderT*, const uint8_t*)
{
    ++guDispatched;
}

static void ResetReceipts()
{
    gOnlineReceipts.clear();
    guAcknowledged = guDispatched = 0;
    gPcLan.uSelfIdent = 101;
    gPcLan.Self.uPort = 1;
    PcLanRegister(PCLAN_PLAY, OnControl, NULL);
}

static void ReceiveControl(uint32_t luSequence, uint64_t luNowMs, uint32_t luSender = 102, uint64_t luEpoch = 11)
{
    const PcLanPeerT lPeer = { luSender, 1, 0 };
    PcLanHeaderT lHeader = { PCLAN_MAGIC, PCLAN_VERSION, PCLAN_PLAY, 0, luSender, 77,
        static_cast<uint16_t>(luSequence), static_cast<uint16_t>(luSequence >> 16) };
    PcLanCoreReceiveOne(reinterpret_cast<const uint8_t*>(&lHeader), sizeof(lHeader), &lPeer, luNowMs, luEpoch);
}

static bool Check(bool lbPass, const char* lpName)
{
    printf("%s: %s\n", lbPass ? "PASS" : "FAIL", lpName);
    return lbPass;
}

int main()
{
    gbOnline = false;
    if (!Check(PcLanCorePeerTimeoutMs() == 5000, "LAN keeps its five-second timeout")) return 1;
    gbOnline = true;
    if (!Check(PcLanCorePeerTimeoutMs() == 15000, "online uses the negotiated timeout")) return 1;

    ResetReceipts();
    for (uint16_t luSequence = 1; luSequence <= 257; ++luSequence)
        ReceiveControl(luSequence, 1000 + (luSequence - 1) * 39);
    ReceiveControl(1, 11001);
    if (!Check(guDispatched == 257 && guAcknowledged == 258,
               "a late retry stays a duplicate after 256 newer controls")) return 1;

    ResetReceipts();
    for (uint16_t luSequence = 1; luSequence <= KU_ONLINE_DEDUP_CAPACITY; ++luSequence)
        ReceiveControl(luSequence, 1000);
    ReceiveControl(static_cast<uint16_t>(KU_ONLINE_DEDUP_CAPACITY + 1), 1001);
    if (!Check(guDispatched == KU_ONLINE_DEDUP_CAPACITY && guAcknowledged == KU_ONLINE_DEDUP_CAPACITY,
               "a full cache defers both dispatch and acknowledgement")) return 1;
    ReceiveControl(1, 1002);
    if (!Check(guDispatched == KU_ONLINE_DEDUP_CAPACITY && guAcknowledged == KU_ONLINE_DEDUP_CAPACITY + 1,
               "a full cache still acknowledges known duplicates")) return 1;
    ReceiveControl(static_cast<uint16_t>(KU_ONLINE_DEDUP_CAPACITY + 1), 31001);
    if (!Check(guDispatched == KU_ONLINE_DEDUP_CAPACITY + 1 && guAcknowledged == KU_ONLINE_DEDUP_CAPACITY + 2,
               "an unacknowledged control is delivered after receipt expiry")) return 1;

    ResetReceipts();
    guTimeoutMs = 300000;
    ReceiveControl(55, 1000);
    ReceiveControl(55, 1000, 103);
    const PcLanPeerT lDeparted = { 102, 1, 0 };
    PcLanHeaderT lBye = { PCLAN_MAGIC, PCLAN_VERSION, PCLAN_BYE, 0, 102, 77, 0, 0 };
    PcLanCoreReceiveOne(reinterpret_cast<const uint8_t*>(&lBye), sizeof(lBye), &lDeparted, 1001, 11);
    ReceiveControl(55, 1002);
    if (!Check(guDispatched == 2, "BYE does not allow a delayed old control to execute again")) return 1;
    ReceiveControl(55, 66536, 102, 12);
    if (!Check(guDispatched == 3, "a new authenticated connection may reuse its control sequence")) return 1;
    ReceiveControl(55, 66537, 102, 11);
    if (!Check(guDispatched == 3, "an old connection retry stays a duplicate after reconnect")) return 1;
    ReceiveControl(55, 66537, 103);
    if (!Check(guDispatched == 3, "peer departure preserves other peers' receipts")) return 1;

    ResetReceipts();
    ReceiveControl(1, 1000);
    ReceiveControl(0x10001, 1001);
    if (!Check(guDispatched == 2 && guLastAckSequence == 0x10001,
               "online control sequences retain and acknowledge their high word")) return 1;
    ReceiveControl(0x10000, 1002);
    if (!Check(guDispatched == 3 && guLastAckSequence == 0x10000,
               "a zero low word is still a reliable online control")) return 1;
    gPcLan.aPending[0].bUsed = gPcLan.aPending[1].bUsed = true;
    gPcLan.aPending[0].To = gPcLan.aPending[1].To = lDeparted;
    gPcLan.aPending[0].uSeq = 0x10001;
    gPcLan.aPending[1].uSeq = 1;
    PcLanHeaderT lAck = { PCLAN_MAGIC, PCLAN_VERSION, PCLAN_ACK, 0, 102, 77, 1, 1 };
    PcLanCoreReceiveOne(reinterpret_cast<const uint8_t*>(&lAck), sizeof(lAck), &lDeparted, 1003, 11);
    if (!Check(!gPcLan.aPending[0].bUsed && gPcLan.aPending[1].bUsed,
               "ACK completion matches the complete online control sequence")) return 1;
    PcLanCoreReleaseAllPending();
    PcLanShutdown();
    return Check(gOnlineReceipts.empty(), "shutdown releases online receipts") ? 0 : 1;
}
