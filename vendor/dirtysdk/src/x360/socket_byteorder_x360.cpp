// DirtySDK X360 network/host byte-order conversions. ARTIST 0x82B2D4A8,
// 0x82B2D4C0, 0x82B2D4E8 and 0x82B2D500 run on big-endian PPC.
#include "types.hpp"

extern "C" u16 SocketHtons(u16 value) { return value; }
extern "C" u32 SocketHtonl(u32 value) { return value; }
extern "C" u16 SocketNtohs(u16 value) { return value; }
extern "C" u32 SocketNtohl(u32 value) { return value; }
