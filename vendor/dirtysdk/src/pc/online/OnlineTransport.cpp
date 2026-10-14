// FLAG PC-platform leaf: BPO1 transport for b5-decomp-server; see its docs/protocol.md.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "OnlineTransport.h"
#include "GameShared/GameClasses/System/PC/CgsPcNetIdentity.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#pragma comment(lib, "bcrypt.lib")

namespace B5Online
{
namespace
{
    const int32_t KI_HEADER = 48;
    const int32_t KI_TAG = 16;
    const int32_t KI_CHUNK = 1136;
    const int32_t KI_MAX_INNER = 20 + PCLAN_MAX_BODY;
    const uint8_t KU_HELLO = 1;
    const uint8_t KU_WELCOME = 2;
    const uint8_t KU_PACKET = 3;
    const uint8_t KU_KEEPALIVE = 4;
    const uint8_t KU_GOODBYE = 5;

    struct Assembly
    {
        bool mbUsed;
        uint32_t muSender;
        uint16_t muMessage;
        uint8_t muCount;
        uint8_t muMask;
        uint16_t muLengths[4];
        uint64_t muSince;
        uint8_t maData[KI_MAX_INNER];
    };
    struct State
    {
        bool mbEnabled;
        SOCKET mSocket;
        PcLanPeerT mServer;
        uint32_t muIdent;
        BCRYPT_ALG_HANDLE mAlgorithm;
        BCRYPT_KEY_HANDLE mKey;
        uint64_t muChallenge;
        uint64_t muSession;
        uint64_t muSequence;
        uint64_t muRxHigh;
        uint64_t muRxLow;
        uint64_t muRxUpper;
        uint64_t muLastHello;
        uint64_t muLastKeepalive;
        uint64_t muLastReceive;
        uint16_t muMessage;
        uint16_t muMaxPlayers;
        uint16_t muTimeoutSeconds;
        bool mbConnectedLogged;
        Assembly maAssemblies[16];
    };
    State gState = {};

    uint16_t Read16(const uint8_t* lpBytes)
    { return static_cast<uint16_t>(lpBytes[0] | (lpBytes[1] << 8)); }
    uint32_t Read32(const uint8_t* lpBytes)
    { return static_cast<uint32_t>(Read16(lpBytes)) | (static_cast<uint32_t>(Read16(lpBytes + 2)) << 16); }
    uint64_t Read64(const uint8_t* lpBytes)
    { return static_cast<uint64_t>(Read32(lpBytes)) | (static_cast<uint64_t>(Read32(lpBytes + 4)) << 32); }
    void Write16(uint8_t* lpBytes, uint16_t luValue)
    { lpBytes[0] = static_cast<uint8_t>(luValue); lpBytes[1] = static_cast<uint8_t>(luValue >> 8); }
    void Write32(uint8_t* lpBytes, uint32_t luValue)
    { Write16(lpBytes, static_cast<uint16_t>(luValue)); Write16(lpBytes + 2, static_cast<uint16_t>(luValue >> 16)); }
    void Write64(uint8_t* lpBytes, uint64_t luValue)
    { Write32(lpBytes, static_cast<uint32_t>(luValue)); Write32(lpBytes + 4, static_cast<uint32_t>(luValue >> 32)); }

    void Log(const char* lpText)
    { CgsDev::Log::WriteToLog(lpText); }

