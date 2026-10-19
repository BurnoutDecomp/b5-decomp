// Native Windows integration fixture: exercise the production BCrypt/BPO1 transport
// against a running Rust server, including a maximum-size fragmented game packet.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vendor/dirtysdk/src/pc/online/OnlineTransport.h"
#include "GameShared/GameClasses/System/PC/CgsPcNetIdentity.h"

static uint32_t guIdent;
const char* CgsPcNetIdentityName() { return (guIdent == 101) ? "Host Andr\xe9" : "TransportGuest"; }
u64 CgsPcNetIdentityXuid() { return guIdent; }
u32 CgsPcNetIdentityLobbyIdent() { return guIdent; }
bool CgsPcNetLanEnabled() { return true; }
namespace CgsDev { namespace Log { void WriteToLog(const char* lpText) { fputs(lpText, stdout); fflush(stdout); } } }

static int SendPacket(uint8_t luKind, uint32_t luTarget, const uint8_t* lpBody, uint16_t luLength)
{
    uint8_t laBytes[20 + PCLAN_MAX_BODY] = {};
    PcLanHeaderT lHeader = { PCLAN_MAGIC, PCLAN_VERSION, luKind, luLength, guIdent, 77, 0, 0 };
    memcpy(laBytes, &lHeader, sizeof(lHeader));
    memcpy(laBytes + 20, lpBody, luLength);
    return B5Online::Send(luTarget, laBytes, 20 + luLength);
}

int main(int liArgc, char** lpArgv)
{
    if (liArgc != 2) return 2;
    const bool lbHost = strcmp(lpArgv[1], "host") == 0;
    guIdent = lbHost ? 101 : 102;
    WSADATA lWsa;
    if (WSAStartup(MAKEWORD(2, 2), &lWsa) != 0) return 2;
    const SOCKET lSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in lBind = {}; lBind.sin_family = AF_INET; lBind.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(lSocket, reinterpret_cast<sockaddr*>(&lBind), sizeof(lBind)) != 0) return 2;
    u_long luNonBlocking = 1; ioctlsocket(lSocket, FIONBIO, &luNonBlocking);
    PcLanPeerT lSelf = {};
    if (!B5Online::Configure(static_cast<uintptr_t>(lSocket), guIdent, &lSelf)) return 2;
    uint8_t laPayload[PCLAN_MAX_BODY];
    for (int li = 0; li < PCLAN_MAX_BODY; ++li) laPayload[li] = static_cast<uint8_t>(li);
    bool lbDiscovered = false;
    bool lbPassed = false;
    uint64_t luLastSend = 0;
    const uint64_t luStart = GetTickCount64();
    while (GetTickCount64() - luStart < 15000 && !lbPassed)
    {
        B5Online::Update();
        const uint64_t luNow = GetTickCount64();
        if (luNow - luLastSend >= 700)
        {
            if (lbHost)
            {
                const char* lpRecord = "IDENT=77\nNAME=TransportProbe\nCOUNT=1\nMAXSIZE=8\nOPID0=101\n\0";
                uint8_t laBody[256] = {}; memcpy(laBody + 4, lpRecord, strlen(lpRecord) + 1);
                SendPacket(PCLAN_PLAY, 0, laBody, static_cast<uint16_t>(4 + strlen(lpRecord) + 1));
            }
            else if (!lbDiscovered) SendPacket(PCLAN_DISCOVER, 0, laPayload, 0);
            else SendPacket(PCLAN_DATA, 101, laPayload, PCLAN_MAX_BODY);
            luLastSend = luNow;
        }
        for (;;)
        {
            uint8_t laWire[1201]; sockaddr_in lFrom = {}; int liFromSize = sizeof(lFrom);
            const int liSize = recvfrom(lSocket, reinterpret_cast<char*>(laWire), sizeof(laWire), 0,
                reinterpret_cast<sockaddr*>(&lFrom), &liFromSize);
            if (liSize <= 0) break;
            PcLanPeerT lFromPeer = { ntohl(lFrom.sin_addr.s_addr), ntohs(lFrom.sin_port), 0 };
            PcLanPeerT lPeer; uint8_t laInner[20 + PCLAN_MAX_BODY];
            const int liInner = B5Online::Receive(&lFromPeer, laWire, liSize, laInner, sizeof(laInner), &lPeer);
            if (liInner < 20) continue;
            if (lbHost && laInner[5] == PCLAN_DISCOVER)
            {
                const char* lpRecord = "IDENT=77\nNAME=TransportProbe\nCOUNT=1\nMAXSIZE=8\nOPID0=101\n";
                uint8_t laAdvert[256] = {};
                const uint64_t luXuid = guIdent;
                memcpy(laAdvert, &luXuid, 8); memcpy(laAdvert + 8, &guIdent, 4);
                memcpy(laAdvert + 12, lpRecord, strlen(lpRecord) + 1);
                SendPacket(PCLAN_ADVERT, lPeer.uAddr, laAdvert, static_cast<uint16_t>(12 + strlen(lpRecord) + 1));
            }
            if (laInner[5] == PCLAN_ADVERT && lPeer.uAddr == 101) lbDiscovered = true;
            if (laInner[5] == PCLAN_DATA && liInner == sizeof(laInner) && memcmp(laInner + 20, laPayload, sizeof(laPayload)) == 0)
            {
                if (lbHost) SendPacket(PCLAN_DATA, lPeer.uAddr, laPayload, sizeof(laPayload));
                lbPassed = true;
            }
        }
        Sleep(5);
    }
    B5Online::Shutdown(); closesocket(lSocket); WSACleanup();
    printf("%s: %s discovery and 4096-byte encrypted relay\n", lbHost ? "host" : "guest", lbPassed ? "PASS" : "FAIL");
    return lbPassed ? 0 : 1;
}
