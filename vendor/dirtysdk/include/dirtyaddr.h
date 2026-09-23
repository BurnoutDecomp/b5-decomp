#ifndef DIRTYSDK_DIRTYADDR_H
#define DIRTYSDK_DIRTYADDR_H

#include "types.hpp"

// DirtySDK 5.5.3 - core/include/dirtyaddr.h
// A DirtyAddrT is the printable machine address a lobby record carries for a player
// (the "MADDR" field). On this platform it is "$" followed by the 16 lowercase hex
// digits of the 64-bit host address (the player's XUID).
// Bodies: ../src/dirtyaddr.cpp.

struct DirtyAddrT
{
    char strMachineAddr[64];
};

#ifdef __cplusplus
extern "C" {
#endif

// Decode pAddr into the 8-byte host address at pOutput (iBufLen must be >= 8).
// Returns 1 on success, 0 when the output buffer is too small.
u32 DirtyAddrToHostAddr(void* pOutput, s32 iBufLen, const DirtyAddrT* pAddr);

// Encode the 8-byte host address at pInput (4-byte aligned) into pAddr.
// Returns 1 on success, 0 for a NULL or misaligned input.
u32 DirtyAddrFromHostAddr(DirtyAddrT* pAddr, const void* pInput);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_DIRTYADDR_H