    bool Random(uint8_t* lpBytes, ULONG luSize)
    { return BCryptGenRandom(NULL, lpBytes, luSize, BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0; }

    bool NewChallenge()
    {
        do
        {
            if (!Random(reinterpret_cast<uint8_t*>(&gState.muChallenge), sizeof(gState.muChallenge)))
                return false;
        } while (gState.muChallenge == 0);
        return true;
    }

    bool PreparePersona(uint8_t* lpName)
    {
        wchar_t laWide[32];
        char laUtf8[96];
        if (MultiByteToWideChar(CP_ACP, 0, CgsPcNetIdentityName(), -1, laWide, 32) == 0) return false;
        const int liBytes = WideCharToMultiByte(CP_UTF8, 0, laWide, -1, laUtf8, sizeof(laUtf8), NULL, NULL);
        if (liBytes <= 1) return false;
        int liCopy = (liBytes - 1 > 15) ? 15 : liBytes - 1;
        while ((liCopy > 0) && ((static_cast<uint8_t>(laUtf8[liCopy]) & 0xC0) == 0x80)) --liCopy;
        memset(lpName, 0, 16);
        memcpy(lpName, laUtf8, liCopy);
        return liCopy > 0;
    }

    int HexDigit(char lcValue)
    {
        if ((lcValue >= '0') && (lcValue <= '9')) return lcValue - '0';
        if ((lcValue >= 'a') && (lcValue <= 'f')) return lcValue - 'a' + 10;
        if ((lcValue >= 'A') && (lcValue <= 'F')) return lcValue - 'A' + 10;
        return -1;
    }

    bool OpenKey(const char* lpText)
    {
        if ((lpText == NULL) || (strlen(lpText) != 64)) return false;
        uint8_t laKey[32];
        bool lbAny = false;
        for (int liIndex = 0; liIndex < 32; ++liIndex)
        {
            const int liHigh = HexDigit(lpText[liIndex * 2]);
            const int liLow = HexDigit(lpText[liIndex * 2 + 1]);
            if ((liHigh < 0) || (liLow < 0)) { SecureZeroMemory(laKey, sizeof(laKey)); return false; }
            laKey[liIndex] = static_cast<uint8_t>((liHigh << 4) | liLow);
            lbAny = lbAny || (laKey[liIndex] != 0);
        }
        bool lbOk = lbAny && (BCryptOpenAlgorithmProvider(&gState.mAlgorithm, BCRYPT_AES_ALGORITHM, NULL, 0) >= 0);
        if (lbOk) lbOk = BCryptSetProperty(gState.mAlgorithm, BCRYPT_CHAINING_MODE,
            reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)), sizeof(BCRYPT_CHAIN_MODE_GCM), 0) >= 0;
        if (lbOk) lbOk = BCryptGenerateSymmetricKey(gState.mAlgorithm, &gState.mKey, NULL, 0, laKey, sizeof(laKey), 0) >= 0;
        SecureZeroMemory(laKey, sizeof(laKey));
        return lbOk;
    }

    bool Resolve(const char* lpText)
    {
        char laHost[256];
        if ((lpText == NULL) || (strlen(lpText) >= sizeof(laHost))) return false;
        strcpy_s(laHost, sizeof(laHost), lpText);
        char* lpPort = strrchr(laHost, ':');
        const char* lpService = "27900";
        if (lpPort != NULL)
        {
            *lpPort++ = '\0';
            char* lpEnd = NULL;
            const long liPort = strtol(lpPort, &lpEnd, 10);
            if ((*lpPort == '\0') || (*lpEnd != '\0') || (liPort < 1) || (liPort > 65535)) return false;
            lpService = lpPort;
        }
        if (laHost[0] == '\0') return false;
        ADDRINFOA lHints = {};
        lHints.ai_family = AF_INET;
        lHints.ai_socktype = SOCK_DGRAM;
        lHints.ai_protocol = IPPROTO_UDP;
        ADDRINFOA* lpAddresses = NULL;
        if (getaddrinfo(laHost, lpService, &lHints, &lpAddresses) != 0) return false;
        const sockaddr_in* lpAddress = reinterpret_cast<const sockaddr_in*>(lpAddresses->ai_addr);
        gState.mServer.uAddr = ntohl(lpAddress->sin_addr.s_addr);
        gState.mServer.uPort = ntohs(lpAddress->sin_port);
        freeaddrinfo(lpAddresses);
        return (gState.mServer.uAddr != 0) && (gState.mServer.uAddr != 0xFFFFFFFFu);
    }

    bool AcceptSequence(uint64_t luSequence)
    {
        if (luSequence == 0) return false;
        if (luSequence > gState.muRxHigh)
        {
            const uint64_t luDistance = luSequence - gState.muRxHigh;
            if (luDistance >= 128) { gState.muRxUpper = 0; gState.muRxLow = 1; }
            else if (luDistance >= 64) { gState.muRxUpper = gState.muRxLow << (luDistance - 64); gState.muRxLow = 1; }
            else
            {
                gState.muRxUpper = (gState.muRxUpper << luDistance) | (gState.muRxLow >> (64 - luDistance));
                gState.muRxLow = (gState.muRxLow << luDistance) | 1;
            }
            gState.muRxHigh = luSequence;
            return true;
        }
        const uint64_t luDistance = gState.muRxHigh - luSequence;
        if (luDistance >= 128) return false;
        uint64_t& lWord = (luDistance < 64) ? gState.muRxLow : gState.muRxUpper;
        const uint64_t luBit = uint64_t(1) << (luDistance % 64);
        if ((lWord & luBit) != 0) return false;
        lWord |= luBit;
        return true;
    }

