// Feed authenticated server datagrams through the production decrypt/reassembly
// path. Reordering stays enabled while obsolete connections are fenced out.
#include "../vendor/dirtysdk/src/pc/online/OnlineTransport.cpp"
#include "../vendor/dirtysdk/src/pc/lan/pclan_core.cpp"
#include <vector>
#include <algorithm>

static std::vector<uint8_t> gaLastInner;
static bool gbDispatchCore = false;
static bool gbOldMember = false;
static bool gbBrowsedWithoutGhost = false;
static unsigned guDepartures = 0;

static void OnBye(void*, const PcLanPeerT*, const PcLanHeaderT*, const uint8_t*)
{ gbOldMember = false; ++guDepartures; }
static void OnDiscover(void*, const PcLanPeerT*, const PcLanHeaderT*, const uint8_t*)
{ gbBrowsedWithoutGhost = !gbOldMember; }

const char* CgsPcNetIdentityName() { return "EpochTest"; }
u64 CgsPcNetIdentityXuid() { return 101; }
u32 CgsPcNetIdentityLobbyIdent() { return 101; }
bool CgsPcNetLanEnabled() { return true; }
namespace CgsDev { namespace Log { void WriteToLog(const char*) {} } }

static void ResetTransport()
{
    using namespace B5Online;
    gState.muSession = 0;
    Shutdown();
    gState.mbEnabled = true;
    gState.mSocket = INVALID_SOCKET;
    gState.mServer = { 0x7f000001, 27900, 0 };
    gState.muIdent = 101;
    gState.muTimeoutSeconds = 300;
    if (!OpenKey("abababababababababababababababababababababababababababababababab")) abort();
    gState.muSession = 77;
}

static std::vector<uint8_t> Payload(uint64_t luEpoch, uint8_t luKind, size_t luBody = 0, uint32_t luSender = 102)
{
    std::vector<uint8_t> laPayload(8 + 20 + luBody, 0x55);
    B5Online::Write64(laPayload.data(), luEpoch);
    const PcLanHeaderT lHeader = { PCLAN_MAGIC, PCLAN_VERSION, luKind,
        static_cast<uint16_t>(luBody), luSender, 77, 1, 0 };
    memcpy(laPayload.data() + 8, &lHeader, sizeof(lHeader));
    return laPayload;
}

static int32_t Deliver(const std::vector<uint8_t>& lPlain, uint64_t luSequence,
                       uint16_t luMessage = 1, uint8_t luIndex = 0, uint8_t luCount = 1,
                       uint32_t luSender = 102)
{
    using namespace B5Online;
    std::vector<uint8_t> laWire(48 + lPlain.size() + 16, 0);
    memcpy(laWire.data(), "BPO1", 4);
    laWire[4] = 2;
    laWire[5] = KU_PACKET;
    Write16(laWire.data() + 6, static_cast<uint16_t>(lPlain.size()));
    Write32(laWire.data() + 8, luSender);
    Write32(laWire.data() + 12, gState.muIdent);
    Write64(laWire.data() + 16, gState.muSession);
    Write64(laWire.data() + 24, luSequence);
    if (!Random(laWire.data() + 32, 12)) abort();
    Write16(laWire.data() + 44, luMessage);
    laWire[46] = luIndex;
    laWire[47] = luCount;
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO lAuth;
    BCRYPT_INIT_AUTH_MODE_INFO(lAuth);
    lAuth.pbNonce = laWire.data() + 32;
    lAuth.cbNonce = 12;
    lAuth.pbAuthData = laWire.data();
    lAuth.cbAuthData = 48;
    lAuth.pbTag = laWire.data() + 48 + lPlain.size();
    lAuth.cbTag = 16;
    ULONG luWritten = 0;
    if (BCryptEncrypt(gState.mKey, const_cast<PUCHAR>(lPlain.data()), static_cast<ULONG>(lPlain.size()),
        &lAuth, NULL, 0, laWire.data() + 48, static_cast<ULONG>(lPlain.size()), &luWritten, 0) < 0) abort();
    uint8_t laInner[20 + PCLAN_MAX_BODY];
    PcLanPeerT lPeer;
    uint64_t luEpoch = 0;
    bool lbRestarted = false;
    const int32_t liSize = Receive(&gState.mServer, laWire.data(), static_cast<int32_t>(laWire.size()),
                                   laInner, sizeof(laInner), &lPeer, &luEpoch, &lbRestarted);
    if (liSize > 0) gaLastInner.assign(laInner, laInner + liSize);
    if (liSize > 0 && gbDispatchCore)
        PcLanCoreReceiveOne(laInner, liSize, &lPeer, GetTickCount64(), luEpoch, lbRestarted);
    return liSize;
}

static bool Check(bool lbPass, const char* lpName)
{
    printf("%s: %s\n", lbPass ? "PASS" : "FAIL", lpName);
    return lbPass;
}

