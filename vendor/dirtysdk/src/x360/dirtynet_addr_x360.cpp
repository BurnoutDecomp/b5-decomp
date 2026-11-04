// DirtySDK 5.5.3 X360 native address operations. ARTIST 82B2D068,
// 82B2D238, 82B2D3C8 and 82B2D528.
#include "types.hpp"
#include "dirtylib.h"
#include <xtl.h>
#include <string.h>

// ARTIST82B2D340: octets accumulate as bytes and the fourth component
// accepts any terminator; malformed first three separators clear the address.
// Public XDK sockaddr is only two-byte aligned. ARTIST addresses are
// assembled and stored as individual network-order bytes.
static u32 SocketReadAddress(const void* value)
{
    const volatile u8* p = static_cast<const volatile u8*>(value);
    return (u32(p[4]) << 24) | (u32(p[5]) << 16) | (u32(p[6]) << 8) | p[7];
}
static void SocketWriteAddress(void* value, u32 address)
{
    volatile u8* p = static_cast<volatile u8*>(value);
    p[7] = static_cast<u8>(address);
    p[6] = static_cast<u8>(address >> 8);
    p[5] = static_cast<u8>(address >> 16);
    p[4] = static_cast<u8>(address >> 24);
}

extern "C" s32 SockaddrInSetAddrText(sockaddr* address, const char* text)
{
    u8* octets = reinterpret_cast<u8*>(address) + 4;
    for (s32 i = 0; i < 4; ++i)
    {
        octets[i] = 0;
        while (static_cast<signed char>(*text) >= '0' && static_cast<signed char>(*text) <= '9')
        {
            octets[i] = static_cast<u8>(octets[i] * 10 + (*text & 15));
            ++text;
        }
        if (i < 3 && *text != '.')
        {
            octets[0] = octets[1] = octets[2] = octets[3] = 0;
            return -1;
        }
        ++text;
    }
    return 0;
}

// ARTIST82B2D580. A failed parse returns zero.
extern "C" u32 SocketInTextGetAddr(const char* text)
{
    sockaddr_in address;
    if (SockaddrInSetAddrText(reinterpret_cast<sockaddr*>(&address), text) != 0)
        return 0;
    return SocketReadAddress(&address);
}

extern "C" char* SockaddrInGetAddrText(const sockaddr* address, char* text, s32 length)
{
    if (length <= 0) return 0;
    if (length < 16) { *text = 0; return 0; }
    const u8* bytes = reinterpret_cast<const u8*>(address) + 4;
    char* out = text;
    for (s32 i = 0; i < 4; ++i)
    {
        u32 value = bytes[i];
        if (value > 99)
        {
            *out++ = static_cast<char>('0' + value / 100);
            value %= 100;
            *out++ = static_cast<char>('0' + value / 10);
            value %= 10;
        }
        if (value > 9)
        {
            *out++ = static_cast<char>('0' + value / 10);
            value %= 10;
        }
        *out++ = static_cast<char>('0' + value);
        if (i < 3) *out++ = '.';
    }
    *out = 0;
    return text;
}

extern "C" const char* SocketInAddrGetText(u32 address)
{
    static char text[20]; // ARTIST 8324EBC4
    sockaddr_in sock;
    SocketWriteAddress(&sock, address);
    SockaddrInGetAddrText(reinterpret_cast<sockaddr*>(&sock), text, sizeof(text));
    return text;
}

extern "C" s32 SocketHost(sockaddr* host, s32 hostLength, const sockaddr* dest, s32 destLength)
{
    if (hostLength != destLength)
    {
        NetPrintfCode("sockethost: len mismatch\n");
        return -1;
    }
    if (dest->sa_family != AF_INET)
    {
        memset(host, 0, hostLength);
        return -3;
    }
    memcpy(host, dest, hostLength);
    SocketWriteAddress(host, 0);
    XNADDR address;
    if ((XNetGetTitleXnAddr(&address) & 0x60) == 0) return -2;
    const volatile u8* source = reinterpret_cast<const volatile u8*>(&address.ina);
    const u32 ip = (u32(source[0]) << 24) | (u32(source[1]) << 16) | (u32(source[2]) << 8) | source[3];
    SocketWriteAddress(host, ip);
    return 0;
}

extern "C" u32 SocketGetLocalAddr()
{
    sockaddr_in destination;
    sockaddr_in host;
    memset(&destination, 0, sizeof(destination));
    memset(&host, 0, sizeof(host));
    destination.sin_family = AF_INET;
    volatile u8* bytes = reinterpret_cast<volatile u8*>(&destination);
    bytes[2] = 0; bytes[3] = 79;
    SocketWriteAddress(&destination, 0xC0A80101);
    SocketHost(reinterpret_cast<sockaddr*>(&host), sizeof(host),
               reinterpret_cast<const sockaddr*>(&destination), sizeof(destination));
    return SocketReadAddress(&host);
}