    int32_t SendFrame(uint8_t luKind, uint32_t luTarget, uint16_t luMessage, uint8_t luIndex,
                      uint8_t luCount, const uint8_t* lpBody, int32_t liLength)
    {
        uint8_t laPacket[1200] = {};
        memcpy(laPacket, "BPO1", 4);
        laPacket[4] = 1;
        laPacket[5] = luKind;
        Write16(laPacket + 6, static_cast<uint16_t>(liLength));
        Write32(laPacket + 8, gState.muIdent);
        Write32(laPacket + 12, luTarget);
        Write64(laPacket + 16, (luKind == KU_HELLO) ? gState.muChallenge : gState.muSession);
        Write64(laPacket + 24, (luKind == KU_HELLO) ? 0 : ++gState.muSequence);
        if (!Random(laPacket + 32, 12)) return -3;
        Write16(laPacket + 44, luMessage);
        laPacket[46] = luIndex;
        laPacket[47] = luCount;
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO lAuth;
        BCRYPT_INIT_AUTH_MODE_INFO(lAuth);
        lAuth.pbNonce = laPacket + 32;
        lAuth.cbNonce = 12;
        lAuth.pbAuthData = laPacket;
        lAuth.cbAuthData = KI_HEADER;
        lAuth.pbTag = laPacket + KI_HEADER + liLength;
        lAuth.cbTag = KI_TAG;
        ULONG luWritten = 0;
        // BCrypt accepts a non-null input for empty authenticated messages.
        uint8_t luEmpty = 0;
        PUCHAR lpInput = (liLength > 0) ? const_cast<PUCHAR>(lpBody) : &luEmpty;
        if (BCryptEncrypt(gState.mKey, lpInput, static_cast<ULONG>(liLength), &lAuth, NULL, 0,
            laPacket + KI_HEADER, static_cast<ULONG>(liLength), &luWritten, 0) < 0) return -3;
        sockaddr_in lTo = {};
        lTo.sin_family = AF_INET;
        lTo.sin_addr.s_addr = htonl(gState.mServer.uAddr);
        lTo.sin_port = htons(gState.mServer.uPort);
        const int liSize = KI_HEADER + liLength + KI_TAG;
        return (sendto(gState.mSocket, reinterpret_cast<const char*>(laPacket), liSize, 0,
            reinterpret_cast<const sockaddr*>(&lTo), sizeof(lTo)) == liSize) ? 0 : -3;
    }
}

bool Enabled()
{
    const char* lpServer = getenv("BP_ONLINE_SERVER");
    return (lpServer != NULL) && (*lpServer != '\0');
}

bool Configure(uintptr_t luSocket, uint32_t luIdent, PcLanPeerT* lpSelf)
{
    Shutdown();
    gState.mbEnabled = Enabled();
    if (!gState.mbEnabled) return true;
    gState.mSocket = static_cast<SOCKET>(luSocket);
    gState.muIdent = luIdent;
    gState.muMaxPlayers = 8;
    gState.muTimeoutSeconds = 15;
    if ((luIdent == 0) || !Resolve(getenv("BP_ONLINE_SERVER")) || !OpenKey(getenv("BP_ONLINE_KEY")) || !NewChallenge())
    {
        Log("[net] online configuration failed: check BP_ONLINE_SERVER and the 64-hex-digit BP_ONLINE_KEY\n");
        Shutdown();
        return false;
    }
    // Logical addresses are peer IDs. Only the physical server address reaches sendto.
    lpSelf->uAddr = luIdent;
    lpSelf->uPort = 1;
    lpSelf->uPad = 0;
    Log("[net] online transport connecting through server (LAN discovery disabled)\n");
    Update();
    return true;
}

void Shutdown()
{
    if (gState.mbEnabled && (gState.muSession != 0) && (gState.mKey != NULL))
        SendFrame(KU_GOODBYE, 0, 0, 0, 1, NULL, 0);
    if (gState.mKey != NULL) BCryptDestroyKey(gState.mKey);
    if (gState.mAlgorithm != NULL) BCryptCloseAlgorithmProvider(gState.mAlgorithm, 0);
    SecureZeroMemory(&gState, sizeof(gState));
}