int main()
{
    _putenv_s("BP_ONLINE_SERVER", "epoch-test.invalid");
    ResetTransport();
    if (Deliver(Payload(11, PCLAN_JOIN_REQ), 10) != 20 ||
        Deliver(Payload(12, PCLAN_JOIN_REQ), 20) != 20) return 1;
    if (!Check(Deliver(Payload(11, PCLAN_BYE), 11) == 0,
               "old BYE cannot close a newer connection")) return 1;
    if (!Check(Deliver(Payload(12, PCLAN_DATA), 21) == 20,
               "new connection remains usable after a delayed old BYE")) return 1;

    ResetTransport();
    if (Deliver(Payload(11, PCLAN_BYE), 10) != 20) return 1;
    if (!Check(Deliver(Payload(11, PCLAN_JOIN_REQ), 11) == 0 &&
               Deliver(Payload(11, PCLAN_ACK), 12) == 0 && Deliver(Payload(11, PCLAN_DATA), 13) == 0,
               "retired connections cannot dispatch unseen controls, ACKs or data")) return 1;
    if (!Check(Deliver(Payload(12, PCLAN_JOIN_REQ), 20) == 20,
               "a later authenticated connection can join after departure")) return 1;

    ResetTransport();
    const auto laOld = Payload(11, PCLAN_DATA, 1500);
    const std::vector<uint8_t> laFirst(laOld.begin(), laOld.begin() + 1136);
    const std::vector<uint8_t> laLast(laOld.begin() + 1136, laOld.end());
    if (Deliver(laFirst, 10, 7, 0, 2) != 0 || Deliver(Payload(12, PCLAN_JOIN_REQ), 20) != 20) return 1;
    if (!Check(Deliver(laLast, 11, 7, 1, 2) == 0,
               "old fragmented packets cannot complete into a new connection")) return 1;

    ResetTransport();
    auto laNew = Payload(11, PCLAN_DATA, 1500);
    std::fill(laNew.begin() + 28, laNew.end(), 0x66);
    const std::vector<uint8_t> laNewFirst(laNew.begin(), laNew.begin() + 1136);
    const std::vector<uint8_t> laNewLast(laNew.begin() + 1136, laNew.end());
    if (Deliver(laFirst, 10, 7, 0, 2) != 0 || Deliver(laNewLast, 65547, 7, 1, 2) != 0) return 1;
    if (!Check(Deliver(laNewFirst, 65546, 7, 0, 2) == 1520 &&
               std::all_of(gaLastInner.begin() + 20, gaLastInner.end(), [](uint8_t luByte) { return luByte == 0x66; }),
               "wrapped fragment IDs cannot combine old and new message bytes")) return 1;

    ResetTransport();
    if (Deliver(Payload(11, PCLAN_DATA), 20) != 20) return 1;
    if (!Check(Deliver(Payload(11, PCLAN_JOIN_REQ), 19) == 20,
               "ordinary reordering within the current connection remains valid")) return 1;
    if (!Check(Deliver(Payload(5, PCLAN_JOIN_REQ), 30) == 20 &&
               Deliver(Payload(11, PCLAN_DATA), 21) == 0,
               "relay sequence orders epochs independently of random epoch values")) return 1;

    ResetTransport();
    auto laInvalidBye = Payload(11, PCLAN_BYE);
    B5Online::Write32(laInvalidBye.data() + 16, 103);
    if (!Check(Deliver(laInvalidBye, 10) == 0 && Deliver(Payload(11, PCLAN_JOIN_REQ), 11) == 20,
               "malformed inner identities cannot retire a connection")) return 1;

    ResetTransport();
    for (uint32_t luSender = 102; luSender < 359; ++luSender)
        if (Deliver(Payload(11, PCLAN_ADVERT, 0, luSender), luSender, 1, 0, 1, luSender) != 20) return 1;
    if (!Check(Deliver(Payload(11, PCLAN_LINK_HELLO, 0, 359), 359, 1, 0, 1, 359) == 20,
               "directory advertisers cannot exhaust room-peer epoch capacity at 256")) return 1;

    ResetTransport();
    if (Deliver(Payload(11, PCLAN_PLAY), 10) != 20) return 1;
    gPcLan.uSelfIdent = 101;
    gPcLan.Self.uPort = 1;
    gPcLan.aPending[0].bUsed = true;
    gPcLan.aPending[0].To = { 102, 1, 0 };
    gOnlineReceipts[std::make_tuple(102, 1, 102, uint64_t(11), 1)] = GetTickCount64();
    PcLanRegister(PCLAN_BYE, OnBye, NULL);
    PcLanRegister(PCLAN_DISCOVER, OnDiscover, NULL);
    gbOldMember = gbDispatchCore = true;
    if (!Check(Deliver(Payload(12, PCLAN_DISCOVER), 20) == 20 && gbBrowsedWithoutGhost &&
               guDepartures == 1 && !gPcLan.aPending[0].bUsed && gOnlineReceipts.size() == 2,
               "reconnect-and-browse cleans old membership and pending sends before discovery, retaining receipts")) return 1;
    if (!Check(Deliver(Payload(11, PCLAN_BYE), 11) == 0 &&
               Deliver(Payload(12, PCLAN_JOIN_REQ), 21) == 20 && guDepartures == 1,
               "delayed old BYE cannot clean up the new connection a second time")) return 1;
    gbDispatchCore = false;
    B5Online::gState.muSession = 0;
    B5Online::Shutdown();
    return 0;
}