void Update()
{
    if (!gState.mbEnabled || (gState.mKey == NULL)) return;
    const uint64_t luNow = GetTickCount64();
    if ((gState.muSession != 0) && (luNow - gState.muLastReceive >= uint64_t(gState.muTimeoutSeconds) * 1000))
    {
        Log("[net] online server timed out; reconnecting\n");
        gState.muSession = 0;
        gState.muRxHigh = gState.muRxLow = gState.muRxUpper = 0;
        memset(gState.maAssemblies, 0, sizeof(gState.maAssemblies));
        if (!NewChallenge()) return;
    }
    if (gState.muSession == 0)
    {
        if ((gState.muLastHello == 0) || (luNow - gState.muLastHello >= 1000))
        {
            uint8_t laHello[28] = {};
            Write64(laHello, CgsPcNetIdentityXuid());
            if (!PreparePersona(laHello + 8)) return;
            Write16(laHello + 24, 8);
            Write16(laHello + 26, PCLAN_VERSION);
            SendFrame(KU_HELLO, 0, 0, 0, 1, laHello, sizeof(laHello));
            gState.muLastHello = luNow;
        }
    }
    else if ((gState.muLastKeepalive == 0) || (luNow - gState.muLastKeepalive >= 3000))
    {
        SendFrame(KU_KEEPALIVE, 0, 0, 0, 1, NULL, 0);
        gState.muLastKeepalive = luNow;
    }
}

uint16_t MaxPlayers() { return gState.muMaxPlayers ? gState.muMaxPlayers : 8; }

int32_t Send(uint32_t luTarget, const uint8_t* lpPacket, int32_t liLength)
{
    if (!gState.mbEnabled || (gState.muSession == 0)) return -1;
    if ((liLength < 20) || (liLength > KI_MAX_INNER)) return -2;
    const uint16_t luMessage = ++gState.muMessage;
    const uint8_t luCount = static_cast<uint8_t>((liLength + KI_CHUNK - 1) / KI_CHUNK);
    for (uint8_t luIndex = 0; luIndex < luCount; ++luIndex)
    {
        const int32_t liOffset = luIndex * KI_CHUNK;
        const int32_t liSize = ((liLength - liOffset) < KI_CHUNK) ? (liLength - liOffset) : KI_CHUNK;
        if (SendFrame(KU_PACKET, luTarget, luMessage, luIndex, luCount, lpPacket + liOffset, liSize) != 0) return -3;
    }
    return 0;
}

int32_t Receive(const PcLanPeerT* lpFrom, const uint8_t* lpPacket, int32_t liLength,
                uint8_t* lpInner, int32_t liCapacity, PcLanPeerT* lpPeer)
{
    if (!gState.mbEnabled || (lpFrom->uAddr != gState.mServer.uAddr) || (lpFrom->uPort != gState.mServer.uPort)
        || (liLength < KI_HEADER + KI_TAG) || (liLength > 1200) || (memcmp(lpPacket, "BPO1", 4) != 0)
        || (lpPacket[4] != 1) || (Read32(lpPacket + 12) != gState.muIdent)) return 0;
    const int32_t liSize = Read16(lpPacket + 6);
    const uint8_t luKind = lpPacket[5];
    const uint8_t luIndex = lpPacket[46];
    const uint8_t luCount = lpPacket[47];
    if ((liSize > KI_CHUNK) || (liLength != KI_HEADER + liSize + KI_TAG) || (luCount == 0) || (luCount > 4)
        || (luIndex >= luCount) || ((luIndex + 1 < luCount) && (liSize != KI_CHUNK))
        || ((luKind != KU_PACKET) && ((luCount != 1) || (luIndex != 0)))) return 0;
    uint8_t laPlain[KI_CHUNK];
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO lAuth;
    BCRYPT_INIT_AUTH_MODE_INFO(lAuth);
    lAuth.pbNonce = const_cast<PUCHAR>(lpPacket + 32);
    lAuth.cbNonce = 12;
    lAuth.pbAuthData = const_cast<PUCHAR>(lpPacket);
    lAuth.cbAuthData = KI_HEADER;
    lAuth.pbTag = const_cast<PUCHAR>(lpPacket + KI_HEADER + liSize);
    lAuth.cbTag = KI_TAG;
    ULONG luWritten = 0;
    if (BCryptDecrypt(gState.mKey, const_cast<PUCHAR>(lpPacket + KI_HEADER), static_cast<ULONG>(liSize),
        &lAuth, NULL, 0, laPlain, sizeof(laPlain), &luWritten, 0) < 0) return 0;
    const uint64_t luSession = Read64(lpPacket + 16);
    if (luKind == KU_WELCOME)
    {
        if ((Read32(lpPacket + 8) != 0) || (liSize != 12) || (Read64(laPlain) != gState.muChallenge)
            || (luSession == 0) || (Read16(laPlain + 8) < 2) || (Read16(laPlain + 8) > 8)
            || (Read16(laPlain + 10) < 10) || (Read16(laPlain + 10) > 300)) return 0;
        if ((gState.muSession != 0) && (luSession != gState.muSession)) return 0;
        gState.muSession = luSession;
        gState.muMaxPlayers = Read16(laPlain + 8);
        gState.muTimeoutSeconds = Read16(laPlain + 10);
        if (!AcceptSequence(Read64(lpPacket + 24))) return 0;
        gState.muLastReceive = GetTickCount64();
        SendFrame(KU_KEEPALIVE, 0, 0, 0, 1, NULL, 0);
        if (!gState.mbConnectedLogged)
        {
            Log("[net] online server connected: authenticated encrypted relay, protocol=1\n");
            gState.mbConnectedLogged = true;
        }
        return 0;
    }
    if ((gState.muSession == 0) || (gState.muSession != luSession) || !AcceptSequence(Read64(lpPacket + 24))) return 0;
    gState.muLastReceive = GetTickCount64();
    if (luKind == KU_KEEPALIVE) return 0;
    if ((luKind != KU_PACKET) || (Read32(lpPacket + 8) == 0)) return 0;
    const uint32_t luSender = Read32(lpPacket + 8);
    const uint16_t luMessage = Read16(lpPacket + 44);
    lpPeer->uAddr = luSender;
    lpPeer->uPort = 1;
    lpPeer->uPad = 0;
    if (luCount == 1)
    {
        if (liSize > liCapacity) return 0;
        memcpy(lpInner, laPlain, liSize);
        return liSize;
    }
    const uint64_t luNow = GetTickCount64();
    Assembly* lpAssembly = NULL;
    Assembly* lpFree = NULL;
    for (int liSlot = 0; liSlot < 16; ++liSlot)
    {
        Assembly* lpEntry = &gState.maAssemblies[liSlot];
        if (lpEntry->mbUsed && (luNow - lpEntry->muSince >= 2000)) lpEntry->mbUsed = false;
        if (lpEntry->mbUsed && (lpEntry->muSender == luSender) && (lpEntry->muMessage == luMessage)) lpAssembly = lpEntry;
        if (!lpEntry->mbUsed) lpFree = lpEntry;
    }
    if (lpAssembly == NULL)
    {
        if (lpFree == NULL) return 0;
        lpAssembly = lpFree;
        memset(lpAssembly, 0, sizeof(*lpAssembly));
        lpAssembly->mbUsed = true;
        lpAssembly->muSender = luSender;
        lpAssembly->muMessage = luMessage;
        lpAssembly->muCount = luCount;
        lpAssembly->muSince = luNow;
    }
    const int32_t liOffset = luIndex * KI_CHUNK;
    if ((lpAssembly->muCount != luCount) || ((lpAssembly->muMask & (1u << luIndex)) != 0)
        || (liOffset + liSize > KI_MAX_INNER)) return 0;
    memcpy(lpAssembly->maData + liOffset, laPlain, liSize);
    lpAssembly->muLengths[luIndex] = static_cast<uint16_t>(liSize);
    lpAssembly->muMask |= static_cast<uint8_t>(1u << luIndex);
    if (lpAssembly->muMask != ((1u << luCount) - 1)) return 0;
    const int32_t liTotal = (luCount - 1) * KI_CHUNK + lpAssembly->muLengths[luCount - 1];
    lpAssembly->mbUsed = false;
    if (liTotal > liCapacity) return 0;
    memcpy(lpInner, lpAssembly->maData, liTotal);
    return liTotal;
}
}
